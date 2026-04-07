#include "NintendoControllerTransport.h"

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <Windows.h>

#include <algorithm>
#include <condition_variable>
#include <cwctype>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace joycon::transport {
namespace {

constexpr uint16_t kNintendoManufacturerId = 1363;
const std::vector<uint8_t> kNintendoManufacturerPrefix = { 0x01, 0x00, 0x03, 0x7E };
const wchar_t* kInputReportUuid = L"ab7de9be-89fe-49ad-828f-118f09df7fd2";
const wchar_t* kWriteCommandUuid = L"649d4ac9-8eb7-4e6c-af44-1ea54fe5f005";

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

ControllerType GuessControllerType(const std::wstring& name) {
    const std::wstring lowered = ToLower(name);
    if (lowered.find(L"left") != std::wstring::npos) {
        return ControllerType::LeftJoyCon;
    }
    if (lowered.find(L"right") != std::wstring::npos) {
        return ControllerType::RightJoyCon;
    }
    return ControllerType::Unknown;
}

ControllerType ToControllerType(JoyConSide side) {
    return side == JoyConSide::Left
        ? ControllerType::LeftJoyCon
        : ControllerType::RightJoyCon;
}

bool IsJoyCon(ControllerType type) {
    return type == ControllerType::LeftJoyCon || type == ControllerType::RightJoyCon;
}

auto GetGattServicesWithRetry(const BluetoothLEDevice& device) {
    constexpr int kMaxAttempts = 5;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        auto result = device.GetGattServicesAsync().get();
        if (result.Status() == GattCommunicationStatus::Success) {
            return result;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    return device.GetGattServicesAsync().get();
}

void SendGenericCommand(
    const GattCharacteristic& characteristic,
    uint8_t commandId,
    uint8_t subCommandId,
    const std::vector<uint8_t>& data) {

    if (!characteristic) {
        return;
    }

    DataWriter writer;
    writer.WriteByte(commandId);
    writer.WriteByte(0x91);
    writer.WriteByte(0x01);
    writer.WriteByte(subCommandId);
    writer.WriteByte(0x00);
    writer.WriteByte(static_cast<uint8_t>(data.size()));
    writer.WriteByte(0x00);
    writer.WriteByte(0x00);

    for (uint8_t byte : data) {
        writer.WriteByte(byte);
    }

    IBuffer buffer = writer.DetachBuffer();
    characteristic.WriteValueAsync(buffer, GattWriteOption::WriteWithoutResponse).get();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

} // namespace

struct ControllerConnection::State {
    BluetoothLEDevice device{ nullptr };
    GattCharacteristic inputChar{ nullptr };
    GattCharacteristic writeChar{ nullptr };
    ControllerInfo info{};
    mutable std::mutex callbackMutex;
    RawPacketCallback callback;
    ConnectionStatusCallback connectionStatusCallback;
    event_token valueChangedToken{};
    bool hasValueChangedToken = false;
    event_token connectionStatusChangedToken{};
    bool hasConnectionStatusChangedToken = false;

    ~State() {
        if (hasValueChangedToken && inputChar) {
            inputChar.ValueChanged(valueChangedToken);
        }
        if (hasConnectionStatusChangedToken && device) {
            device.ConnectionStatusChanged(connectionStatusChangedToken);
        }
    }
};

ControllerConnection::ControllerConnection() = default;

ControllerConnection::ControllerConnection(std::shared_ptr<State> state)
    : state_(std::move(state)) {}

ControllerConnection::~ControllerConnection() = default;

ControllerConnection::ControllerConnection(ControllerConnection&& other) noexcept
    : state_(std::move(other.state_)) {}

ControllerConnection& ControllerConnection::operator=(ControllerConnection&& other) noexcept {
    if (this != &other) {
        state_ = std::move(other.state_);
    }
    return *this;
}

bool ControllerConnection::IsValid() const {
    return state_ && state_->device && state_->inputChar;
}

ControllerInfo ControllerConnection::Info() const {
    if (!state_) {
        return {};
    }
    return state_->info;
}

ControllerType ControllerConnection::Type() const {
    return Info().type;
}

bool ControllerConnection::IsConnected() const {
    return state_ &&
        state_->device &&
        state_->device.ConnectionStatus() == BluetoothConnectionStatus::Connected;
}

void ControllerConnection::Configure(const DeviceConfiguration& configuration) const {
    if (configuration.sendDefaultInitSequence) {
        SendDefaultInitSequence();
    }
    if (configuration.setPlayerLights) {
        SetPlayerLights(configuration.playerLightPattern);
    }
    if (configuration.playConnectionRumble) {
        EmitDefaultRumble();
    }
}

void ControllerConnection::SendDefaultInitSequence() const {
    if (!state_ || !state_->writeChar) {
        return;
    }

    const std::vector<std::vector<uint8_t>> commands = {
        { 0x0C, 0x91, 0x01, 0x02, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 },
        { 0x0C, 0x91, 0x01, 0x04, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 }
    };

    for (const auto& command : commands) {
        DataWriter writer;
        writer.WriteBytes(command);
        IBuffer buffer = writer.DetachBuffer();
        state_->writeChar.WriteValueAsync(buffer, GattWriteOption::WriteWithoutResponse).get();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void ControllerConnection::SetPlayerLights(uint8_t pattern) const {
    if (!state_) {
        return;
    }

    std::vector<uint8_t> data(8, 0x00);
    data[0] = pattern;
    SendGenericCommand(state_->writeChar, 0x09, 0x07, data);
}

void ControllerConnection::EmitDefaultRumble() const {
    if (!state_) {
        return;
    }

    std::vector<uint8_t> data(8, 0x00);
    data[0] = 0x04;
    SendGenericCommand(state_->writeChar, 0x0A, 0x02, data);
}

bool ControllerConnection::StartInputStream(const RawPacketCallback& callback) const {
    if (!state_ || !state_->inputChar) {
        return false;
    }

    {
        std::scoped_lock lock(state_->callbackMutex);
        state_->callback = callback;
    }

    if (state_->hasValueChangedToken) {
        state_->inputChar.ValueChanged(state_->valueChangedToken);
        state_->hasValueChangedToken = false;
    }

    std::weak_ptr<State> weakState = state_;
    state_->valueChangedToken = state_->inputChar.ValueChanged(
        [weakState](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
            auto state = weakState.lock();
            if (!state) {
                return;
            }

            auto reader = DataReader::FromBuffer(args.CharacteristicValue());
            RawInputPacket packet;
            packet.data.resize(reader.UnconsumedBufferLength());
            reader.ReadBytes(packet.data);
            packet.timestampMs = GetTickCount64();

            RawPacketCallback callbackCopy;
            {
                std::scoped_lock lock(state->callbackMutex);
                callbackCopy = state->callback;
            }

            if (callbackCopy) {
                callbackCopy(packet);
            }
        });
    state_->hasValueChangedToken = true;

    const auto status = state_->inputChar
        .WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify)
        .get();

    return status == GattCommunicationStatus::Success;
}

void ControllerConnection::StopInputStream() const {
    if (!state_ || !state_->inputChar || !state_->hasValueChangedToken) {
        return;
    }

    state_->inputChar.ValueChanged(state_->valueChangedToken);
    state_->hasValueChangedToken = false;
    state_->inputChar.WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue::None).get();
}

void ControllerConnection::SetConnectionStatusCallback(const ConnectionStatusCallback& callback) const {
    if (!state_) {
        return;
    }

    std::scoped_lock lock(state_->callbackMutex);
    state_->connectionStatusCallback = callback;
}

ControllerConnection ConnectMatchingControllerImpl(
    std::wstring_view prompt,
    const ConnectionOptions& options,
    const std::function<bool(const ControllerInfo&)>& matcher) {

    std::wcout << prompt << L"\n";

    const auto deadline = std::chrono::steady_clock::now() + options.timeout;

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            throw std::runtime_error("Timed out waiting for a matching Nintendo controller.");
        }

        ConnectionOptions attemptOptions = options;
        attemptOptions.timeout = std::chrono::duration_cast<std::chrono::seconds>(deadline - now);
        if (attemptOptions.timeout <= std::chrono::seconds::zero()) {
            attemptOptions.timeout = std::chrono::seconds(1);
        }

    BluetoothLEDevice device{ nullptr };
    bool connected = false;

    BluetoothLEAdvertisementWatcher watcher;
    std::mutex mutex;
    std::condition_variable cv;

    watcher.Received([&](auto const&, auto const& args) {
        std::unique_lock lock(mutex);
        if (connected) {
            return;
        }

        const auto manufacturerData = args.Advertisement().ManufacturerData();
        for (uint32_t i = 0; i < manufacturerData.Size(); ++i) {
            const auto section = manufacturerData.GetAt(i);
            if (section.CompanyId() != kNintendoManufacturerId) {
                continue;
            }

            auto reader = DataReader::FromBuffer(section.Data());
            std::vector<uint8_t> data(reader.UnconsumedBufferLength());
            reader.ReadBytes(data);

            if (data.size() < kNintendoManufacturerPrefix.size()) {
                continue;
            }

            if (!std::equal(
                    kNintendoManufacturerPrefix.begin(),
                    kNintendoManufacturerPrefix.end(),
                    data.begin())) {
                continue;
            }

            device = BluetoothLEDevice::FromBluetoothAddressAsync(args.BluetoothAddress()).get();
            if (!device) {
                return;
            }

            connected = true;
            watcher.Stop();
            cv.notify_one();
            return;
        }
    });

    watcher.ScanningMode(BluetoothLEScanningMode::Active);
    watcher.Start();

    {
        std::unique_lock lock(mutex);
        if (!cv.wait_for(lock, attemptOptions.timeout, [&]() { return connected; })) {
            watcher.Stop();
            throw std::runtime_error("Timed out waiting for a Nintendo controller.");
        }
    }

    auto servicesResult = GetGattServicesWithRetry(device);
    if (servicesResult.Status() != GattCommunicationStatus::Success) {
        throw std::runtime_error("Failed to enumerate GATT services.");
    }

    auto state = std::shared_ptr<ControllerConnection::State>(new ControllerConnection::State());
    state->device = device;
    state->info.name = device.Name().c_str();
    state->info.bluetoothAddress = device.BluetoothAddress();
    state->info.type = GuessControllerType(state->info.name);

    const auto services = servicesResult.Services();
    for (uint32_t i = 0; i < services.Size(); ++i) {
        const auto service = services.GetAt(i);
        auto charsResult = service.GetCharacteristicsAsync().get();
        if (charsResult.Status() != GattCommunicationStatus::Success) {
            continue;
        }

        const auto characteristics = charsResult.Characteristics();
        for (uint32_t j = 0; j < characteristics.Size(); ++j) {
            const auto characteristic = characteristics.GetAt(j);
            if (characteristic.Uuid() == guid(kInputReportUuid)) {
                state->inputChar = characteristic;
            } else if (characteristic.Uuid() == guid(kWriteCommandUuid)) {
                state->writeChar = characteristic;
            }
        }
    }

    if (!state->inputChar) {
        throw std::runtime_error("Input report characteristic was not found.");
    }

    std::weak_ptr<ControllerConnection::State> weakState = state;
    state->connectionStatusChangedToken = state->device.ConnectionStatusChanged(
        [weakState](BluetoothLEDevice const& sender, auto const&) {
            auto lockedState = weakState.lock();
            if (!lockedState) {
                return;
            }

            ConnectionStatusCallback callbackCopy;
            {
                std::scoped_lock lock(lockedState->callbackMutex);
                callbackCopy = lockedState->connectionStatusCallback;
            }

            if (callbackCopy) {
                const auto status = sender.ConnectionStatus() == BluetoothConnectionStatus::Connected
                    ? ControllerConnectionStatus::Connected
                    : ControllerConnectionStatus::Disconnected;
                callbackCopy(status);
            }
        });
    state->hasConnectionStatusChangedToken = true;

    if (attemptOptions.requestLowLatency) {
        try {
            auto params = BluetoothLEPreferredConnectionParameters::ThroughputOptimized();
            state->device.RequestPreferredConnectionParameters(params);
        } catch (...) {
            // Keep the transport layer resilient. Callers can still receive packets.
        }
    }

        if (matcher(state->info)) {
            return ControllerConnection(state);
        }
    }
}

ControllerConnection ConnectToFirstController(
    std::wstring_view prompt,
    const ConnectionOptions& options) {
    return ConnectMatchingControllerImpl(prompt, options, [](const ControllerInfo&) {
        return true;
    });
}

ControllerConnection ConnectJoyCon(
    JoyConSide side,
    std::wstring_view prompt,
    const ConnectionOptions& options) {
    const ControllerType expectedType = ToControllerType(side);
    return ConnectMatchingControllerImpl(prompt, options, [expectedType](const ControllerInfo& info) {
        if (info.type == ControllerType::Unknown) {
            return true;
        }

        return IsJoyCon(info.type) && info.type == expectedType;
    });
}

} // namespace joycon::transport
