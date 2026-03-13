#pragma once

#include "model.h"
#include "geom.h"

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/utility.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace geom{
    // Сериализация геометрических типов
    template <typename Archive>
    void serialize(Archive& ar, Point2D& p, [[maybe_unused]] const unsigned int version) {
        ar & p.x & p.y;
    }

    template <typename Archive>
    void serialize(Archive& ar, Vec2D& v, [[maybe_unused]] const unsigned int version) {
        ar & v.x & v.y;
    }
}

namespace serialization {

struct LostObjectRepr {
    uint32_t id = 0;
    int type = 0;
    int value = 0;
    geom::Point2D pos;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & id & type & value & pos;
    }
};

struct PlayerRepr {
    uint32_t id = 0;
    std::string name;
    uint32_t session_id = 0;
    geom::Point2D pos;
    geom::Vec2D speed;
    int dir = 0;                       
    int score = 0;
    size_t bag_capacity = 0;
    std::vector<model::BagItem> bag;   

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & id & name & session_id & pos & speed & dir & score & bag_capacity & bag;
    }
};

// Вспомогательная структура для токена
struct TokenRepr {
    std::string token;
    uint32_t player_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & token & player_id;
    }
};

// Вспомогательная структура для игровой сессии
struct SessionRepr {
    uint32_t id = 0;
    std::string map_id;
    uint32_t next_loot_id = 0;
    std::vector<uint32_t> player_ids;
    std::vector<LostObjectRepr> loot_objects;
    std::string loot_engine_state;

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & id & map_id & next_loot_id & player_ids & loot_objects & loot_engine_state;
    }
};

// Корневая структура состояния игры
struct GameStateRepr {
    uint32_t next_player_id = 0;
    std::vector<SessionRepr> sessions;
    std::vector<PlayerRepr> players;
    std::vector<TokenRepr> tokens;
    std::string random_generator_state; 

    template <typename Archive>
    void serialize(Archive& ar, [[maybe_unused]] const unsigned int version) {
        ar & next_player_id & sessions & players & tokens & random_generator_state;
    }
};

} // namespace serialization

namespace model {
    template <typename Archive>
    void serialize(Archive& ar, BagItem& item, [[maybe_unused]] const unsigned int version) {
        ar & item.id & item.type & item.value;
    }
}