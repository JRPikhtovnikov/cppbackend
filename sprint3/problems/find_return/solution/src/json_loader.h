#pragma once

#include <filesystem>
#include <boost/json.hpp>

#include "model.h"

namespace json_loader {

using LootTypesMap = std::unordered_map<model::Map::Id, boost::json::array, util::TaggedHasher<model::Map::Id>>;
model::Game LoadGame(const std::filesystem::path& json_path, LootTypesMap& loot_types, double& loot_period, double& loot_probability);
std::string LoadFile(const std::filesystem::path& json_path);
}  // namespace json_loader
