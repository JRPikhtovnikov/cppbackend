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
    } else {
        // Вертикальная дорога
        int x0 = json::value_to<int>(road_json.at("x0"));
        int y0 = json::value_to<int>(road_json.at("y0"));
        int y1 = json::value_to<int>(road_json.at("y1"));
        return model::Road(model::Road::VERTICAL, 
                          {x0, y0}, 
                          y1);
    }
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

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::string json_str = LoadFile(json_path);
    
    json::value json_value = json::parse(json_str);
    const json::object& root = json_value.as_object();
    
    model::Game game;
    
    const json::array& maps_array = root.at("maps").as_array();
    
    for (const auto& map_json : maps_array) {
        const json::object& map_obj = map_json.as_object();
        
        std::string id = json::value_to<std::string>(map_obj.at("id"));
        std::string name = json::value_to<std::string>(map_obj.at("name"));
        
        model::Map map(model::Map::Id{id}, name);
        
        const json::array& roads_array = map_obj.at("roads").as_array();
        for (const auto& road_json : roads_array) {
            map.AddRoad(ParseRoad(road_json));
        }
        
        const json::array& buildings_array = map_obj.at("buildings").as_array();
        for (const auto& building_json : buildings_array) {
            map.AddBuilding(ParseBuilding(building_json));
        }
        
        const json::array& offices_array = map_obj.at("offices").as_array();
        for (const auto& office_json : offices_array) {
            map.AddOffice(ParseOffice(office_json));
        }
        
        game.AddMap(std::move(map));
    }
    
    return game;
}

}  // namespace json_loader