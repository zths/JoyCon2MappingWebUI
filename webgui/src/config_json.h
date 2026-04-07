#pragma once

#include "json.hpp"
#include "mapper_runtime.h"

namespace joycon::webgui {

nlohmann::json ConfigToJson(const AppConfig& config);
void UpdateConfigFromJson(const nlohmann::json& root, AppConfig& config);
nlohmann::json RuntimeSnapshotToJson(const RuntimeSnapshot& snapshot);

/// Single source for factory defaults (runtime ctor, UI diff, exported template).
AppConfig BuiltinDefaultConfig();

/// Ordered mappable action ids (matches `MapperRuntime` / `ActionToVirtualKey`); UI loads via `/api/ui-schema`.
nlohmann::json ActionsCatalogJson();

/// `{ "defaults": <full config JSON>, "actions": [ ... ] }` for the Web UI.
nlohmann::json UiSchemaJson();

} // namespace joycon::webgui
