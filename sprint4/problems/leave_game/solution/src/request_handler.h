#pragma once
#include "http_server.h"
#include "model.h"
#include "file_handler.h"
#include "json_loader.h"
#include "loot_generator.h"  
#include "collision_detector.h"
#include "geom.h"
#include "state_serialization.h"
#include "database.h"

#include <boost/json.hpp>
#include <boost/beast/core/string_type.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/log/trivial.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <algorithm>
#include <cmath>
#include <vector>
#include <fstream>

namespace api {
using boost::beast::string_view;

constexpr string_view MAPS        = "/api/v1/maps";
constexpr string_view MAPS_SLASH  = "/api/v1/maps/";
constexpr string_view MAPS_PREFIX = "/api/v1/maps/";
constexpr string_view API_PREFIX  = "/api/";

constexpr string_view JOIN        = "/api/v1/game/join";
constexpr string_view PLAYERS     = "/api/v1/game/players";

constexpr string_view STATE       = "/api/v1/game/state";
constexpr string_view ACTION      = "/api/v1/game/player/action";
constexpr string_view TICK        = "/api/v1/game/tick";

constexpr string_view RECORDS     = "/api/v1/game/records";
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
    explicit RequestHandler(model::Game& game,
                   http_server::net::strand<http_server::net::io_context::executor_type> api_strand,
                   const std::filesystem::path& static_path,
                   bool randomize_spawn_points,
                   bool manual_tick_enabled,
                   const json_loader::LootTypesMap& loot_types,
                   const json_loader::LootValuesMap& loot_values,
                   double loot_period,
                   double loot_probability,
                   std::optional<std::filesystem::path> state_file,
                   std::optional<std::chrono::milliseconds> save_state_period,
                   db::Database& db, 
                   double dog_retirement_time)
        : game_{game}
        , static_handler_(static_path)
        , api_strand_{std::move(api_strand)}
        , randomize_spawn_points_{randomize_spawn_points}
        , manual_tick_enabled_{manual_tick_enabled}
        , loot_types_map_{loot_types}
        , loot_values_map_{loot_values} 
        , loot_period_{loot_period}
        , loot_probability_{loot_probability}
        , gen_{rd_()}
        , state_file_path_(std::move(state_file))
        , save_state_period_(save_state_period)
        , db_(db)
        , dog_retirement_time_(dog_retirement_time){}

    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    template <typename Body, typename Allocator, typename Send>
    void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
        http_server::net::dispatch(api_strand_,
            [this,
            req = std::move(req),
            send = std::forward<Send>(send)]() mutable {
                HandleRequest(std::move(req), std::move(send));
            }
        );
    }

    void Tick(std::chrono::milliseconds delta) {
        AdvanceGameTime(delta.count());
        }

        std::string GetRandomGeneratorState() const {
        std::ostringstream oss;
        oss << gen_;
        return oss.str();
    }

    void RetirePlayer(model::Player::Id player_id){
        auto* player = players_.FindMutable(player_id);
        if (!player || player->IsRetired()) return;

        db::RetiredPlayerRecord record{
            .name = player->GetName(),
            .score = player->GetScore(),
            .play_time = player->GetTotalPlayTime()
        };

        try {
            db_.InsertRetiredPlayer(record);
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to insert retired player: " << e.what();
        }

        auto* session = sessions_.Find(player->GetSessionId());
        if (session) {
            session->RemovePlayer(player_id);
        }

        tokens_.RemoveByPlayerId(player_id);

        player->SetRetired(true);
        BOOST_LOG_TRIVIAL(info) << "Retiring player " << player_id << " due to idle timeout";
        players_.GetAllMutable().erase(player_id);
    }

    void SetRandomGeneratorState(const std::string& state) {
        std::istringstream iss(state);
        iss >> gen_;
    }

    void LoadState() {
        if (!state_file_path_ || !std::filesystem::exists(*state_file_path_))
            return;

        std::ifstream ifs(state_file_path_->string());
        if (!ifs.is_open())
            throw std::runtime_error("Cannot open state file for reading: " + state_file_path_->string());

        try {
            boost::archive::text_iarchive ia(ifs);
            serialization::GameStateRepr repr;
            ia >> repr;

            for (const auto& srep : repr.sessions) {
                if (!game_.FindMap(model::Map::Id(srep.map_id))) {
                    throw std::runtime_error("Map not found for session: " + srep.map_id);
                }
            }

            players_ = model::Players();
            tokens_.Clear();
            sessions_ = model::GameSessions();
            next_player_id_ = repr.next_player_id;
            sessions_.SetNextSessionId(repr.next_session_id);

            for (const auto& srep : repr.sessions) {
                auto& session = sessions_.CreateSessionWithId(
                    srep.id,                                   
                    model::Map::Id(srep.map_id),
                    loot_period_, loot_probability_
                );
                session.SetNextLootId(srep.next_loot_id);
                
                session.SetLootEngineState(srep.loot_engine_state);
                
                for (auto pid : srep.player_ids) {
                    session.AddPlayer(pid);
                }
                
                std::unordered_map<uint32_t, model::LostObject> loot_objects;
                for (const auto& lrep : srep.loot_objects) {
                    model::Vec2 pos;
                    pos.x = lrep.pos.x;
                    pos.y = lrep.pos.y;
                    loot_objects[lrep.id] = {lrep.id, lrep.type, lrep.value, pos};
                }
                session.SetLootObjects(std::move(loot_objects));
            }

            for (const auto& prep : repr.players) {
                model::Vec2 pos;
                pos.x = prep.pos.x;
                pos.y = prep.pos.y;
                auto& player = players_.Add(prep.id, prep.name, prep.session_id, pos);
                
                model::Vec2 speed;
                speed.x = prep.speed.x;
                speed.y = prep.speed.y;
                player.SetSpeed(speed);
                
                player.SetDir(static_cast<model::Direction>(prep.dir));
                player.AddScore(prep.score);
                player.SetBagCapacity(prep.bag_capacity);
                for (const auto& item : prep.bag) {
                    player.AddLoot(item.id, item.type, item.value);
                }
            }

            for (const auto& trep : repr.tokens) {
                tokens_.Add(model::Token(trep.token), trep.player_id);
            }

            SetRandomGeneratorState(repr.random_generator_state); 
        
            time_since_last_save_ = std::chrono::milliseconds::zero();
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load state: " + std::string(e.what()));
        }
    }

    void SaveState() const {
        if (!state_file_path_)
            return;

        serialization::GameStateRepr repr;
        repr.next_player_id = next_player_id_;
        repr.next_session_id = sessions_.GetNextSessionId(); 
        repr.random_generator_state = GetRandomGeneratorState();
        
        for (const auto& [sid, session_ptr] : sessions_.GetAllSessions()) {
            const auto& session = *session_ptr;
            serialization::SessionRepr srep;
            srep.id = session.GetId();
            srep.map_id = *session.GetMapId();
            srep.next_loot_id = session.GetNextLootId();
            srep.player_ids = session.GetPlayers();
            srep.loot_engine_state = session.GetLootEngineState();
            
            for (const auto& [lid, obj] : session.GetLootObjects()) {
                serialization::LostObjectRepr lrep;
                lrep.id = obj.id;
                lrep.type = obj.type;
                lrep.value = obj.value;
                lrep.pos.x = obj.pos.x;
                lrep.pos.y = obj.pos.y;
                srep.loot_objects.push_back(lrep);
            }
            repr.sessions.push_back(srep);
        }

        for (const auto& [pid, player] : players_.GetAll()) {
            serialization::PlayerRepr prep;
            prep.id = player.GetId();
            prep.name = player.GetName();
            prep.session_id = player.GetSessionId();
            
            prep.pos.x = player.GetPos().x;
            prep.pos.y = player.GetPos().y;
            
            prep.speed.x = player.GetSpeed().x;
            prep.speed.y = player.GetSpeed().y;
            
            prep.dir = static_cast<int>(player.GetDir());
            prep.score = player.GetScore();
            prep.bag_capacity = player.GetBagCapacity();
            prep.bag = player.GetBag();
            repr.players.push_back(prep);
        }

        for (const auto& [token, pid] : tokens_.GetAllTokens()) {
            repr.tokens.push_back({*token, pid});
        }

        auto tmp_path = *state_file_path_;
        tmp_path += ".tmp";
        {
            std::ofstream ofs(tmp_path.string());
            if (!ofs.is_open())
                throw std::runtime_error("Cannot open temporary file for writing: " + tmp_path.string());
            
            boost::archive::text_oarchive oa(ofs);
            oa << repr;
        }
        std::filesystem::rename(tmp_path, *state_file_path_);
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

    http_server::net::strand<http_server::net::io_context::executor_type> api_strand_;
    bool randomize_spawn_points_ = false;
    bool manual_tick_enabled_ = true;

    json_loader::LootTypesMap loot_types_map_;
    double loot_period_;
    double loot_probability_;
    json_loader::LootValuesMap loot_values_map_;

    std::optional<std::filesystem::path> state_file_path_;
    std::optional<std::chrono::milliseconds> save_state_period_;
    mutable std::chrono::milliseconds time_since_last_save_{0};

    db::Database& db_;
    double dog_retirement_time_;

private:
    template <typename Body, typename Allocator, typename Send>
    void HandleRecords(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send){
        BOOST_LOG_TRIVIAL(info) << "HandleRecords called with target: " << req.target();

        if (req.method() != http::verb::get && req.method() != http::verb::head) {
            auto resp = MakeError(http::status::method_not_allowed, req,
                                "invalidMethod", "Only GET and HEAD methods are allowed");
            resp.set(http::field::allow, "GET, HEAD");
            return send(std::move(resp));
        }

        size_t start = 0;
        size_t max_items = 100;

        auto target = req.target();
        auto query_pos = target.find('?');
        if (query_pos != boost::beast::string_view::npos) {
            boost::beast::string_view query = target.substr(query_pos + 1);
            BOOST_LOG_TRIVIAL(info) << "Query string: " << query;

            auto parse_param = [&](boost::beast::string_view name) -> std::optional<size_t> {
                auto pos = query.find(name);
                if (pos == boost::beast::string_view::npos) return std::nullopt;
                auto eq = query.find('=', pos);
                if (eq == boost::beast::string_view::npos) return std::nullopt;
                auto val_start = eq + 1;
                auto val_end = query.find('&', val_start);
                if (val_end == boost::beast::string_view::npos) val_end = query.size();
                boost::beast::string_view val_str = query.substr(val_start, val_end - val_start);
                std::string val_copy(val_str);
                char* end;
                long long v = std::strtoll(val_copy.c_str(), &end, 10);
                if (end == val_copy.c_str() || v < 0) {
                    BOOST_LOG_TRIVIAL(warning) << "Failed to parse parameter " << name << " value: " << val_str;
                    return std::nullopt;
                }
                BOOST_LOG_TRIVIAL(debug) << "Parsed " << name << " = " << v;
                return static_cast<size_t>(v);
            };

            if (auto s = parse_param("start")) start = *s;
            if (auto m = parse_param("maxItems")) {
                if (*m > 100) {
                    BOOST_LOG_TRIVIAL(warning) << "maxItems exceeds 100: " << *m;
                    return send(MakeError(http::status::bad_request, req,
                                        "invalidArgument", "maxItems must not exceed 100"));
                }
                max_items = *m;
            }
        }

        BOOST_LOG_TRIVIAL(info) << "Records request: start=" << start << ", max_items=" << max_items;

        std::vector<db::RetiredPlayerRecord> records;
        try {
            records = db_.GetRecords(start, max_items);
            BOOST_LOG_TRIVIAL(info) << "GetRecords returned " << records.size() << " records";
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Failed to get records: " << e.what();
            return send(MakeError(http::status::internal_server_error, req,
                                "internalError", "Database error"));
        }

        json::array result;
        for (const auto& rec : records) {
            json::object obj;
            obj["name"] = rec.name;
            obj["score"] = rec.score;
            obj["playTime"] = static_cast<double>(rec.play_time.count()) / 1000.0;
            result.push_back(std::move(obj));
        }

        BOOST_LOG_TRIVIAL(info) << "Sending " << result.size() << " records";
        send(MakeJsonResponse(http::status::ok, req, result));
    }

    void TrySaveState() {
        try {
            SaveState();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "Periodic state save failed: " << e.what();
        }
    }

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
        resp.set(http::field::cache_control, "no-cache");
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
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto resp = MakeError(http::status::method_not_allowed, req, "invalidMethod", "Only GET and HEAD methods are allowed");
                resp.set(http::field::allow, "GET, HEAD");
                return send(std::move(resp));
            }
            return SendMapsList(req, std::forward<Send>(send));
        }

        if (target.starts_with(api::MAPS_PREFIX)) {
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                auto resp = MakeError(http::status::method_not_allowed, req, "invalidMethod", "Only GET and HEAD methods are allowed");
                resp.set(http::field::allow, "GET, HEAD");
                return send(std::move(resp));
            }
            auto id = target.substr(api::MAPS_SLASH.size());
            if (!id.empty() && id.back() == '/') {
                id.remove_suffix(1);
            }
            return SendMapInfo(req, std::string(id), std::forward<Send>(send));
        }

        if (target == api::STATE) {
            return HandleState(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        if (target == api::ACTION) {
            return HandleAction(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        if (target == api::TICK) {
            if (!manual_tick_enabled_) {
                return send(MakeError(http::status::bad_request, req, "badRequest", "Invalid endpoint"));
            }
            return HandleTick(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        if (target.starts_with(api::RECORDS) || target == (std::string(api::RECORDS) + "/")) {
            return HandleRecords(std::forward<decltype(req)>(req), std::forward<Send>(send));
        }

        return send(MakeError(http::status::bad_request, req, "badRequest", "Invalid endpoint"));
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
        } catch (const json::system_error& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error: " + std::string(e.what())));
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
        } catch (const json::system_error& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error: " + std::string(e.what())));
        } catch (const std::bad_cast& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Join game request parse error: " + std::string(e.what())));
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

        auto& session = sessions_.GetOrCreateByMap(model::Map::Id{map_id}, loot_period_, loot_probability_);
        const model::Vec2 pos = randomize_spawn_points_
            ? RandomPointOnRoad(*map)
            : FirstRoadSpawnPoint(*map);


        const auto pid = next_player_id_++;
        
        model::Player& player = players_.Add(pid, user_name, session.GetId(), pos);
        player.SetBagCapacity(map->GetBagCapacity());
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
            p_state["score"] = p->GetScore();   

            json::array bag_array;
            for (const auto& item : p->GetBag()) {
                json::object item_obj;
                item_obj["id"] = item.id;
                item_obj["type"] = item.type;
                bag_array.push_back(std::move(item_obj));
            }
            p_state["bag"] = std::move(bag_array);

            players_obj[std::to_string(p->GetId())] = std::move(p_state);
        }

        json::object lost_objects_obj;
        for (const auto& [id, obj] : session->GetLootObjects()) {
            json::object loot_item;
            loot_item["type"] = obj.type;
            loot_item["pos"] = json::array{ obj.pos.x, obj.pos.y };
            lost_objects_obj[std::to_string(id)] = std::move(loot_item);
        }

        json::object body;
        body["players"] = std::move(players_obj);
        body["lostObjects"] = std::move(lost_objects_obj);

        return send(MakeJsonResponse(http::status::ok, req, body));
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleAction(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto resp = MakeError(http::status::method_not_allowed, req, "invalidMethod", "Invalid method");
            resp.set(http::field::allow, "POST");
            return send(std::move(resp));
        }

        const auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ct_it->value() != "application/json") {
            return send(MakeError(http::status::bad_request, req, "invalidArgument", "Invalid content type"));
        }

        auto response = ExecuteAuthorized(req, [&](const model::Token& token) {
            const auto pid_opt = tokens_.FindPlayerIdByToken(token);
            if (!pid_opt) {
                return MakeError(http::status::unauthorized, req, "unknownToken", "Player token has not been found");
            }

            model::Player* p = nullptr;
            p = players_.FindMutable(*pid_opt);
            if (!p) {
                return MakeError(http::status::unauthorized, req, "unknownToken", "Player token has not been found");
            }

            json::value v;
            try {
                v = json::parse(req.body());
            } catch (const json::system_error& e) {
                return MakeError(http::status::bad_request, req, "invalidArgument", 
                                 "Failed to parse action: " + std::string(e.what()));
            }
            if (!v.is_object()) {
                return MakeError(http::status::bad_request, req, "invalidArgument", "Failed to parse action");
            }
            const auto& obj = v.as_object();
            if (!obj.if_contains("move")) {
                return MakeError(http::status::bad_request, req, "invalidArgument", "Failed to parse action");
            }

            std::string move;
            try {
                move = json::value_to<std::string>(obj.at("move"));
            } catch (const json::system_error& e) {
                return MakeError(http::status::bad_request, req, "invalidArgument", 
                                 "Failed to parse action: " + std::string(e.what()));
            } catch (const std::bad_cast& e) {
                return MakeError(http::status::bad_request, req, "invalidArgument", 
                                 "Failed to parse action: " + std::string(e.what()));
            }

            const model::GameSession* session = sessions_.Find(p->GetSessionId());
            if (!session) {
                return MakeError(http::status::unauthorized, req, "unknownToken", "Player token has not been found");
            }

            const model::Map* map = game_.FindMap(session->GetMapId());
            if (!map) {
                return MakeError(http::status::unauthorized, req, "unknownToken", "Player token has not been found");
            }

            const double s = map->GetDogSpeed();

            if (move == "L") {
                p->SetDir(model::Direction::West);
                p->SetSpeed({-s, 0.0});
            } else if (move == "R") {
                p->SetDir(model::Direction::East);
                p->SetSpeed({ s, 0.0});
            } else if (move == "U") {
                p->SetDir(model::Direction::North);
                p->SetSpeed({0.0, -s});
            } else if (move == "D") {
                p->SetDir(model::Direction::South);
                p->SetSpeed({0.0,  s});
            } else if (move == "") {
                p->SetSpeed({0.0, 0.0});
            } else {
                return MakeError(http::status::bad_request, req, "invalidArgument", "Failed to parse action");
            }

            json::object ok;
            return MakeJsonResponse(http::status::ok, req, ok);
        });

        return send(std::move(response));
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
        SetApiCommonHeaders(response);
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

        auto it = loot_types_map_.find(map->GetId());
        if (it != loot_types_map_.end()) {
            map_json["lootTypes"] = it->second;
        } else {
            map_json["lootTypes"] = json::array();
        }

        StringResponse response(http::status::ok, req.version());
        SetApiCommonHeaders(response);
        response.body() = json::serialize(map_json);
        response.prepare_payload();

        send(std::move(response));
    }

    template <typename Body, typename Allocator>
    std::optional<model::Token> TryExtractToken(const http::request<Body, http::basic_fields<Allocator>>& req) const {
        const auto it = req.find(http::field::authorization);
        if (it == req.end()) return std::nullopt;

        const std::string auth = std::string(it->value());
        constexpr std::string_view prefix = "Bearer ";

        if (auth.size() <= prefix.size() || auth.compare(0, prefix.size(), prefix) != 0) return std::nullopt;

        const std::string token_str = auth.substr(prefix.size());
        if (!model::IsHex32(token_str)) return std::nullopt;

        return model::Token{token_str};
    }

    template <typename Body, typename Allocator, typename Fn>
    auto ExecuteAuthorized(const http::request<Body, http::basic_fields<Allocator>>& req, Fn&& action) {
        auto token_opt = TryExtractToken(req);
        if (!token_opt) {
            return MakeError(http::status::unauthorized, req, "invalidToken", "Authorization header is required");
        }
        return action(*token_opt);
    }

    struct Interval {
        double a = 0.0;
        double b = 0.0;
    };

    static std::vector<Interval> MergeIntervals(std::vector<Interval> v) {
        if (v.empty()) return v;
        std::sort(v.begin(), v.end(), [](const Interval& l, const Interval& r) {
            return l.a < r.a;
        });
        std::vector<Interval> out;
        out.reserve(v.size());
        out.push_back(v.front());
        for (size_t i = 1; i < v.size(); ++i) {
            if (v[i].a <= out.back().b) {
                out.back().b = std::max(out.back().b, v[i].b);
            } else {
                out.push_back(v[i]);
            }
        }
        return out;
    }

    static const Interval* FindContaining(const std::vector<Interval>& intervals, double x) {
        for (const auto& in : intervals) {
            if (x >= in.a && x <= in.b) return &in;
        }
        return nullptr;
    }

    static void ProcessRoadForXIntervals(const model::Road& r, double y, std::vector<Interval>& intervals) {
        constexpr double half_w = 0.4;
        const auto s = r.GetStart();
        const auto e = r.GetEnd();
        
        if (r.IsHorizontal()) {
            const double y0 = static_cast<double>(s.y);
            if (std::abs(y - y0) <= half_w) {
                const double x0 = static_cast<double>(std::min(s.x, e.x));
                const double x1 = static_cast<double>(std::max(s.x, e.x));
                intervals.push_back({x0 - half_w, x1 + half_w});
            }
        } else { // vertical
            const double x0 = static_cast<double>(s.x);
            const double y0 = static_cast<double>(std::min(s.y, e.y));
            const double y1 = static_cast<double>(std::max(s.y, e.y));
            if (y >= y0 && y <= y1) {
                intervals.push_back({x0 - half_w, x0 + half_w});
            }
        }
    }

    static void ProcessRoadForYIntervals(const model::Road& r, double x, std::vector<Interval>& intervals) {
        constexpr double half_w = 0.4;
        const auto s = r.GetStart();
        const auto e = r.GetEnd();
        
        if (r.IsVertical()) {
            const double x0 = static_cast<double>(s.x);
            if (std::abs(x - x0) <= half_w) {
                const double y0 = static_cast<double>(std::min(s.y, e.y));
                const double y1 = static_cast<double>(std::max(s.y, e.y));
                intervals.push_back({y0 - half_w, y1 + half_w});  
            }
        } else { // horizontal
            const double y0 = static_cast<double>(s.y);
            const double x0 = static_cast<double>(std::min(s.x, e.x));
            const double x1 = static_cast<double>(std::max(s.x, e.x));
            if (x >= x0 && x <= x1) {
                intervals.push_back({y0 - half_w, y0 + half_w});
            }
        }
    }

    static std::vector<Interval> AllowedXIntervalsAtY(const model::Map& map, double y) {
        std::vector<Interval> intervals;
        intervals.reserve(map.GetRoads().size());

        for (const auto& r : map.GetRoads()) {
            ProcessRoadForXIntervals(r, y, intervals);
        }
        return MergeIntervals(std::move(intervals));
    }

    static std::vector<Interval> AllowedYIntervalsAtX(const model::Map& map, double x) {
        std::vector<Interval> intervals;
        intervals.reserve(map.GetRoads().size());

        for (const auto& r : map.GetRoads()) {
            ProcessRoadForYIntervals(r, x, intervals);
        }
        return MergeIntervals(std::move(intervals));
    }

    static model::Vec2 FirstRoadSpawnPoint(const model::Map& map) {
        const auto& roads = map.GetRoads();
        if (roads.empty()) return {0.0, 0.0};
        const auto s = roads.front().GetStart();
        return {static_cast<double>(s.x), static_cast<double>(s.y)};
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
        }
        
        const auto y0 = std::min(s.y, e.y);
        const auto y1 = std::max(s.y, e.y);
        std::uniform_real_distribution<double> ydist(static_cast<double>(y0), static_cast<double>(y1));
        return {static_cast<double>(s.x), ydist(gen_)};
    }

    void AdvanceGameTime(std::int64_t time_delta_ms) {
        if (time_delta_ms <= 0) return;

        const double dt = static_cast<double>(time_delta_ms) / 1000.0;
        const std::chrono::milliseconds delta(time_delta_ms);

        std::unordered_map<model::Player::Id, model::Vec2> old_positions;
        for (const auto& [pid, p] : players_.GetAll()) {
            old_positions[pid] = p.GetPos();
        }

        for (auto& [pid, player] : players_.GetAllMutable()) {
            if (!player.IsRetired()) {
                player.AddPlayTime(delta);
            }
        }

        auto& all = players_.GetAllMutable();
        for (auto& [pid, p] : all) {
            const auto* session = sessions_.Find(p.GetSessionId());
            if (!session) continue;
            const auto* map = game_.FindMap(session->GetMapId());
            if (!map) continue;

            const auto v = p.GetSpeed();
            if (v.x == 0.0 && v.y == 0.0) continue;

            auto pos = p.GetPos();

            if (v.x != 0.0) {
                const double dx = v.x * dt;
                auto intervals = AllowedXIntervalsAtY(*map, pos.y);
                const Interval* cur = FindContaining(intervals, pos.x);
                if (!cur) {
                    p.SetSpeed({0.0, 0.0});
                    continue;
                }

                double new_x = pos.x + dx;
                if (dx > 0.0 && new_x > cur->b) {
                    new_x = cur->b;
                    p.SetSpeed({0.0, 0.0});
                } else if (dx < 0.0 && new_x < cur->a) {
                    new_x = cur->a;
                    p.SetSpeed({0.0, 0.0});
                }
                pos.x = new_x;
            } else if (v.y != 0.0) {
                const double dy = v.y * dt;
                auto intervals = AllowedYIntervalsAtX(*map, pos.x);
                const Interval* cur = FindContaining(intervals, pos.y);
                if (!cur) {
                    p.SetSpeed({0.0, 0.0});
                    continue;
                }

                double new_y = pos.y + dy;
                if (dy > 0.0 && new_y > cur->b) {
                    new_y = cur->b;
                    p.SetSpeed({0.0, 0.0});
                } else if (dy < 0.0 && new_y < cur->a) {
                    new_y = cur->a;
                    p.SetSpeed({0.0, 0.0});
                }
                pos.y = new_y;
            }
            p.SetPos(pos);
        }

        for (const auto& [sid, _] : sessions_.GetAllSessions()) {
            auto* session = sessions_.Find(sid);
            if (!session) continue;

            const auto* map = game_.FindMap(session->GetMapId());
            if (!map) continue;

            const auto& values_it = loot_values_map_.find(map->GetId());
            const std::vector<int>* loot_values = nullptr;
            if (values_it != loot_values_map_.end()) {
                loot_values = &values_it->second;
            }

            struct CollisionObject {
                bool is_office;
                size_t office_idx;      // индекс офиса в карте
                uint32_t loot_id;        // id потерянного предмета
                int loot_type;            // тип предмета
                int loot_value;           // стоимость предмета
                geom::Point2D pos;
                double width;
            };
            std::vector<CollisionObject> objects;
            std::vector<bool> alive;     

            // Офисы (базы)
            const auto& offices = map->GetOffices();
            for (size_t i = 0; i < offices.size(); ++i) {
                const auto& off = offices[i];
                objects.push_back(CollisionObject{
                    .is_office = true,
                    .office_idx = i,
                    .loot_id = 0,
                    .loot_type = 0,
                    .loot_value = 0,
                    .pos = {static_cast<double>(off.GetPosition().x),
                            static_cast<double>(off.GetPosition().y)},
                    .width = 0.25  
                });
                alive.push_back(true);
            }

            const auto& loot_map = session->GetLootObjects();
            for (const auto& [id, obj] : loot_map) {
                objects.push_back(CollisionObject{
                    .is_office = false,
                    .office_idx = 0,
                    .loot_id = id,
                    .loot_type = obj.type,
                    .loot_value = obj.value,
                    .pos = {obj.pos.x, obj.pos.y},
                    .width = 0.0 
                });
                alive.push_back(true);
            }

            struct GathererInfo {
                model::Player::Id player_id;
                geom::Point2D start;
                geom::Point2D end;
                double width;
            };
            std::vector<GathererInfo> gatherers;
            std::vector<model::Player*> session_players;

            for (auto pid : session->GetPlayers()) {
                auto* p = players_.FindMutable(pid);
                if (!p) continue;
                auto old_it = old_positions.find(pid);
                if (old_it == old_positions.end()) continue;

                session_players.push_back(p);
                gatherers.push_back(GathererInfo{
                    .player_id = pid,
                    .start = {old_it->second.x, old_it->second.y},
                    .end = {p->GetPos().x, p->GetPos().y},
                    .width = 0.3  
                });
            }

            if (gatherers.empty() || objects.empty()) continue;

            class Provider : public collision_detector::ItemGathererProvider {
            public:
                Provider(const std::vector<CollisionObject>& objs,
                        const std::vector<GathererInfo>& gath)
                    : objects_(objs), gatherers_(gath) {}

                size_t ItemsCount() const override { return objects_.size(); }
                collision_detector::Item GetItem(size_t idx) const override {
                    return {objects_[idx].pos, objects_[idx].width};
                }
                size_t GatherersCount() const override { return gatherers_.size(); }
                collision_detector::Gatherer GetGatherer(size_t idx) const override {
                    return {gatherers_[idx].start, gatherers_[idx].end, gatherers_[idx].width};
                }
            private:
                const std::vector<CollisionObject>& objects_;
                const std::vector<GathererInfo>& gatherers_;
            };

            Provider provider(objects, gatherers);
            auto events = collision_detector::FindGatherEvents(provider);

            for (const auto& ev : events) {
                if (!alive[ev.item_id]) continue; 

                const auto& obj = objects[ev.item_id];
                auto* player = session_players[ev.gatherer_id];
                if (!player) continue;

                if (obj.is_office) {
                    if (!player->GetBag().empty()) {
                        int total_score = 0;
                        for (const auto& item : player->GetBag()) {
                            total_score += item.value;
                        }
                        player->AddScore(total_score);
                        player->ClearBag();
                    }
                } else {
                    if (player->GetBag().size() < player->GetBagCapacity()) {
                        int value = 0;
                        if (loot_values && obj.loot_type < static_cast<int>(loot_values->size())) {
                            value = (*loot_values)[obj.loot_type];
                        }
                        
                        if (player->AddLoot(obj.loot_id, obj.loot_type, value)) {
                            session->RemoveLoot(obj.loot_id);
                            alive[ev.item_id] = false;
                        }
                    }
                }
            }
        }

        for (const auto& [sid, _] : sessions_.GetAllSessions()) {
            auto* session = sessions_.Find(sid);
            if (!session) continue;
            const auto* map = game_.FindMap(session->GetMapId());
            if (!map) continue;

            unsigned looter_count = session->GetPlayers().size();
            unsigned loot_count = session->GetLootCount();

            try {
                unsigned new_loot = session->GenerateLootCount(
                    std::chrono::milliseconds(time_delta_ms),
                    loot_count, looter_count);

                if (new_loot > 0 && map->GetLootTypesCount() > 0) {
                    std::uniform_int_distribution<int> type_dist(0, map->GetLootTypesCount() - 1);
                    
                    const auto& values_it = loot_values_map_.find(map->GetId());
                    if (values_it != loot_values_map_.end()) {
                        const auto& values = values_it->second;
                        
                        for (unsigned i = 0; i < new_loot; ++i) {
                            int type = type_dist(gen_);
                            int value = (type < static_cast<int>(values.size())) ? values[type] : 0;
                            model::Vec2 pos = RandomPointOnRoad(*map);
                            session->AddLoot(type, value, pos);
                        }
                    }
                }
            } catch (...) {}
        }

        std::vector<model::Player::Id> to_retire;
        for (auto& [pid, player] : players_.GetAllMutable()) {
            if (player.IsRetired()) continue;
            if (player.GetSpeed().x == 0.0 && player.GetSpeed().y == 0.0) {
                player.AddIdleTime(delta);
                if (player.GetIdleTime() >= std::chrono::milliseconds(static_cast<int>(dog_retirement_time_ * 1000))) {
                    to_retire.push_back(pid);
                }
            } else {
                player.ResetIdleTime();
            }
        }
        for (auto pid : to_retire) {
            RetirePlayer(pid);
        }

        if (save_state_period_) {
            time_since_last_save_ += delta;
            if (time_since_last_save_ >= *save_state_period_) {
                TrySaveState();
                time_since_last_save_ = std::chrono::milliseconds::zero();
            }
        }
    }

    template <typename Body, typename Allocator, typename Send>
    void HandleTick(const http::request<Body, http::basic_fields<Allocator>>& req, Send&& send) {
        if (req.method() != http::verb::post) {
            auto resp = MakeError(http::status::method_not_allowed, req,
                                  "invalidMethod", "Only POST method is expected");
            resp.set(http::field::allow, "POST");
            return send(std::move(resp));
        }

        const auto ct_it = req.find(http::field::content_type);
        if (ct_it == req.end() || ct_it->value() != "application/json") {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON"));
        }

        json::value parsed;
        try {
            parsed = json::parse(req.body());
        } catch (const json::system_error& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON: " + std::string(e.what())));
        }

        if (!parsed.is_object()) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON"));
        }

        const auto& obj = parsed.as_object();
        if (!obj.if_contains("timeDelta")) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON"));
        }

        std::int64_t time_delta_ms = -1;
        try {
            const auto& td = obj.at("timeDelta");
            if (!td.is_int64()) {
                return send(MakeError(http::status::bad_request, req,
                                      "invalidArgument", "Failed to parse tick request JSON"));
            }
            time_delta_ms = td.as_int64();
        } catch (const json::system_error& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON: " + std::string(e.what())));
        } catch (const std::bad_cast& e) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON: " + std::string(e.what())));
        }

        if (time_delta_ms < 0) {
            return send(MakeError(http::status::bad_request, req,
                                  "invalidArgument", "Failed to parse tick request JSON"));
        }

        AdvanceGameTime(time_delta_ms);

        json::object ok;
        return send(MakeJsonResponse(http::status::ok, req, ok));
    }

};

}  // namespace http_handler