#pragma once

#include "json.hpp"
#include "mapper_runtime.h"

namespace joycon::webgui {

nlohmann::json ConfigToJson(const AppConfig& config);
void UpdateConfigFromJson(const nlohmann::json& root, AppConfig& config);
nlohmann::json RuntimeSnapshotToJson(const RuntimeSnapshot& snapshot);

} // namespace joycon::webgui
