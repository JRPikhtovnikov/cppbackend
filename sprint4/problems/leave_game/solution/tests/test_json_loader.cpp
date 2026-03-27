#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include "../src/json_loader.h"

TEST_CASE("JSON loader basic functionality", "[json]") {
    std::string config_path = "test_config.json";
    std::ofstream file(config_path);
    file << R"({
        "defaultDogSpeed": 2.0,
        "defaultBagCapacity": 3,
        "lootGeneratorConfig": {
            "period": 1.5,
            "probability": 0.7
        },
        "maps": [
            {
                "id": "test_map",
                "name": "Test Map",
                "dogSpeed": 1.5,
                "bagCapacity": 5,
                "roads": [
                    {"x0": 0, "y0": 0, "x1": 10},
                    {"x0": 5, "y0": 0, "y1": 10}
                ],
                "buildings": [
                    {"x": 2, "y": 2, "w": 3, "h": 4}
                ],
                "offices": [
                    {"id": "office1", "x": 0, "y": 0, "offsetX": 0, "offsetY": 0}
                ],
                "lootTypes": [
                    {"name": "coin", "value": 1},
                    {"name": "gem", "value": 5}
                ]
            }
        ]
    })";
    file.close();
    
    json_loader::LootTypesMap loot_types;
    json_loader::LootValuesMap loot_values;
    double loot_period = 0, loot_prob = 0;
    double dog_retirement_time = 60.0;
    auto game = json_loader::LoadGame(config_path, loot_types, loot_values, 
                                      loot_period, loot_prob, dog_retirement_time);
    
    SECTION("Game loaded correctly") {
        CHECK(game.GetMaps().size() == 1);
    }
    
    SECTION("Loot config loaded") {
        CHECK(loot_period == 1.5);
        CHECK(loot_prob == 0.7);
    }
    
    SECTION("Map properties") {
        auto* map = game.FindMap(model::Map::Id("test_map"));
        REQUIRE(map != nullptr);
        
        CHECK(map->GetDogSpeed() == 1.5);
        CHECK(map->GetBagCapacity() == 5);
        CHECK(map->GetLootTypesCount() == 2);
        
        CHECK(map->GetRoads().size() == 2);
        CHECK(map->GetBuildings().size() == 1);
        CHECK(map->GetOffices().size() == 1);
    }
    
    SECTION("Loot values") {
        auto* map = game.FindMap(model::Map::Id("test_map"));
        REQUIRE(map != nullptr);
        
        auto it = loot_values.find(map->GetId());
        REQUIRE(it != loot_values.end());
        
        CHECK(it->second.size() == 2);
        CHECK(it->second[0] == 1);  
        CHECK(it->second[1] == 5);  
    }
    
    std::remove(config_path.c_str());
}