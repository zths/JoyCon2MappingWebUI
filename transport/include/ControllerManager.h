#pragma once

#include "JoyconTypes.h"
#include "NintendoControllerTransport.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace joycon::transport {

enum class ControllerEventType {
    Connected,        ///< A controller connected and is streaming. `info` is valid.
    Disconnected,     ///< A controller disconnected.
    Input,            ///< A raw input report arrived. `packet` is valid.
    Paired,           ///< OOB pairing succeeded. `pairing` is valid.
    PairFailed,       ///< OOB pairing failed. `message` describes why.
    DuplicateIgnored, ///< A second controller for an already-claimed side was ignored. `side`/`message` set.
    Error,            ///< A connection attempt or operation failed. `message` set.
};

struct ControllerEvent {
    ControllerEventType type = ControllerEventType::Error;
    JoyConSide side = JoyConSide::Left;
    ControllerInfo info{};      ///< valid for Connected
    RawInputPacket packet{};    ///< valid for Input
    PairingResult pairing{};    ///< valid for Paired
    std::string message;        ///< diagnostic/error text
};

using ControllerManagerCallback = std::function<void(const ControllerEvent&)>;

/// Event-driven manager for up to two Joy-Con 2 controllers (one per side).
///
/// Owns a persistent BLE advertisement watcher plus internal worker threads, and
/// drives connect / reconnect / pair / input entirely through callbacks. All BLE
/// work happens on internal threads; events are delivered SERIALLY on a single
/// dedicated dispatch thread, so the callback never runs on a BLE/WinRT thread
/// and slow callbacks cannot stall BLE processing.
///
/// The manager is side-aware (left/right resolved from the advertised Product
/// ID) but the connect API is side-agnostic: enable "accepting" and whatever
/// fits a free side connects, reported back via a Connected event.
class ControllerManager {
public:
    ControllerManager();
    ~ControllerManager();

    ControllerManager(const ControllerManager&) = delete;
    ControllerManager& operator=(const ControllerManager&) = delete;

    /// Begin watching for controllers and delivering events. `onEvent` is always
    /// invoked on the manager's dedicated dispatch thread.
    void Start(ControllerManagerCallback onEvent);
    void Stop();

    /// When true, a newly discovered Joy-Con that maps to a currently free side
    /// will be connected automatically (used for first-time connect / pairing).
    void SetAccepting(bool accepting);

    /// Remember a paired controller address for a side (0 clears). A side with a
    /// known address auto-reconnects to exactly that device when it re-advertises
    /// (e.g. on a button press), independent of the accepting flag.
    void SetKnownDevice(JoyConSide side, uint64_t controllerAddress);

    /// Request OOB pairing of the currently-connected controller on `side`.
    /// Emits Paired or PairFailed.
    void RequestPair(JoyConSide side);

    /// Drop the connection on `side` (if any). Emits Disconnected.
    void Disconnect(JoyConSide side);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace joycon::transport
