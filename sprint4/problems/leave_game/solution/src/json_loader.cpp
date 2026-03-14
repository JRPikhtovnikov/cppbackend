#include "json_loader.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>

namespace json_loader {

namespace json = boost::json;

std::string LoadFile(const std::filesystem::path& json_path) {
    std::ifstream file(json_path.string());
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + json_path.string());
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

model::Road ParseRoad(const json::value& road_json) {
    if (road_json.as_object().contains("x1")) {
        // Горизонтальная дорога
        int x0 = json::value_to<int>(road_json.at("x0"));
        int y0 = json::value_to<int>(road_json.at("y0"));
        int x1 = json::value_to<int>(road_json.at("x1"));
        return model::Road(model::Road::HORIZONTAL, 
                          {x0, y0}, 
                          x1);
    }
    // Вертикальная дорога
    int x0 = json::value_to<int>(road_json.at("x0"));
    int y0 = json::value_to<int>(road_json.at("y0"));
    int y1 = json::value_to<int>(road_json.at("y1"));
    return model::Road(model::Road::VERTICAL, 
                        {x0, y0}, 
                        y1);
}

model::Building ParseBuilding(const json::value& building_json) {
    int x = json::value_to<int>(building_json.at("x"));
    int y = json::value_to<int>(building_json.at("y"));
    int w = json::value_to<int>(building_json.at("w"));
    int h = json::value_to<int>(building_json.at("h"));
    
    return model::Building({{x, y}, {w, h}});
}

model::Office ParseOffice(const json::value& office_json) {
    std::string id = json::value_to<std::string>(office_json.at("id"));
    int x = json::value_to<int>(office_json.at("x"));
    int y = json::value_to<int>(office_json.at("y"));
    int offsetX = json::value_to<int>(office_json.at("offsetX"));
    int offsetY = json::value_to<int>(office_json.at("offsetY"));
    
    return model::Office(model::Office::Id{std::move(id)}, 
                         {x, y}, 
                         {offsetX, offsetY});
}

model::Game LoadGame(const std::filesystem::path& json_path, 
                     json_loader::LootTypesMap& loot_types, 
                     LootValuesMap& loot_values, 
                     double& loot_period, 
                     double& loot_probability,
                     double& dog_retirement_time) {
    std::string json_str = LoadFile(json_path);
    json::value json_value;

    try {
        json_value = json::parse(json_str);
    } catch (const json::system_error& e) {
        throw std::runtime_error(
            "JSON parse error in file " + json_path.string() + ": " + e.what()
        );
    }
    const json::object& root = json_value.as_object();

    if (const auto* cfg = root.if_contains("lootGeneratorConfig")) {
        const auto& cfg_obj = cfg->as_object();
        loot_period = json::value_to<double>(cfg_obj.at("period"));
        loot_probability = json::value_to<double>(cfg_obj.at("probability"));
    } else {
        throw std::runtime_error("lootGeneratorConfig not found");
    }
    
    model::Game game;
    
    const json::array& maps_array = root.at("maps").as_array();

    double default_speed = 1.0;
    if (const auto* v = root.if_contains("defaultDogSpeed")) {
        default_speed = json::value_to<double>(*v);
    }

    double default_bag_capacity = 3.0;
    if (const auto* v = root.if_contains("defaultBagCapacity")) {
        default_bag_capacity = json::value_to<double>(*v);
    }

    dog_retirement_time = 60.0;
    if (const auto* v = root.if_contains("dogRetirementTime")) {
        dog_retirement_time = json::value_to<double>(*v);
    }
    
    for (const auto& map_json : maps_array) {
        const json::object& map_obj = map_json.as_object();
        
        std::string id = json::value_to<std::string>(map_obj.at("id"));
        std::string name = json::value_to<std::string>(map_obj.at("name"));
        
        model::Map map(model::Map::Id{id}, name);

        // Скорость собаки на карте
        double map_speed = default_speed;
        if (const auto* v = map_obj.if_contains("dogSpeed")) {
            map_speed = json::value_to<double>(*v);
        }
        map.SetDogSpeed(map_speed);
        
        // Дороги
        const json::array& roads_array = map_obj.at("roads").as_array();
        for (const auto& road_json : roads_array) {
            map.AddRoad(ParseRoad(road_json));
        }
        
        // Здания
        const json::array& buildings_array = map_obj.at("buildings").as_array();
        for (const auto& building_json : buildings_array) {
            map.AddBuilding(ParseBuilding(building_json));
        }
        
        // Офисы
        const json::array& offices_array = map_obj.at("offices").as_array();
        for (const auto& office_json : offices_array) {
            map.AddOffice(ParseOffice(office_json));
        }

        // Типы лута для карты
        size_t loot_types_count = 0;
        if (const auto* v = map_obj.if_contains("lootTypes")) {
            if (v->is_array()) {
                loot_types_count = v->as_array().size();
                loot_types[map.GetId()] = v->as_array();
            }
        }
        map.SetLootTypesCount(loot_types_count);

        std::vector<int> values;
        if (const auto* v = map_obj.if_contains("lootTypes")) {
            if (v->is_array()) {
                loot_types_count = v->as_array().size();
                loot_types[map.GetId()] = v->as_array();
                
                for (const auto& item : v->as_array()) {
                    if (item.is_object() && item.as_object().contains("value")) {
                        values.push_back(json::value_to<int>(item.as_object().at("value")));
                    } else {
                        values.push_back(0);
                    }
                }
            }
        }
        map.SetLootTypesCount(loot_types_count);
        loot_values[map.GetId()] = std::move(values);

        double bag_capacity = default_bag_capacity;
        if (const auto* v = map_obj.if_contains("bagCapacity")) {
            bag_capacity = json::value_to<double>(*v);
        }
        map.SetBagCapacity(static_cast<size_t>(bag_capacity));
        
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader