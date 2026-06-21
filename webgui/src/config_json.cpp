#include "config_json.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace joycon::webgui {
namespace {

using json = nlohmann::json;

/// Format a 48-bit BT address as "AA:BB:CC:DD:EE:FF" (big-endian); empty if 0.
std::string FormatMacAddress(uint64_t address) {
    if (address == 0) {
        return "";
    }
    char buffer[18];
    std::snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
        static_cast<unsigned>((address >> 40) & 0xFF),
        static_cast<unsigned>((address >> 32) & 0xFF),
        static_cast<unsigned>((address >> 24) & 0xFF),
        static_cast<unsigned>((address >> 16) & 0xFF),
        static_cast<unsigned>((address >> 8) & 0xFF),
        static_cast<unsigned>(address & 0xFF));
    return buffer;
}

/// Parse "AA:BB:CC:DD:EE:FF" (any non-hex separators tolerated) into a uint64; 0 if invalid.
uint64_t ParseMacAddress(const std::string& text) {
    uint64_t address = 0;
    int nibbles = 0;
    for (char ch : text) {
        int value = -1;
        if (ch >= '0' && ch <= '9') value = ch - '0';
        else if (ch >= 'a' && ch <= 'f') value = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') value = ch - 'A' + 10;
        else continue;
        address = (address << 4) | static_cast<uint64_t>(value);
        if (++nibbles > 12) {
            return 0;
        }
    }
    return nibbles == 12 ? address : 0;
}

json PairedDeviceToJson(const PairedDevice& device) {
    return json{
        { "paired", device.paired },
        { "controllerAddress", FormatMacAddress(device.controllerAddress) },
        { "hostAddress", FormatMacAddress(device.hostAddress) },
        { "deviceName", device.deviceName }
    };
}

PairedDevice PairedDeviceFromJson(const json& value, const PairedDevice& fallback) {
    PairedDevice device = fallback;
    if (!value.is_object()) {
        return device;
    }
    device.paired = value.value("paired", device.paired);
    device.deviceName = value.value("deviceName", device.deviceName);
    if (value.contains("controllerAddress")) {
        device.controllerAddress = ParseMacAddress(value.value("controllerAddress", std::string()));
    }
    if (value.contains("hostAddress")) {
        device.hostAddress = ParseMacAddress(value.value("hostAddress", std::string()));
    }
    return device;
}

/// Coarse UI % from reported pack mV (not coulomb-count SoC; load / charge / temperature skew terminal voltage).
///
/// Joy-Con 2 (Nintendo Japan official support specs): rated voltage 3.89 V, 500 mAh, 1.95 Wh
/// (3.89 V × 0.5 Ah ≈ 1.945 Wh). The linear map below is only a rough visual guide, not OEM SoC.
/// Full-charge reading from BLE @0x1F is often well below 4.2 V; field sample: ~3702 mV when full.
int ApproxBatteryPercentFromMv(uint16_t mv) {
    constexpr int kMvEmpty = 3200;
    constexpr int kMvFull = 3702;
    if (mv <= kMvEmpty) {
        return 0;
    }
    if (mv >= kMvFull) {
        return 100;
    }
    const int span = kMvFull - kMvEmpty;
    return static_cast<int>((static_cast<long>(mv - kMvEmpty) * 100L + span / 2) / span);
}

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
        { "distanceThreshold", static_cast<int>(settings.distanceThreshold) },
        { "opticalTiltScroll", settings.opticalTiltScroll },
        { "opticalTiltBlock", settings.opticalTiltBlock },
        { "accelFlatMin", settings.accelFlatMinAbs },
        { "tiltScrollSensitivity", settings.tiltScrollSensitivity }
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
    settings.opticalTiltScroll = value.value("opticalTiltScroll", settings.opticalTiltScroll);
    settings.opticalTiltBlock = value.value("opticalTiltBlock", settings.opticalTiltBlock);
    if (settings.opticalTiltBlock && settings.opticalTiltScroll) {
        settings.opticalTiltScroll = false;
    }
    settings.accelFlatMinAbs = std::clamp(
        value.value("accelFlatMin", settings.accelFlatMinAbs),
        500,
        8000);
    settings.tiltScrollSensitivity = std::clamp(
        value.value("tiltScrollSensitivity", settings.tiltScrollSensitivity),
        0.005,
        2.0);

    const int distanceIn = value.value("distanceThreshold", static_cast<int>(settings.distanceThreshold));
    if (distanceIn >= 0 && distanceIn <= 12) {
        settings.distanceThreshold = static_cast<uint16_t>(std::clamp(80 + distanceIn * 35, 50, 650));
    } else {
        settings.distanceThreshold = static_cast<uint16_t>(std::clamp(distanceIn, 50, 4095));
    }
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
    json rawBytes = json::array();
    for (uint8_t b : state.lastRawPacket) {
        rawBytes.push_back(b);
    }

    json root{
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
        { "stickRY", state.decoded.rightStick.y },
        { "batteryVoltageMv", state.decoded.batteryVoltageMv },
        { "batteryPercentApprox",
            state.decoded.batteryVoltageMv > 0 ? json(ApproxBatteryPercentFromMv(state.decoded.batteryVoltageMv)) : json(nullptr) },
        { "batteryCurrentRaw", state.decoded.batteryCurrentRaw },
        { "batteryCurrent_mA", static_cast<double>(state.decoded.batteryCurrentRaw) / 100.0 },
        { "magnetometerX", state.decoded.magnetometerX },
        { "magnetometerY", state.decoded.magnetometerY },
        { "magnetometerZ", state.decoded.magnetometerZ },
        { "accelX", state.decoded.motion.accelX },
        { "accelY", state.decoded.motion.accelY },
        { "accelZ", state.decoded.motion.accelZ },
        { "gyroX", state.decoded.motion.gyroX },
        { "gyroY", state.decoded.motion.gyroY },
        { "gyroZ", state.decoded.motion.gyroZ },
        { "temperatureValid", state.decoded.temperatureValid },
        { "temperatureCelsius", state.decoded.temperatureValid ? json(state.decoded.temperatureCelsius) : json(nullptr) },
        { "temperatureRaw", state.decoded.temperatureRaw },
        { "temperatureSecondaryRaw", state.decoded.temperatureSecondaryRaw },
        { "chargerBits67", state.decoded.chargerBits67 },
        { "chargerCableConnected", state.decoded.chargerCableConnected },
        { "raw", std::move(rawBytes) },
        { "rawLength", state.lastRawPacket.size() }
    };
    return root;
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
        } },
        { "pairing", {
            { "autoConnect", config.pairing.autoConnect },
            { "left", PairedDeviceToJson(config.pairing.left) },
            { "right", PairedDeviceToJson(config.pairing.right) }
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

    if (const auto pairingIt = root.find("pairing"); pairingIt != root.end() && pairingIt->is_object()) {
        config.pairing.autoConnect = pairingIt->value("autoConnect", config.pairing.autoConnect);
        config.pairing.left = PairedDeviceFromJson(pairingIt->value("left", json::object()), config.pairing.left);
        config.pairing.right = PairedDeviceFromJson(pairingIt->value("right", json::object()), config.pairing.right);
    }
}

json RuntimeSnapshotToJson(const RuntimeSnapshot& snapshot) {
    const auto pairedToJson = [&](const PairedDevice& device) {
        const bool valid = device.paired
            && device.hostAddress != 0
            && device.hostAddress == snapshot.hostBluetoothAddress;
        return json{
            { "paired", device.paired },
            { "valid", valid },
            { "deviceName", device.deviceName },
            { "controllerAddress", FormatMacAddress(device.controllerAddress) },
            { "hostAddress", FormatMacAddress(device.hostAddress) }
        };
    };

    return json{
        { "ok", true },
        { "left", ControllerStateToJson(snapshot.left) },
        { "right", ControllerStateToJson(snapshot.right) },
        { "pairing", {
            { "autoConnect", snapshot.config.pairing.autoConnect },
            { "hostAddress", FormatMacAddress(snapshot.hostBluetoothAddress) },
            { "left", pairedToJson(snapshot.config.pairing.left) },
            { "right", pairedToJson(snapshot.config.pairing.right) }
        } },
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

AppConfig BuiltinDefaultConfig() {
    AppConfig config;
    config.mouse.left = {};
    config.mouse.right = {};
    config.mapping.left = {
        { "Capture", "key_tab" },
        { "L", "mouse_left" },
        { "Minus", "key_escape" },
        { "SL", "key_q" },
        { "SR", "key_e" },
        { "ZL", "mouse_right" },
    };
    config.mapping.right = {
        { "A", "key_custom:x" },
        { "B", "key_custom:z" },
        { "C", "mouse_middle" },
        { "Home", "key_custom:s" },
        { "Plus", "key_a" },
        { "R", "mouse_left" },
        { "SL", "key_space" },
        { "SR", "key_e" },
        { "X", "key_custom:c" },
        { "Y", "key_custom:v" },
        { "ZR", "mouse_right" },
    };
    config.sticks.left = {
        .deadzone = 8000,
        .hysteresis = 1600,
        .diagonalUnlockRadius = 14000,
        .fourWayHysteresisDegrees = 12.0,
        .eightWayHysteresisDegrees = 8.0,
        .up = "key_w",
        .down = "key_s",
        .left = "key_a",
        .right = "key_d",
    };
    config.sticks.right = {
        .deadzone = 8000,
        .hysteresis = 1600,
        .diagonalUnlockRadius = 14000,
        .fourWayHysteresisDegrees = 12.0,
        .eightWayHysteresisDegrees = 8.0,
        .up = "key_up",
        .down = "key_down",
        .left = "key_left",
        .right = "key_right",
    };
    return config;
}

json ActionsCatalogJson() {
    static constexpr const char* kActionIds[] = {
        "none",
        "mouse_left",
        "mouse_right",
        "mouse_middle",
        "mouse_wheel_up",
        "mouse_wheel_down",
        "key_space",
        "key_enter",
        "key_escape",
        "key_tab",
        "key_ctrl",
        "key_shift",
        "key_alt",
        "key_up",
        "key_down",
        "key_left",
        "key_right",
        "key_w",
        "key_a",
        "key_s",
        "key_d",
        "key_q",
        "key_e",
        "key_r",
        "key_f",
        "key_1",
        "key_2",
        "key_3",
        "key_4",
        "key_5",
        "key_custom",
    };
    json arr = json::array();
    for (const char* id : kActionIds) {
        arr.push_back(id);
    }
    return arr;
}

json UiSchemaJson() {
    return json{
        { "defaults", ConfigToJson(BuiltinDefaultConfig()) },
        { "actions", ActionsCatalogJson() }
    };
}

} // namespace joycon::webgui
