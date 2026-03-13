#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <optional>
#include <cctype>
#include "loot_generator.h"
#include <random>
#include <unordered_map>
#include <cstdint>
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

struct LostObject {
    std::uint32_t id;
    int type;
    int value;
    Vec2 pos;
};

struct BagItem {
    uint32_t id;
    int type;
    int value;
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

    void SetDogSpeed(double s) noexcept { dog_speed_ = s; }
    double GetDogSpeed() const noexcept { return dog_speed_; }

    size_t GetLootTypesCount() const noexcept { return loot_types_count_; }
    void SetLootTypesCount(size_t count) { loot_types_count_ = count; }

    void SetBagCapacity(size_t cap) noexcept { bag_capacity_ = cap; }
    size_t GetBagCapacity() const noexcept { return bag_capacity_; }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;
    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    double dog_speed_ = 1.0;
    size_t loot_types_count_ = 0;
    size_t bag_capacity_ = 3;
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
    GameSession(SessionId id, Map::Id map_id, double loot_period, double loot_probability)
        : id_(id)
        , map_id_(std::move(map_id))
        , loot_engine_(std::random_device{}())
        , loot_dist_(0.0, 1.0)
        , loot_generator_(std::make_unique<loot_gen::LootGenerator>(
            std::chrono::milliseconds(static_cast<int>(loot_period * 1000)),
            loot_probability,
            [this]() { return loot_dist_(loot_engine_); })) {}

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;
    
    GameSession(GameSession&&) = default;
    GameSession& operator=(GameSession&&) = default;

    SessionId GetId() const noexcept { return id_; }
    const Map::Id& GetMapId() const noexcept { return map_id_; }

    void AddPlayer(PlayerId pid) {
        player_ids_.push_back(pid);
    }

    const std::vector<PlayerId>& GetPlayers() const noexcept {
        return player_ids_;
    }   

    size_t GetLootCount() const { return loot_objects_.size(); }
    
    void AddLoot(int type, int value, Vec2 pos) {
        loot_objects_[next_loot_id_] = LostObject{next_loot_id_, type, value, pos};
        next_loot_id_++;
    }
    
    const auto& GetLootObjects() const { return loot_objects_; }
    
    unsigned GenerateLootCount(std::chrono::milliseconds delta, unsigned loot_count, unsigned looter_count) {
        return loot_generator_->Generate(delta, loot_count, looter_count);
    }

    void RemoveLoot(uint32_t id) {
        loot_objects_.erase(id);
    }

    uint32_t GetNextLootId() const noexcept { return next_loot_id_; }
    void SetNextLootId(uint32_t id) noexcept { next_loot_id_ = id; }
    void SetLootObjects(const std::unordered_map<uint32_t, LostObject>& loot) { loot_objects_ = loot; }

    std::string GetLootEngineState() const {
        std::ostringstream oss;
        oss << loot_engine_;
        return oss.str();
    }
    
    void SetLootEngineState(const std::string& state) {
        std::istringstream iss(state);
        iss >> loot_engine_;
    }

    
private:
    SessionId id_;
    Map::Id map_id_;
    std::vector<PlayerId> player_ids_;
    std::unordered_map<uint32_t, LostObject> loot_objects_;
    uint32_t next_loot_id_ = 0;
    
    mutable std::mt19937 loot_engine_;
    std::uniform_real_distribution<double> loot_dist_;
    
    std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
};

class GameSessions {
public:
    GameSession& GetOrCreateByMap(const Map::Id& map_id, double loot_period, double loot_probability) {
        auto it = map_to_session_.find(map_id);
        if (it != map_to_session_.end()) {
            return *sessions_.at(it->second);
        }
        
        const SessionId sid = next_session_id_++;
        auto session = std::make_unique<GameSession>(sid, map_id, loot_period, loot_probability);
        GameSession& ref = *session;
        sessions_.emplace(sid, std::move(session));
        map_to_session_.emplace(map_id, sid);
        return ref;
    }

    const GameSession* Find(SessionId id) const {
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second.get() : nullptr;
    }

    GameSession* Find(SessionId id) {
        auto it = sessions_.find(id);
        return it != sessions_.end() ? it->second.get() : nullptr;
    }

    const auto& GetAllSessions() const { return sessions_; }

    void SetNextSessionId(SessionId id) noexcept { next_session_id_ = id; }
    
    SessionId GetNextSessionId() const noexcept { return next_session_id_; }

    // Создание сессии с заданным ID (используется при загрузке)
    GameSession& CreateSessionWithId(SessionId id, const Map::Id& map_id,
                                      double loot_period, double loot_probability) {
        auto session = std::make_unique<GameSession>(id, map_id, loot_period, loot_probability);
        GameSession& ref = *session;
        sessions_.emplace(id, std::move(session));
        map_to_session_.emplace(map_id, id);
        // Важно: не увеличиваем next_session_id_, так как ID задан явно
        return ref;
    }
private:
    struct MapIdHash {
        size_t operator()(const Map::Id& id) const noexcept {
            return util::TaggedHasher<Map::Id>{}(id);
        }
    };

    SessionId next_session_id_{0};
    std::unordered_map<SessionId, std::unique_ptr<GameSession>> sessions_;
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
    void SetPos(Vec2 p) noexcept { pos_ = p; }

    void SetBagCapacity(size_t cap) noexcept { bag_capacity_ = cap; }
    size_t GetBagCapacity() const noexcept { return bag_capacity_; }
    const std::vector<BagItem>& GetBag() const noexcept { return bag_; }

    bool AddLoot(uint32_t loot_id, int type, int value) {
        if (bag_.size() >= bag_capacity_) return false;
        bag_.push_back({loot_id, type, value});
        return true;
    }

    void ClearBag() noexcept { bag_.clear(); }

    int GetScore() const noexcept { return score_; }
    void AddScore(int points) noexcept { score_ += points; }

private:
    Id id_;
    std::string name_;
    SessionId session_id_;

    Vec2 pos_{};
    Vec2 speed_{0.0, 0.0};
    Direction dir_ = Direction::North;

    std::vector<BagItem> bag_;
    size_t bag_capacity_ = 0;

    int score_ = 0;
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

    Player* FindMutable(Player::Id id) {
        if (auto it = players_.find(id); it != players_.end()) return &it->second;
        return nullptr;
    }

    std::unordered_map<Player::Id, Player>& GetAllMutable() noexcept {
        return players_;
    }
    
    const std::unordered_map<Player::Id, Player>& GetAll() const noexcept {
        return players_;
    }

private:
    std::unordered_map<Player::Id, Player> players_;
};

class PlayerTokens {
public:
    PlayerTokens() = default;

    PlayerTokens(PlayerTokens&&) = default;
    PlayerTokens& operator=(PlayerTokens&&) = default;

    Token GenerateToken() {
        const std::uint64_t a = generator1_();
        const std::uint64_t b = generator2_();

        std::ostringstream ss;
        ss << std::hex << std::nouppercase
           << std::setw(16) << std::setfill('0') << a
           << std::setw(16) << std::setfill('0') << b;

        return Token{ss.str()};
    }

    void Clear() {
        token_to_player_.clear();
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

    const std::unordered_map<Token, Player::Id, util::TaggedHasher<Token>>& GetAllTokens() const noexcept {
        return token_to_player_;
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
