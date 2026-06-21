#include "ControllerManager.h"

#include <winrt/base.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

using namespace winrt;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Storage::Streams;

namespace joycon::transport {
namespace {

constexpr uint16_t kNintendoCompanyId = 0x0553;
const std::array<uint8_t, 4> kManufacturerPrefix = { 0x01, 0x00, 0x03, 0x7E };

std::string FormatMacAddress(uint64_t address) {
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

} // namespace

struct ControllerManager::Impl {
    ControllerManagerCallback callback;
    std::atomic<bool> running{ false };

    // Event delivery (single dedicated dispatch thread; callback never runs on a BLE thread).
    std::mutex eventMutex;
    std::condition_variable eventCv;
    std::deque<ControllerEvent> events;
    std::thread dispatchThread;

    // Blocking BLE operations (connect / pair / disconnect) run serially here.
    std::mutex taskMutex;
    std::condition_variable taskCv;
    std::deque<std::function<void()>> tasks;
    std::thread workerThread;

    struct Slot {
        std::unique_ptr<ControllerConnection> connection;
        bool busy = false;                  ///< connecting or connected (guards duplicate attempts)
        uint64_t knownAddress = 0;          ///< if set, only auto-reconnect to this exact device
        uint64_t activeAddress = 0;         ///< address currently claiming this side
        uint64_t lastNotifiedDuplicate = 0; ///< dedup for DuplicateIgnored notifications
    };

    std::mutex stateMutex;
    bool accepting = false;
    Slot left;
    Slot right;

    BluetoothLEAdvertisementWatcher watcher{ nullptr };
    event_token watcherToken{};
    bool hasWatcher = false;

    Slot& SlotFor(JoyConSide side) { return side == JoyConSide::Left ? left : right; }

    void EnqueueEvent(ControllerEvent event) {
        {
            std::scoped_lock lock(eventMutex);
            events.push_back(std::move(event));
        }
        eventCv.notify_one();
    }

    void EnqueueTask(std::function<void()> task) {
        {
            std::scoped_lock lock(taskMutex);
            tasks.push_back(std::move(task));
        }
        taskCv.notify_one();
    }

    void EmitSimple(ControllerEventType type, JoyConSide side, std::string message = {}) {
        ControllerEvent event;
        event.type = type;
        event.side = side;
        event.message = std::move(message);
        EnqueueEvent(std::move(event));
    }

    void DispatchLoop() {
        while (true) {
            ControllerEvent event;
            {
                std::unique_lock lock(eventMutex);
                eventCv.wait(lock, [&] { return !running || !events.empty(); });
                if (!running && events.empty()) {
                    break;
                }
                event = std::move(events.front());
                events.pop_front();
            }
            if (callback) {
                callback(event);
            }
        }
    }

    void WorkerLoop() {
        try {
            init_apartment();
        } catch (...) {
        }
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock lock(taskMutex);
                taskCv.wait(lock, [&] { return !running || !tasks.empty(); });
                if (!running && tasks.empty()) {
                    break;
                }
                task = std::move(tasks.front());
                tasks.pop_front();
            }
            task();
        }
    }

    void OnAdvertisement(BluetoothLEAdvertisementReceivedEventArgs const& args) {
        const auto manufacturerData = args.Advertisement().ManufacturerData();
        for (uint32_t i = 0; i < manufacturerData.Size(); ++i) {
            const auto section = manufacturerData.GetAt(i);
            if (section.CompanyId() != kNintendoCompanyId) {
                continue;
            }

            auto reader = DataReader::FromBuffer(section.Data());
            std::vector<uint8_t> data(reader.UnconsumedBufferLength());
            if (data.size() < 7) {
                continue;
            }
            reader.ReadBytes(data);
            if (!std::equal(kManufacturerPrefix.begin(), kManufacturerPrefix.end(), data.begin())) {
                continue;
            }

            const uint16_t productId = static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8);
            const ControllerType type = ControllerTypeFromProductId(productId);
            if (type != ControllerType::LeftJoyCon && type != ControllerType::RightJoyCon) {
                continue; // only Joy-Cons are managed here
            }
            const JoyConSide side = (type == ControllerType::RightJoyCon) ? JoyConSide::Right : JoyConSide::Left;
            const uint64_t address = args.BluetoothAddress();

            bool doConnect = false;
            bool notifyDuplicate = false;
            {
                std::scoped_lock lock(stateMutex);
                Slot& slot = SlotFor(side);
                if (slot.busy) {
                    // This side is already claimed by another device. Notify once
                    // per distinct intruder so the user knows a second same-side
                    // controller was ignored.
                    if (address != slot.activeAddress && address != slot.lastNotifiedDuplicate) {
                        slot.lastNotifiedDuplicate = address;
                        notifyDuplicate = true;
                    }
                } else {
                    const bool shouldConnect = (slot.knownAddress != 0)
                        ? (address == slot.knownAddress)
                        : accepting;
                    if (shouldConnect) {
                        slot.busy = true;
                        slot.activeAddress = address;
                        slot.lastNotifiedDuplicate = 0;
                        doConnect = true;
                    }
                }
            }

            if (notifyDuplicate) {
                ControllerEvent event;
                event.type = ControllerEventType::DuplicateIgnored;
                event.side = side;
                event.message = FormatMacAddress(address);
                EnqueueEvent(std::move(event));
            }
            if (doConnect) {
                EnqueueTask([this, side, address, type] { ConnectTask(side, address, type); });
            }
            return;
        }
    }

    void ConnectTask(JoyConSide side, uint64_t address, ControllerType type) {
        try {
            ConnectionOptions options;
            options.requestLowLatency = true;
            auto connection = std::make_unique<ControllerConnection>(ConnectByAddress(address, type, options));

            connection->SetConnectionStatusCallback([this, side](ControllerConnectionStatus status) {
                if (status == ControllerConnectionStatus::Disconnected) {
                    EnqueueTask([this, side] { DisconnectTask(side, /*graceful=*/false); });
                }
            });

            const bool started = connection->StartInputStream([this, side](const RawInputPacket& packet) {
                ControllerEvent event;
                event.type = ControllerEventType::Input;
                event.side = side;
                event.packet = packet;
                EnqueueEvent(std::move(event));
            });
            if (!started) {
                throw std::runtime_error("Failed to start input stream.");
            }

            DeviceConfiguration config;
            config.sendDefaultInitSequence = true;
            config.setPlayerLights = true;
            config.playerLightPattern = (side == JoyConSide::Left) ? 0x08 : 0x02;
            config.playConnectionRumble = true;
            connection->Configure(config);

            ControllerInfo info = connection->Info();
            {
                std::scoped_lock lock(stateMutex);
                SlotFor(side).connection = std::move(connection);
            }

            ControllerEvent event;
            event.type = ControllerEventType::Connected;
            event.side = side;
            event.info = info;
            EnqueueEvent(std::move(event));
        } catch (const hresult_error& ex) {
            {
                std::scoped_lock lock(stateMutex);
                Slot& slot = SlotFor(side);
                slot.busy = false;
                slot.activeAddress = 0;
                slot.lastNotifiedDuplicate = 0;
            }
            EmitSimple(ControllerEventType::Error, side, to_string(ex.message()));
        } catch (const std::exception& ex) {
            {
                std::scoped_lock lock(stateMutex);
                Slot& slot = SlotFor(side);
                slot.busy = false;
                slot.activeAddress = 0;
                slot.lastNotifiedDuplicate = 0;
            }
            EmitSimple(ControllerEventType::Error, side, ex.what());
        }
    }

    void DisconnectTask(JoyConSide side, bool graceful) {
        std::unique_ptr<ControllerConnection> dead;
        {
            std::scoped_lock lock(stateMutex);
            Slot& slot = SlotFor(side);
            dead = std::move(slot.connection);
            slot.busy = false;
            slot.activeAddress = 0;
            slot.lastNotifiedDuplicate = 0;
        }
        if (!dead) {
            return;
        }
        if (graceful) {
            try {
                dead->StopInputStream();
            } catch (...) {
            }
        }
        dead.reset();
        EmitSimple(ControllerEventType::Disconnected, side);
    }

    void PairTask(JoyConSide side) {
        ControllerConnection* connection = nullptr;
        {
            std::scoped_lock lock(stateMutex);
            connection = SlotFor(side).connection.get();
        }
        if (!connection) {
            EmitSimple(ControllerEventType::PairFailed, side, "Controller is not connected.");
            return;
        }
        const auto host = GetHostBluetoothAddress();
        if (!host) {
            EmitSimple(ControllerEventType::PairFailed, side, "No local Bluetooth adapter is available.");
            return;
        }

        PairingResult result;
        std::string error;
        const bool ok = connection->PairToHost(*host, result, error);

        ControllerEvent event;
        event.side = side;
        if (ok) {
            event.type = ControllerEventType::Paired;
            event.pairing = result;
        } else {
            event.type = ControllerEventType::PairFailed;
            event.message = error.empty() ? "Pairing failed." : error;
        }
        EnqueueEvent(std::move(event));
    }

    void StopInternals() {
        running = false;

        if (hasWatcher) {
            try {
                watcher.Stop();
            } catch (...) {
            }
            if (watcherToken) {
                watcher.Received(watcherToken);
                watcherToken = {};
            }
            hasWatcher = false;
        }

        taskCv.notify_all();
        eventCv.notify_all();
        if (workerThread.joinable()) {
            workerThread.join();
        }
        if (dispatchThread.joinable()) {
            dispatchThread.join();
        }

        std::scoped_lock lock(stateMutex);
        for (Slot* slot : { &left, &right }) {
            if (slot->connection) {
                try {
                    slot->connection->StopInputStream();
                } catch (...) {
                }
                slot->connection.reset();
            }
            slot->busy = false;
        }
    }
};

ControllerManager::ControllerManager()
    : impl_(std::make_unique<Impl>()) {}

ControllerManager::~ControllerManager() {
    Stop();
}

void ControllerManager::Start(ControllerManagerCallback onEvent) {
    if (impl_->running) {
        return;
    }
    impl_->callback = std::move(onEvent);
    impl_->running = true;
    impl_->dispatchThread = std::thread([impl = impl_.get()] { impl->DispatchLoop(); });
    impl_->workerThread = std::thread([impl = impl_.get()] { impl->WorkerLoop(); });

    impl_->watcher = BluetoothLEAdvertisementWatcher();
    impl_->watcher.ScanningMode(BluetoothLEScanningMode::Active);
    impl_->watcherToken = impl_->watcher.Received(
        [impl = impl_.get()](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args) {
            impl->OnAdvertisement(args);
        });
    impl_->hasWatcher = true;
    impl_->watcher.Start();
}

void ControllerManager::Stop() {
    impl_->StopInternals();
}

void ControllerManager::SetAccepting(bool accepting) {
    std::scoped_lock lock(impl_->stateMutex);
    impl_->accepting = accepting;
}

void ControllerManager::SetKnownDevice(JoyConSide side, uint64_t controllerAddress) {
    std::scoped_lock lock(impl_->stateMutex);
    impl_->SlotFor(side).knownAddress = controllerAddress;
}

void ControllerManager::RequestPair(JoyConSide side) {
    impl_->EnqueueTask([impl = impl_.get(), side] { impl->PairTask(side); });
}

void ControllerManager::Disconnect(JoyConSide side) {
    impl_->EnqueueTask([impl = impl_.get(), side] { impl->DisconnectTask(side, /*graceful=*/true); });
}

} // namespace joycon::transport
