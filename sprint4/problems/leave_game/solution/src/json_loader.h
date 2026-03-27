#pragma once

#include <filesystem>
#include <boost/json.hpp>

#include "model.h"

namespace json_loader {

using LootTypesMap = std::unordered_map<model::Map::Id, boost::json::array, util::TaggedHasher<model::Map::Id>>;
using LootValuesMap = std::unordered_map<model::Map::Id, std::vector<int>, util::TaggedHasher<model::Map::Id>>;
model::Game LoadGame(const std::filesystem::path& json_path, LootTypesMap& loot_types, LootValuesMap& loot_values, 
    double& loot_period, double& loot_probability, double& dog_retirement_time);
std::string LoadFile(const std::filesystem::path& json_path);
}  // namespace json_loader
