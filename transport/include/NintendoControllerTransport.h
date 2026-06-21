#pragma once

#include "JoyconTypes.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
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

/// Result of a Nintendo OOB pairing operation (command 0x15 + 0x03/07,09).
/// The controller persists the host address + LTK in its own pairing table;
/// the transport layer performs no host-side persistence. Callers decide what
/// (if anything) to store, e.g. `hostAddress` to later detect an invalidated
/// pairing when the local Bluetooth radio changes.
struct PairingResult {
    uint64_t hostAddress = 0;          ///< host (PC) BT address registered into the controller
    uint64_t controllerAddress = 0;    ///< controller BT address reported during the exchange
    std::array<uint8_t, 16> ltk{};     ///< derived link key (A1 ^ B1)
    bool deviceKeyMatchedKnown = false; ///< controller public key B1 matched the known fixed value
    bool challengeVerified = false;     ///< AES128-ECB(LTK, challenge) matched the controller response
};

/// Map a Switch 2 controller USB/BLE Product ID to a controller type. Available
/// during BLE discovery via the advertised product id; no connection needed.
/// 0x2066/0x2070 = Joy-Con 2 (R), 0x2067/0x2071 = Joy-Con 2 (L).
ControllerType ControllerTypeFromProductId(uint16_t productId);

/// Read the local (host) Bluetooth adapter address. Returns std::nullopt when
/// no default adapter is available. Stateless: the caller is responsible for any
/// comparison/persistence (e.g. detecting that a stored pairing became invalid).
std::optional<uint64_t> GetHostBluetoothAddress();

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

    /// Run Nintendo's Out-Of-Band pairing so this host (PC) is stored in the
    /// controller's pairing table, enabling button-wake reconnection to the PC.
    /// The controller must already be connected (typically via open/sync mode).
    /// Returns false and sets `error` on failure. No host-side state is persisted.
    bool PairToHost(uint64_t hostAddress, PairingResult& result, std::string& error) const;

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
        ControllerType expectedAdvertisedType,
        const std::function<bool(const ControllerInfo&)>& matcher);
    friend ControllerConnection ConnectByAddress(
        uint64_t controllerAddress,
        ControllerType expectedType,
        const ConnectionOptions& options);
};

ControllerConnection ConnectToFirstController(
    std::wstring_view prompt,
    const ConnectionOptions& options = {});

ControllerConnection ConnectJoyCon(
    JoyConSide side,
    std::wstring_view prompt,
    const ConnectionOptions& options = {});

/// Connect directly to a known controller Bluetooth address (no scanning) and
/// build a ready-to-use connection. `expectedType` (from the advertised Product
/// ID) sets the controller type; Unknown falls back to the device name. Throws
/// std::runtime_error on failure. Used by ControllerManager for targeted
/// (re)connection.
ControllerConnection ConnectByAddress(
    uint64_t controllerAddress,
    ControllerType expectedType,
    const ConnectionOptions& options = {});

} // namespace joycon::transport
