#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>
#include <boost/beast/core/string_type.hpp>


namespace api {
    using boost::beast::string_view;

    constexpr string_view MAPS        = "/api/v1/maps";
    constexpr string_view MAPS_SLASH  = "/api/v1/maps/";
    constexpr string_view MAPS_PREFIX = "/api/v1/maps/";
    constexpr string_view API_PREFIX  = "/api/v1/";
}

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game)
        : game_{game} {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        HandleRequest(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    }

private:
    model::Game& game_;

    json::array SerializeRoads(const model::Map& map) {
        json::array roads_json;

        for (const auto& road : map.GetRoads()) {
            json::object road_obj;
            road_obj["x0"] = road.GetStart().x;
            road_obj["y0"] = road.GetStart().y;

            if (road.IsHorizontal()) {
                road_obj["x1"] = road.GetEnd().x;
            } else {
                road_obj["y1"] = road.GetEnd().y;
            }

            roads_json.push_back(std::move(road_obj));
        }

        return roads_json;
    }

    json::array SerializeBuildings(const model::Map& map) {
        json::array buildings_json;

        for (const auto& building : map.GetBuildings()) {
            json::object building_obj;
            building_obj["x"] = building.GetBounds().position.x;
            building_obj["y"] = building.GetBounds().position.y;
            building_obj["w"] = building.GetBounds().size.width;
            building_obj["h"] = building.GetBounds().size.height;
            buildings_json.push_back(std::move(building_obj));
        }

        return buildings_json;
    }

    json::array SerializeOffices(const model::Map& map) {
        json::array offices_json;

        for (const auto& office : map.GetOffices()) {
            json::object office_obj;
            office_obj["id"] = *office.GetId();
            office_obj["x"] = office.GetPosition().x;
            office_obj["y"] = office.GetPosition().y;
            office_obj["offsetX"] = office.GetOffset().dx;
            office_obj["offsetY"] = office.GetOffset().dy;
            offices_json.push_back(std::move(office_obj));
        }

        return offices_json;
    }

    
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        if (req.method() != http::verb::get) {
            return SendMethodNotAllowed(req, std::forward<Send>(send));
        }
        
        const auto target = req.target(); // beast::string_view

        if (target == api::MAPS || target == api::MAPS_SLASH) {
            return SendMapsList(req, std::forward<Send>(send));
        }

        if (target.starts_with(api::MAPS_PREFIX)) {
            auto id = target.substr(api::MAPS_SLASH.size());
            if (!id.empty() && id.back() == '/') {
                id.remove_suffix(1);
            }
            return SendMapInfo(req, std::string(id), std::forward<Send>(send));
        }

        if (target.starts_with(api::API_PREFIX)) {
            return SendBadRequest(req, "Bad request", std::forward<Send>(send));
        }

            return SendNotFound(req, std::string(target), std::forward<Send>(send));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void SendMapsList(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        json::array maps_json;
        
        for (const auto& map : game_.GetMaps()) {
            json::object map_obj;
            map_obj["id"] = *map.GetId();
            map_obj["name"] = map.GetName();
            maps_json.push_back(std::move(map_obj));
        }
        
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(maps_json);
        response.prepare_payload();
        
        send(std::move(response));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void SendMapInfo(const http::request<Body, http::basic_fields<Allocator>>& req, 
                     std::string map_id, Send&& send) {
        const auto* map = game_.FindMap(model::Map::Id{std::move(map_id)});
        
        if (!map) {
            json::object error;
            error["code"] = "mapNotFound";
            error["message"] = "Map not found";
            
            StringResponse response(http::status::not_found, req.version());
            response.set(http::field::content_type, "application/json");
            response.body() = json::serialize(error);
            response.prepare_payload();
            
            return send(std::move(response));
        }
        
        json::object map_json;
        map_json["id"] = *map->GetId();
        map_json["name"] = map->GetName();

        map_json["roads"] = SerializeRoads(*map);
        map_json["buildings"] = SerializeBuildings(*map);
        map_json["offices"] = SerializeOffices(*map);
        
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(map_json);
        response.prepare_payload();
        
        send(std::move(response));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void SendBadRequest(const http::request<Body, http::basic_fields<Allocator>>& req,
                        std::string_view message, Send&& send) {
        json::object error;
        error["code"] = "badRequest";
        error["message"] = std::string(message);
        
        StringResponse response(http::status::bad_request, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(error);
        response.prepare_payload();
        
        send(std::move(response));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void SendNotFound(const http::request<Body, http::basic_fields<Allocator>>& req,
                      std::string_view target, Send&& send) {
        json::object error;
        error["code"] = "pageNotFound";
        error["message"] = "Page not found: " + std::string(target);
        
        StringResponse response(http::status::not_found, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(error);
        response.prepare_payload();
        
        send(std::move(response));
    }
    
    template <typename Body, typename Allocator, typename Send>
    void SendMethodNotAllowed(const http::request<Body, http::basic_fields<Allocator>>& req,
                              Send&& send) {
        json::object error;
        error["code"] = "invalidMethod";
        error["message"] = "Invalid method";
        
        StringResponse response(http::status::method_not_allowed, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(error);
        response.prepare_payload();
        
        send(std::move(response));
    }
};

}  // namespace http_handler
