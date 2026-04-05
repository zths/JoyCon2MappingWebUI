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

json StickMappingToJson(const StickMapping& mapping) {
    return json{
        { "deadzone", mapping.deadzone },
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
            { "enabled", config.mouse.enabled },
            { "baseSensitivity", config.mouse.baseSensitivity },
            { "acceleration", config.mouse.acceleration },
            { "exponent", config.mouse.exponent },
            { "maxGain", config.mouse.maxGain },
            { "distanceThreshold", static_cast<int>(config.mouse.distanceThreshold) }
        } },
        { "mapping", {
            { "left", config.mapping.left },
            { "right", config.mapping.right }
        } },
        { "sticks", {
            { "left", StickMappingToJson(config.leftStick) },
            { "right", StickMappingToJson(config.rightStick) }
        } },
        { "server", {
            { "port", config.server.port }
        } }
    };
}

void UpdateConfigFromJson(const json& root, AppConfig& config) {
    if (const auto mouseIt = root.find("mouse"); mouseIt != root.end() && mouseIt->is_object()) {
        const auto& mouse = *mouseIt;
        config.mouse.enabled = mouse.value("enabled", config.mouse.enabled);
        config.mouse.baseSensitivity = mouse.value("baseSensitivity", config.mouse.baseSensitivity);
        config.mouse.acceleration = mouse.value("acceleration", config.mouse.acceleration);
        config.mouse.exponent = mouse.value("exponent", config.mouse.exponent);
        config.mouse.maxGain = mouse.value("maxGain", config.mouse.maxGain);
        config.mouse.distanceThreshold = static_cast<uint8_t>(
            std::clamp(mouse.value("distanceThreshold", static_cast<int>(config.mouse.distanceThreshold)), 0, 12));
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
        config.leftStick = StickMappingFromJson(sticksIt->value("left", json::object()), config.leftStick);
        config.rightStick = StickMappingFromJson(sticksIt->value("right", json::object()), config.rightStick);
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
