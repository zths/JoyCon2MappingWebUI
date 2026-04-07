#pragma once

#include "JoyconSdk.h"
#include "output_sink.h"

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
    /// Full u16 LE optical distance @0x16: active when value <= this (desk ~100–200, air ~3000).
    uint16_t distanceThreshold = 480;
    /// When set, if accel does not match “optical sensor down” pose, map optical delta Y to scroll wheel.
    bool opticalTiltScroll = false;
    /// When set, if not flat mouse pose, ignore all optical input (no move, no scroll). Mutually exclusive with opticalTiltScroll.
    bool opticalTiltBlock = false;
    /// Min |accel| on the “down” axis (raw s16); flat mouse = that axis strictly larger than the other two; L: X<0, R: X>0.
    int accelFlatMinAbs = 2800;
    /// Scales wrapped optical delta Y into scroll wheel notches (with internal notch threshold).
    double tiltScrollSensitivity = 0.08;
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
    std::vector<uint8_t> lastRawPacket{};
};

struct MouseStatsSnapshot {
    uint64_t movedPackets = 0;
    uint64_t injectedMoves = 0;
    uint64_t gatedPackets = 0;
    double averageDispatchUs = 0.0;
    double maxDispatchUs = 0.0;
    uint16_t lastDistance = 0xFFFF;
    uint16_t minDistance = 0xFFFF;
    uint16_t maxDistance = 0x0000;
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
    explicit MapperRuntime(std::shared_ptr<IOutputSink> outputSink);
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
        double pendingScroll = 0.0;
        bool lastOpticalScrollMode = false;
        std::chrono::steady_clock::time_point lastEnqueueTime{};
    };

    struct MouseStats {
        uint64_t movedPackets = 0;
        uint64_t injectedMoves = 0;
        uint64_t gatedPackets = 0;
        uint64_t dispatchSamples = 0;
        double dispatchSumUs = 0.0;
        double maxDispatchUs = 0.0;
        uint16_t lastDistance = 0xFFFF;
        uint16_t minDistance = 0xFFFF;
        uint16_t maxDistance = 0x0000;
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
        std::vector<uint8_t> lastRawPacket{};
        std::unordered_map<std::string, bool> previousInputs;
        int lockedFourWayDirection = -1;
        int activeEightWaySector = -1;
        bool diagonalModeActive = false;
        MouseAccumulator mouseAccumulator{};
    };

    void StartMouseOutputThread();
    void StopMouseOutputThread();
    void HandleDecodedState(JoyConSide side, const protocol::DecodedInputState& state, const std::vector<uint8_t>& rawPacket);
    void HandleConnectionStatusEvent(JoyConSide side, transport::ControllerConnectionStatus status);
    void UpdateMouseFromJoyCon(ControllerSlot& slot, const std::chrono::steady_clock::time_point& callbackTime);
    void UpdateMappedButtons(ControllerSlot& slot);
    void UpdateMappedStick(ControllerSlot& slot);
    void ReleaseMappedInputs(ControllerSlot& slot);
    void UpdateMappedInputState(ControllerSlot& slot, const std::string& inputId, bool pressed, const std::string& action);

    static uint32_t ExtractButtonBits(JoyConSide side, const protocol::DecodedInputState& state);
    static bool IsOpticalMouseActive(const MouseSettings& settings, uint16_t opticalDistance);
    static bool IsOpticalFlatMousePose(JoyConSide side, const joycon::ImuSample& imu, int accelFlatMinAbs);
    static int16_t WrapOpticalDelta(int16_t current, int16_t previous);
    static double ComputeAccelerationGain(const MouseSettings& settings, double speed);
    void InjectMappedAction(const std::string& logicalInputId, const std::string& action, bool pressed);
    static uint16_t ResolveKeyboardVirtualKey(const std::string& action);
    static bool ShouldAutoRepeatVirtualKey(uint16_t virtualKey);
    static ControllerStateSnapshot SnapshotFromSlot(const ControllerSlot& slot);

    mutable std::mutex mutex_;
    std::shared_ptr<IOutputSink> outputSink_;
    AppConfig config_;
    ControllerSlot leftSlot_;
    ControllerSlot rightSlot_;
    MouseStats mouseStats_;
    std::condition_variable mouseCondition_;
    std::thread mouseOutputThread_;
    bool running_ = false;
};

} // namespace joycon::webgui
