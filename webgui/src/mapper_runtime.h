#pragma once

#include "JoyconSdk.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace joycon::webgui {

enum class ConnectionStatus {
    Disconnected,
    Connecting,
    Connected,
    Error
};

struct MouseSettings {
    bool enabled = true;
    double baseSensitivity = 0.10;
    double acceleration = 0.040;
    double exponent = 0.50;
    double maxGain = 2.50;
    uint8_t distanceThreshold = 0x0C;
};

struct MouseConfig {
    MouseSettings left;
    MouseSettings right;
};

struct MappingConfig {
    std::map<std::string, std::string> left;
    std::map<std::string, std::string> right;
};

struct StickMapping {
    int deadzone = 8000;
    int hysteresis = 1600;
    int diagonalUnlockRadius = 14000;
    double fourWayHysteresisDegrees = 12.0;
    double eightWayHysteresisDegrees = 8.0;
    std::string up = "none";
    std::string down = "none";
    std::string left = "none";
    std::string right = "none";
};

struct StickSettings {
    StickMapping left;
    StickMapping right;
};

struct ServerSettings {
    uint16_t port = 17777;
};

struct AppConfig {
    MouseConfig mouse;
    MappingConfig mapping;
    StickSettings sticks;
    ServerSettings server;
};

struct ControllerStateSnapshot {
    JoyConSide side = JoyConSide::Left;
    ConnectionStatus status = ConnectionStatus::Disconnected;
    std::string error;
    std::wstring deviceName;
    uint64_t packetCount = 0;
    double averageIntervalMs = 0.0;
    double rateHz = 0.0;
    uint32_t buttonBits = 0;
    protocol::DecodedInputState decoded{};
};

struct MouseStatsSnapshot {
    uint64_t movedPackets = 0;
    uint64_t injectedMoves = 0;
    uint64_t gatedPackets = 0;
    double averageDispatchUs = 0.0;
    double maxDispatchUs = 0.0;
    uint8_t lastDistance = 0xFF;
    uint8_t minDistance = 0xFF;
    uint8_t maxDistance = 0x00;
};

struct RuntimeSnapshot {
    AppConfig config;
    ControllerStateSnapshot left;
    ControllerStateSnapshot right;
    MouseStatsSnapshot mouseStats;
};

class MapperRuntime {
public:
    MapperRuntime();
    ~MapperRuntime();

    MapperRuntime(const MapperRuntime&) = delete;
    MapperRuntime& operator=(const MapperRuntime&) = delete;

    void ApplyConfig(const AppConfig& config);
    AppConfig CurrentConfig() const;

    bool ConnectSide(JoyConSide side, std::string& errorMessage);
    void DisconnectSide(JoyConSide side);

    RuntimeSnapshot Snapshot() const;

private:
    struct MouseAccumulator {
        bool hasOpticalSample = false;
        int16_t lastOpticalX = 0;
        int16_t lastOpticalY = 0;
        double pendingX = 0.0;
        double pendingY = 0.0;
        std::chrono::steady_clock::time_point lastEnqueueTime{};
    };

    struct MouseStats {
        uint64_t movedPackets = 0;
        uint64_t injectedMoves = 0;
        uint64_t gatedPackets = 0;
        uint64_t dispatchSamples = 0;
        double dispatchSumUs = 0.0;
        double maxDispatchUs = 0.0;
        uint8_t lastDistance = 0xFF;
        uint8_t minDistance = 0xFF;
        uint8_t maxDistance = 0x00;
    };

    struct KeyRepeatState {
        uint16_t virtualKey = 0;
        std::chrono::steady_clock::time_point nextRepeatTime{};
    };

    struct KeyRepeatSettings {
        std::chrono::milliseconds initialDelay{ 500 };
        std::chrono::milliseconds repeatInterval{ 40 };
    };

    struct ControllerSlot {
        JoyConSide side = JoyConSide::Left;
        ConnectionStatus status = ConnectionStatus::Disconnected;
        std::string error;
        std::unique_ptr<transport::ControllerConnection> connection;
        std::wstring deviceName;
        protocol::DecodedInputState latestState{};
        uint32_t latestButtonBits = 0;
        uint64_t packetCount = 0;
        uint64_t intervalSamples = 0;
        double intervalSumUs = 0.0;
        std::chrono::steady_clock::time_point lastPacketTime{};
        std::unordered_map<std::string, bool> previousInputs;
        int lockedFourWayDirection = -1;
        int activeEightWaySector = -1;
        bool diagonalModeActive = false;
        MouseAccumulator mouseAccumulator{};
    };

    void StartMouseOutputThread();
    void StopMouseOutputThread();
    void StartKeyRepeatThread();
    void StopKeyRepeatThread();
    void HandleDecodedState(JoyConSide side, const protocol::DecodedInputState& state);
    void HandleConnectionStatusEvent(JoyConSide side, transport::ControllerConnectionStatus status);
    void UpdateMouseFromJoyCon(ControllerSlot& slot, const std::chrono::steady_clock::time_point& callbackTime);
    void UpdateMappedButtons(ControllerSlot& slot);
    void UpdateMappedStick(ControllerSlot& slot);
    void ReleaseMappedInputs(ControllerSlot& slot);
    void UpdateMappedInputState(ControllerSlot& slot, const std::string& inputId, bool pressed, const std::string& action);

    static uint32_t ExtractButtonBits(JoyConSide side, const protocol::DecodedInputState& state);
    static bool IsOpticalMouseActive(const MouseSettings& settings, uint8_t opticalDistance);
    static int16_t WrapOpticalDelta(int16_t current, int16_t previous);
    static double ComputeAccelerationGain(const MouseSettings& settings, double speed);
    static void InjectMappedAction(const std::string& action, bool pressed);
    static uint16_t ResolveKeyboardVirtualKey(const std::string& action);
    static bool ShouldAutoRepeatVirtualKey(uint16_t virtualKey);
    static KeyRepeatSettings QueryKeyRepeatSettings();
    static ControllerStateSnapshot SnapshotFromSlot(const ControllerSlot& slot);

    mutable std::mutex mutex_;
    AppConfig config_;
    ControllerSlot leftSlot_;
    ControllerSlot rightSlot_;
    MouseStats mouseStats_;
    std::condition_variable mouseCondition_;
    std::thread mouseOutputThread_;
    std::unordered_map<std::string, KeyRepeatState> activeKeyRepeats_;
    KeyRepeatSettings keyRepeatSettings_{};
    std::condition_variable keyRepeatCondition_;
    std::thread keyRepeatThread_;
    bool running_ = false;
};

} // namespace joycon::webgui
