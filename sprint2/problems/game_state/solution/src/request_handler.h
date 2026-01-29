#pragma once
#include "http_server.h"
#include "model.h"
#include "file_handler.h"

#include <boost/json.hpp>
#include <boost/beast/core/string_type.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace api {
using boost::beast::string_view;

constexpr string_view MAPS        = "/api/v1/maps";
constexpr string_view MAPS_SLASH  = "/api/v1/maps/";
constexpr string_view MAPS_PREFIX = "/api/v1/maps/";
constexpr string_view API_PREFIX  = "/api/";

constexpr string_view JOIN        = "/api/v1/game/join";
constexpr string_view PLAYERS     = "/api/v1/game/players";

constexpr string_view STATE = "/api/v1/game/state";

}  // namespace api

namespace http_handler {

namespace beast = boost::beast;
namespace http = beast::http;
namespace json = boost::json;

using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;

template <typename Send>
class HttpSendHandler : public file_handler::SendHandler {
public:
    explicit HttpSendHandler(Send&& send) : send_(std::forward<Send>(send)) {}

    void SendStringResponse(http::response<http::string_body>&& response) override {
        send_(std::move(response));
    }

    void SendEmptyResponse(http::response<http::empty_body>&& response) override {
        send_(std::move(response));
    }

private:
    Send send_;
};

class RequestHandler {
public:
    explicit RequestHandler(model::Game& game, const std::filesystem::path& static_path = "")
        : game_{game}
        , static_handler_(static_path) {
    }

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        HandleRequest(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
    }

private:
    model::Game& game_;
    file_handler::StaticFileHandler static_handler_;

    model::Players players_;
    model::PlayerTokens tokens_;
    model::GameSessions sessions_;
    model::Player::Id next_player_id_{0};

    std::random_device rd_;
    std::mt19937 gen_{rd_()};

private:
    static void SetApiCommonHeaders(StringResponse& resp) {
        resp.set(http::field::content_type, "application/json");
        resp.set(http::field::cache_control, "no-cache");
    }

    template <typename Body, typename Allocator>
    static StringResponse MakeJsonResponse(http::status status,
                                          const http::request<Body, http::basic_fields<Allocator>>& req,
                                          json::value body) {
        StringResponse resp(status, req.version());
        SetApiCommonHeaders(resp);
        resp.body() = json::serialize(body);
        resp.prepare_payload();
        return resp;
    }

    template <typename Body, typename Allocator>
    static StringResponse MakeError(http::status status,
                                    const http::request<Body, http::basic_fields<Allocator>>& req,
                                    std::string code,
                                    std::string message) {
        json::object err;
        err["code"] = std::move(code);
        err["message"] = std::move(message);
        return MakeJsonResponse(status, req, err);
    }

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
        const auto target = req.target();
        const auto method = req.method();

        if (target.starts_with(api::API_PREFIX)) {
            return HandleApiRequest(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        if (method != http::verb::get && method != http::verb::head) {
            json::object error;
            error["code"] = "invalidMethod";
            error["message"] = "Invalid method";

            StringResponse response(http::status::method_not_allowed, req.version());
            response.set(http::field::content_type, "application/json");
            response.body() = json::serialize(error);
            response.prepare_payload();
            return send(std::move(response));
        }

        // Serve static
        HttpSendHandler<Send> send_handler(std::forward<Send>(send));
        auto method_sv = boost::beast::http::to_string(method);
        std::string method_str(method_sv.data(), method_sv.size());
        static_handler_.HandleFileRequest(std::string(target), method_str, send_handler);
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        const auto target = req.target();

        if (target == api::JOIN) {
            return HandleJoin(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }
        if (target == api::PLAYERS) {
            return HandlePlayers(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

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

        if (target == api::STATE) {
            return HandleState(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        return send(MakeError(http::status::bad_request, req, "badRequest", "Bad request"));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleJoin(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto resp = MakeError(http::status::method_not_allowed, req,
                                  "invalidMethod", "Only POST method is expected");
            resp.set(http::field::allow, "POST");
            return send(std::move(resp));
        }

        const auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ct_it->value() != "application/json") {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error"));
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (...) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error"));
        }

        if (!parsed.is_object()) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error"));
        }

        const auto& obj = parsed.as_object();

        std::string user_name;
        std::string map_id;
        try {
            user_name = json::value_to<std::string>(obj.at("userName"));
            map_id = json::value_to<std::string>(obj.at("mapId"));
        } catch (...) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error"));
        }

        if (user_name.empty()) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Invalid name"));
        }

        const auto* map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            return send(MakeError(http::status::not_found, req,
                                  "mapNotFound", "Map not found"));
        }

        auto& session = sessions_.GetOrCreateByMap(model::Map::Id{map_id});
        const model::Vec2 pos = RandomPointOnRoad(*map);

        const auto pid = next_player_id_++;
        
        players_.Add(pid, user_name, session.GetId(), pos);
        session.AddPlayer(pid);

        model::Token token = tokens_.GenerateToken();
        while (!model::IsHex32(*token)) {
            token = tokens_.GenerateToken();
        }
        tokens_.Add(token, pid);

        json::object answer;
        answer["authToken"] = *token;
        answer["playerId"] = pid;

        return send(MakeJsonResponse(http::status::ok, req, answer));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandlePlayers(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto resp = MakeError(http::status::method_not_allowed, req,
                                  "invalidMethod", "Invalid method");
            resp.set(http::field::allow, "GET, HEAD");
            return send(std::move(resp));
        }

        const auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeError(http::status::unauthorized, req,
                                  "invalidToken", "Authorization header is missing"));
        }

        const std::string auth = std::string(auth_it->value());
        constexpr std::string_view prefix = "Bearer ";

        if (auth.size() <= prefix.size() || auth.compare(0, prefix.size(), prefix) != 0) {
            return send(MakeError(http::status::unauthorized, req,
                                  "invalidToken", "Invalid Authorization header"));
        }

        const std::string token_str = auth.substr(prefix.size());
        if (!model::IsHex32(token_str)) {
            return send(MakeError(http::status::unauthorized, req,
                                  "invalidToken", "Invalid token"));
        }

        const model::Token token{token_str};
        const auto player_id_opt = tokens_.FindPlayerIdByToken(token);
        if (!player_id_opt) {
            return send(MakeError(http::status::unauthorized, req,
                                  "unknownToken", "Player token has not been found"));
        }

        const model::Player* me = players_.Find(*player_id_opt);
        if (!me) {
            return send(MakeError(http::status::unauthorized, req,
                                "unknownToken", "Player token has not been found"));
        }

        const model::GameSession* session = sessions_.Find(me->GetSessionId());
        if (!session) {
            return send(MakeError(http::status::unauthorized, req,
                                "unknownToken", "Player token has not been found"));
        }

        json::object result;
        for (auto pid : session->GetPlayers()) {
            if (const model::Player* p = players_.Find(pid)) {
                json::object v;
                v["name"] = p->GetName();
                result[std::to_string(p->GetId())] = std::move(v);
            }
        }

        return send(MakeJsonResponse(http::status::ok, req, result));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleState(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto resp = MakeError(http::status::method_not_allowed, req,
                                "invalidMethod", "Invalid method");
            resp.set(http::field::allow, "GET, HEAD");
            return send(std::move(resp));
        }

        const auto auth_it = req.find(http::field::authorization);
        if (auth_it == req.end()) {
            return send(MakeError(http::status::unauthorized, req,
                                "invalidToken", "Authorization header is required"));
        }

        const std::string auth = std::string(auth_it->value());
        constexpr std::string_view prefix = "Bearer ";

        if (auth.size() <= prefix.size() || auth.compare(0, prefix.size(), prefix) != 0) {
            return send(MakeError(http::status::unauthorized, req,
                                "invalidToken", "Authorization header is required"));
        }

        const std::string token_str = auth.substr(prefix.size());
        if (!model::IsHex32(token_str)) {
            return send(MakeError(http::status::unauthorized, req,
                                "invalidToken", "Authorization header is required"));
        }

        const model::Token token{token_str};
        const auto player_id_opt = tokens_.FindPlayerIdByToken(token);
        if (!player_id_opt) {
            return send(MakeError(http::status::unauthorized, req,
                                "unknownToken", "Player token has not been found"));
        }

        const model::Player* me = players_.Find(*player_id_opt);
        if (!me) {
            return send(MakeError(http::status::unauthorized, req,
                                "unknownToken", "Player token has not been found"));
        }

        const model::GameSession* session = sessions_.Find(me->GetSessionId());
        if (!session) {
            return send(MakeError(http::status::unauthorized, req,
                                "unknownToken", "Player token has not been found"));
        }

        json::object players_obj;

        for (auto pid : session->GetPlayers()) {
            const model::Player* p = players_.Find(pid);
            if (!p) continue;

            json::object p_state;
            p_state["pos"] = json::array{ p->GetPos().x, p->GetPos().y };
            p_state["speed"] = json::array{ p->GetSpeed().x, p->GetSpeed().y };
            p_state["dir"] = model::ToDirString(p->GetDir());

            players_obj[std::to_string(p->GetId())] = std::move(p_state);
        }

        json::object body;
        body["players"] = std::move(players_obj);

        return send(MakeJsonResponse(http::status::ok, req, body));
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
            auto resp = MakeError(http::status::not_found, req, "mapNotFound", "Map not found");
            return send(std::move(resp));
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

    model::Vec2 RandomPointOnRoad(const model::Map& map) {
        const auto& roads = map.GetRoads();
        if (roads.empty()) return {0.0, 0.0};

        std::uniform_int_distribution<size_t> road_dist(0, roads.size() - 1);
        const auto& r = roads[road_dist(gen_)];

        const auto s = r.GetStart();
        const auto e = r.GetEnd();

        if (r.IsHorizontal()) {
            const auto x0 = std::min(s.x, e.x);
            const auto x1 = std::max(s.x, e.x);
            std::uniform_real_distribution<double> xdist(static_cast<double>(x0), static_cast<double>(x1));
            return {xdist(gen_), static_cast<double>(s.y)};
        } else {
            const auto y0 = std::min(s.y, e.y);
            const auto y1 = std::max(s.y, e.y);
            std::uniform_real_distribution<double> ydist(static_cast<double>(y0), static_cast<double>(y1));
            return {static_cast<double>(s.x), ydist(gen_)};
        }
    }

};

}  // namespace http_handler
