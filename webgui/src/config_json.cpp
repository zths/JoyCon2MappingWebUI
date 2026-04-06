#include "config_json.h"

#include <algorithm>

namespace joycon::webgui {
namespace {

using json = nlohmann::json;

std::string NarrowLossy(const std::wstring& value) {
    std::string result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        if (ch >= 0 && ch < 128) {
            result.push_back(static_cast<char>(ch));
        } else {
            result += "?";
        }
    }
    return result;
}

std::string ConnectionStatusToString(ConnectionStatus status) {
    switch (status) {
    case ConnectionStatus::Disconnected: return "disconnected";
    case ConnectionStatus::Connecting: return "connecting";
    case ConnectionStatus::Connected: return "connected";
    case ConnectionStatus::Error: return "error";
    default: return "unknown";
    }
}

std::string SideToString(JoyConSide side) {
    return side == JoyConSide::Left ? "left" : "right";
}

uint16_t NormalizePort(int port, uint16_t fallback) {
    if (port < 1 || port > 65535) {
        return fallback;
    }
    return static_cast<uint16_t>(port);
}

json MouseSettingsToJson(const MouseSettings& settings) {
    return json{
        { "enabled", settings.enabled },
        { "baseSensitivity", settings.baseSensitivity },
        { "acceleration", settings.acceleration },
        { "exponent", settings.exponent },
        { "maxGain", settings.maxGain },
        { "distanceThreshold", static_cast<int>(settings.distanceThreshold) }
    };
}

MouseSettings MouseSettingsFromJson(const json& value, const MouseSettings& fallback) {
    MouseSettings settings = fallback;
    if (!value.is_object()) {
        return settings;
    }

    settings.enabled = value.value("enabled", settings.enabled);
    settings.baseSensitivity = value.value("baseSensitivity", settings.baseSensitivity);
    settings.acceleration = value.value("acceleration", settings.acceleration);
    settings.exponent = value.value("exponent", settings.exponent);
    settings.maxGain = value.value("maxGain", settings.maxGain);
    settings.distanceThreshold = static_cast<uint8_t>(
        std::clamp(value.value("distanceThreshold", static_cast<int>(settings.distanceThreshold)), 0, 12));
    return settings;
}

json StickMappingToJson(const StickMapping& mapping) {
    return json{
        { "deadzone", mapping.deadzone },
        { "hysteresis", mapping.hysteresis },
        { "diagonalUnlockRadius", mapping.diagonalUnlockRadius },
        { "fourWayHysteresisDegrees", mapping.fourWayHysteresisDegrees },
        { "eightWayHysteresisDegrees", mapping.eightWayHysteresisDegrees },
        { "up", mapping.up },
        { "down", mapping.down },
        { "left", mapping.left },
        { "right", mapping.right }
    };
}

StickMapping StickMappingFromJson(const json& value, const StickMapping& fallback) {
    StickMapping mapping = fallback;
    if (!value.is_object()) {
        return mapping;
    }

    mapping.deadzone = std::clamp(value.value("deadzone", mapping.deadzone), 0, 32767);
    mapping.hysteresis = std::clamp(value.value("hysteresis", mapping.hysteresis), 0, 32767);
    mapping.diagonalUnlockRadius = std::clamp(
        value.value("diagonalUnlockRadius", value.value("cardinalLockRadius", mapping.diagonalUnlockRadius)),
        mapping.deadzone,
        32767);
    mapping.fourWayHysteresisDegrees = std::clamp(
        value.value("fourWayHysteresisDegrees", mapping.fourWayHysteresisDegrees),
        0.0,
        45.0);
    mapping.eightWayHysteresisDegrees = std::clamp(
        value.value("eightWayHysteresisDegrees", mapping.eightWayHysteresisDegrees),
        0.0,
        22.5);
    mapping.up = value.value("up", mapping.up);
    mapping.down = value.value("down", mapping.down);
    mapping.left = value.value("left", mapping.left);
    mapping.right = value.value("right", mapping.right);
    return mapping;
}

json ControllerStateToJson(const ControllerStateSnapshot& state) {
    return json{
        { "side", SideToString(state.side) },
        { "status", ConnectionStatusToString(state.status) },
        { "error", state.error },
        { "deviceName", NarrowLossy(state.deviceName) },
        { "packetCount", state.packetCount },
        { "averageIntervalMs", state.averageIntervalMs },
        { "rateHz", state.rateHz },
        { "buttonBits", state.buttonBits },
        { "opticalDistance", static_cast<int>(state.decoded.opticalDistance) },
        { "opticalX", state.decoded.opticalMouse.x },
        { "opticalY", state.decoded.opticalMouse.y },
        { "stickLX", state.decoded.leftStick.x },
        { "stickLY", state.decoded.leftStick.y },
        { "stickRX", state.decoded.rightStick.x },
        { "stickRY", state.decoded.rightStick.y }
    };
}

} // namespace

json ConfigToJson(const AppConfig& config) {
    return json{
        { "mouse", {
            { "left", MouseSettingsToJson(config.mouse.left) },
            { "right", MouseSettingsToJson(config.mouse.right) }
        } },
        { "mapping", {
            { "left", config.mapping.left },
            { "right", config.mapping.right }
        } },
        { "sticks", {
            { "left", StickMappingToJson(config.sticks.left) },
            { "right", StickMappingToJson(config.sticks.right) }
        } },
        { "server", {
            { "port", config.server.port }
        } }
    };
}

void UpdateConfigFromJson(const json& root, AppConfig& config) {
    if (const auto mouseIt = root.find("mouse"); mouseIt != root.end() && mouseIt->is_object()) {
        const auto& mouse = *mouseIt;
        config.mouse.left = MouseSettingsFromJson(mouse.value("left", json::object()), config.mouse.left);
        config.mouse.right = MouseSettingsFromJson(mouse.value("right", json::object()), config.mouse.right);
    }

    if (const auto mappingIt = root.find("mapping"); mappingIt != root.end() && mappingIt->is_object()) {
        if (const auto leftIt = mappingIt->find("left"); leftIt != mappingIt->end() && leftIt->is_object()) {
            config.mapping.left = leftIt->get<std::map<std::string, std::string>>();
        }
        if (const auto rightIt = mappingIt->find("right"); rightIt != mappingIt->end() && rightIt->is_object()) {
            config.mapping.right = rightIt->get<std::map<std::string, std::string>>();
        }
    }

    if (const auto sticksIt = root.find("sticks"); sticksIt != root.end() && sticksIt->is_object()) {
        config.sticks.left = StickMappingFromJson(sticksIt->value("left", json::object()), config.sticks.left);
        config.sticks.right = StickMappingFromJson(sticksIt->value("right", json::object()), config.sticks.right);
    } else {
        if (const auto leftStickIt = root.find("leftStick"); leftStickIt != root.end()) {
            config.sticks.left = StickMappingFromJson(*leftStickIt, config.sticks.left);
        }
        if (const auto rightStickIt = root.find("rightStick"); rightStickIt != root.end()) {
            config.sticks.right = StickMappingFromJson(*rightStickIt, config.sticks.right);
        }
    }

    if (const auto serverIt = root.find("server"); serverIt != root.end() && serverIt->is_object()) {
        config.server.port = NormalizePort(serverIt->value("port", static_cast<int>(config.server.port)), config.server.port);
    }
}

json RuntimeSnapshotToJson(const RuntimeSnapshot& snapshot) {
    return json{
        { "ok", true },
        { "config", ConfigToJson(snapshot.config) },
        { "left", ControllerStateToJson(snapshot.left) },
        { "right", ControllerStateToJson(snapshot.right) },
        { "mouseStats", {
            { "movedPackets", snapshot.mouseStats.movedPackets },
            { "injectedMoves", snapshot.mouseStats.injectedMoves },
            { "gatedPackets", snapshot.mouseStats.gatedPackets },
            { "averageDispatchUs", snapshot.mouseStats.averageDispatchUs },
            { "maxDispatchUs", snapshot.mouseStats.maxDispatchUs },
            { "lastDistance", static_cast<int>(snapshot.mouseStats.lastDistance) },
            { "minDistance", static_cast<int>(snapshot.mouseStats.minDistance) },
            { "maxDistance", static_cast<int>(snapshot.mouseStats.maxDistance) }
        } }
    };
}

} // namespace joycon::webgui
