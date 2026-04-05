#pragma once

#include "JoyconTypes.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace joycon::transport {

struct RawInputPacket {
    std::vector<uint8_t> data;
    uint64_t timestampMs = 0;
};

struct ControllerInfo {
    std::wstring name;
    uint64_t bluetoothAddress = 0;
    ControllerType type = ControllerType::Unknown;
};

using RawPacketCallback = std::function<void(const RawInputPacket&)>;
enum class ControllerConnectionStatus {
    Connected,
    Disconnected,
};
using ConnectionStatusCallback = std::function<void(ControllerConnectionStatus)>;

struct ConnectionOptions {
    std::chrono::seconds timeout = std::chrono::seconds(30);
    bool requestLowLatency = true;
};

struct DeviceConfiguration {
    bool sendDefaultInitSequence = true;
    bool setPlayerLights = false;
    uint8_t playerLightPattern = 0x01;
    bool playConnectionRumble = false;
};

class ControllerConnection {
public:
    ControllerConnection();
    ~ControllerConnection();

    ControllerConnection(const ControllerConnection&) = delete;
    ControllerConnection& operator=(const ControllerConnection&) = delete;
    ControllerConnection(ControllerConnection&& other) noexcept;
    ControllerConnection& operator=(ControllerConnection&& other) noexcept;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] ControllerInfo Info() const;
    [[nodiscard]] ControllerType Type() const;

    void Configure(const DeviceConfiguration& configuration) const;
    void SendDefaultInitSequence() const;
    void SetPlayerLights(uint8_t pattern) const;
    void EmitDefaultRumble() const;

    bool StartInputStream(const RawPacketCallback& callback) const;
    void StopInputStream() const;
    void SetConnectionStatusCallback(const ConnectionStatusCallback& callback) const;
    [[nodiscard]] bool IsConnected() const;

private:
    struct State;

    explicit ControllerConnection(std::shared_ptr<State> state);

    std::shared_ptr<State> state_;

    friend ControllerConnection ConnectToFirstController(
        std::wstring_view prompt,
        const ConnectionOptions& options);
    friend ControllerConnection ConnectMatchingControllerImpl(
        std::wstring_view prompt,
        const ConnectionOptions& options,
        const std::function<bool(const ControllerInfo&)>& matcher);
};

ControllerConnection ConnectToFirstController(
    std::wstring_view prompt,
    const ConnectionOptions& options = {});

ControllerConnection ConnectJoyCon(
    JoyConSide side,
    std::wstring_view prompt,
    const ConnectionOptions& options = {});

} // namespace joycon::transport
