#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <optional>
#include <cctype>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

enum class Direction {
    North,
    South,
    West,
    East
};


inline const char* ToDirString(Direction d) {
    switch (d) {
        case Direction::West:  return "L";
        case Direction::East:  return "R";
        case Direction::North: return "U";
        case Direction::South: return "D";
    }
    return "U";
}

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
};

namespace detail {
struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

inline bool IsHex32(const std::string& s) {
    if (s.size() != 32) return false;
    for (unsigned char ch : s) {
        if (!std::isxdigit(ch)) return false;
    }
    return true;
}

using PlayerId  = std::uint32_t;
using SessionId = std::uint32_t;
class GameSession {
public:
    GameSession(SessionId id, Map::Id map_id)
        : id_(id)
        , map_id_(std::move(map_id)) {
    }

    SessionId GetId() const noexcept { return id_; }
    const Map::Id& GetMapId() const noexcept { return map_id_; }

    void AddPlayer(PlayerId pid) {
        player_ids_.push_back(pid);
    }

    const std::vector<PlayerId>& GetPlayers() const noexcept {
        return player_ids_;
    }

private:
    SessionId id_;
    Map::Id map_id_;
    std::vector<PlayerId> player_ids_;
};

class GameSessions {
public:
    GameSession& GetOrCreateByMap(const Map::Id& map_id) {
        if (auto it = map_to_session_.find(map_id); it != map_to_session_.end()) {
            return sessions_.at(it->second);
        }

        const SessionId sid = next_session_id_++;
        auto [it_sess, inserted] =
            sessions_.emplace(sid, GameSession{sid, map_id});   // map_id копируем как Tagged
        map_to_session_.emplace(map_id, sid);

        return it_sess->second;
    }

    const GameSession* Find(SessionId id) const {
        if (auto it = sessions_.find(id); it != sessions_.end()) return &it->second;
        return nullptr;
    }

private:
    struct MapIdHash {
        size_t operator()(const Map::Id& id) const noexcept {
            return util::TaggedHasher<Map::Id>{}(id);
        }
    };

    SessionId next_session_id_{0};
    std::unordered_map<SessionId, GameSession> sessions_;
    std::unordered_map<Map::Id, SessionId, MapIdHash> map_to_session_;
};

class Player {
public:
    using Id = PlayerId;

    Player(Id id, std::string name, SessionId session_id, Vec2 pos)
        : id_(id)
        , name_(std::move(name))
        , session_id_(session_id)
        , pos_(pos) {
    }

    Id GetId() const noexcept { return id_; }
    const std::string& GetName() const noexcept { return name_; }
    SessionId GetSessionId() const noexcept { return session_id_; }

    const Vec2& GetPos() const noexcept { return pos_; }
    const Vec2& GetSpeed() const noexcept { return speed_; }
    Direction GetDir() const noexcept { return dir_; }

    void SetSpeed(Vec2 v) noexcept { speed_ = v; }
    void SetDir(Direction d) noexcept { dir_ = d; }

private:
    Id id_;
    std::string name_;
    SessionId session_id_;

    Vec2 pos_{};
    Vec2 speed_{0.0, 0.0};
    Direction dir_ = Direction::North;
};


class Players {
public:
    Player& Add(Player::Id id, std::string name, SessionId session_id, Vec2 pos) {
        auto [it, inserted] = players_.emplace(id, Player{id, std::move(name), session_id, pos});
        return it->second;
    }

    const Player* Find(Player::Id id) const {
        if (auto it = players_.find(id); it != players_.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<Player::Id, Player> players_;
};

class PlayerTokens {
public:
    PlayerTokens() = default;

    Token GenerateToken() {
        const std::uint64_t a = generator1_();
        const std::uint64_t b = generator2_();

        std::ostringstream ss;
        ss << std::hex << std::nouppercase
           << std::setw(16) << std::setfill('0') << a
           << std::setw(16) << std::setfill('0') << b;

        return Token{ss.str()};
    }

    void Add(Token token, Player::Id player_id) {
        token_to_player_.emplace(std::move(token), player_id);
    }

    std::optional<Player::Id> FindPlayerIdByToken(const Token& token) const {
        if (auto it = token_to_player_.find(token); it != token_to_player_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

private:
    std::random_device random_device_;
    std::mt19937_64 generator1_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};
    std::mt19937_64 generator2_{[this] {
        std::uniform_int_distribution<std::mt19937_64::result_type> dist;
        return dist(random_device_);
    }()};

    std::unordered_map<Token, Player::Id, util::TaggedHasher<Token>> token_to_player_;
};

}  // namespace model
