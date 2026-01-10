#pragma once
#include "http_server.h"
#include "model.h"
#include <boost/json.hpp>

#define BOOST_NO_CXX17_HDR_STRING_VIEW

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
        // Обработать запрос request и отправить ответ, используя send
        HandleRequest(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    }

private:
    model::Game& game_;
    
    // Основной метод обработки запроса
    template <typename Body, typename Allocator, typename Send>
    void HandleRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        // Проверяем, что это GET-запрос
        if (req.method() != http::verb::get) {
            return SendMethodNotAllowed(req, std::forward<Send>(send));
        }
        
        const auto target = req.target();
        
        // Обработка /api/v1/maps
        if (target == "/api/v1/maps" || target == "/api/v1/maps/") {
            return SendMapsList(req, std::forward<Send>(send));
        }
        
        // Обработка /api/v1/maps/{id}
        if (target.starts_with("/api/v1/maps/")) {
            // Извлекаем id из пути
            auto id = target.substr(std::string_view("/api/v1/maps/").size());
            if (!id.empty() && id.back() == '/') {
                id.remove_suffix(1); // Убираем завершающий слэш
            }
            return SendMapInfo(req, std::string(id), std::forward<Send>(send));
        }
        
        // Если путь начинается с /api/ но не соответствует известным маршрутам
        if (target.starts_with("/api/")) {
            return SendBadRequest(req, "Bad request", std::forward<Send>(send));
        }
        
        // Для всех остальных запросов - 404 Not Found
        return SendNotFound(req, std::string(target.data(), target.size()), std::forward<Send>(send));
    }
    
    // Отправка списка карт
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
    
    // Отправка информации о конкретной карте
    template <typename Body, typename Allocator, typename Send>
    void SendMapInfo(const http::request<Body, http::basic_fields<Allocator>>& req, 
                     std::string map_id, Send&& send) {
        // Ищем карту по id
        const auto* map = game_.FindMap(model::Map::Id{std::move(map_id)});
        
        if (!map) {
            // Карта не найдена
            json::object error;
            error["code"] = "mapNotFound";
            error["message"] = "Map not found";
            
            StringResponse response(http::status::not_found, req.version());
            response.set(http::field::content_type, "application/json");
            response.body() = json::serialize(error);
            response.prepare_payload();
            
            return send(std::move(response));
        }
        
        // Создаем JSON объект карты
        json::object map_json;
        map_json["id"] = *map->GetId();
        map_json["name"] = map->GetName();
        
        // Добавляем дороги
        json::array roads_json;
        for (const auto& road : map->GetRoads()) {
            json::object road_obj;
            if (road.IsHorizontal()) {
                road_obj["x0"] = road.GetStart().x;
                road_obj["y0"] = road.GetStart().y;
                road_obj["x1"] = road.GetEnd().x;
            } else {
                road_obj["x0"] = road.GetStart().x;
                road_obj["y0"] = road.GetStart().y;
                road_obj["y1"] = road.GetEnd().y;
            }
            roads_json.push_back(std::move(road_obj));
        }
        map_json["roads"] = std::move(roads_json);
        
        // Добавляем здания
        json::array buildings_json;
        for (const auto& building : map->GetBuildings()) {
            json::object building_obj;
            building_obj["x"] = building.GetBounds().position.x;
            building_obj["y"] = building.GetBounds().position.y;
            building_obj["w"] = building.GetBounds().size.width;
            building_obj["h"] = building.GetBounds().size.height;
            buildings_json.push_back(std::move(building_obj));
        }
        map_json["buildings"] = std::move(buildings_json);
        
        // Добавляем офисы
        json::array offices_json;
        for (const auto& office : map->GetOffices()) {
            json::object office_obj;
            office_obj["id"] = *office.GetId();
            office_obj["x"] = office.GetPosition().x;
            office_obj["y"] = office.GetPosition().y;
            office_obj["offsetX"] = office.GetOffset().dx;
            office_obj["offsetY"] = office.GetOffset().dy;
            offices_json.push_back(std::move(office_obj));
        }
        map_json["offices"] = std::move(offices_json);
        
        StringResponse response(http::status::ok, req.version());
        response.set(http::field::content_type, "application/json");
        response.body() = json::serialize(map_json);
        response.prepare_payload();
        
        send(std::move(response));
    }
    
    // Отправка ошибки 400 Bad Request
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
    
    // Отправка ошибки 404 Not Found
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
    
    // Отправка ошибки 405 Method Not Allowed
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