#include "NintendoControllerTransport.h"

#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cwctype>
#include <iostream>
#include <mutex>
#include <optional>
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
// Command response notification characteristics (see bluetooth_interface.md).
const wchar_t* kResponseBasicUuid = L"c765a961-d9d8-4d36-a20a-5315b111836a"; // 0x001a, all controllers
const wchar_t* kResponseExtLeftUuid = L"63a3810f-aec7-474b-9010-3d52403cb996"; // 0x001e, JoyCon (L)
const wchar_t* kResponseExtRightUuid = L"640ca58e-0e88-410c-a7f3-426faf2b690b"; // 0x001e, JoyCon (R)

// Fixed controller public key (B1) observed during OOB pairing key exchange.
constexpr std::array<uint8_t, 16> kFixedDeviceKey = {
    0x5C, 0xF6, 0xEE, 0x79, 0x2C, 0xDF, 0x05, 0xE1,
    0xBA, 0x2B, 0x63, 0x25, 0xC4, 0x1A, 0x5F, 0x10
};

/// 6-byte wire form (reverse byte-order / little-endian) of a BT address.
std::array<uint8_t, 6> AddressToWire(uint64_t address) {
    std::array<uint8_t, 6> out{};
    for (int i = 0; i < 6; ++i) {
        out[static_cast<std::size_t>(i)] = static_cast<uint8_t>((address >> (8 * i)) & 0xFF);
    }
    return out;
}

/// Reconstruct a BT address from 6 reverse-byte-order (little-endian) bytes.
uint64_t WireToAddress(const uint8_t* wire) {
    uint64_t address = 0;
    for (int i = 0; i < 6; ++i) {
        address |= static_cast<uint64_t>(wire[i]) << (8 * i);
    }
    return address;
}

void FillRandomBytes(uint8_t* data, std::size_t length) {
    if (BCryptGenRandom(nullptr, data, static_cast<ULONG>(length), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (std::size_t i = 0; i < length; ++i) {
            data[i] = static_cast<uint8_t>(std::rand() & 0xFF);
        }
    }
}

/// AES-128 ECB encrypt of a single 16-byte block.
bool AesEcbEncryptBlock(const std::array<uint8_t, 16>& key,
                        const std::array<uint8_t, 16>& input,
                        std::array<uint8_t, 16>& output) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0) {
        return false;
    }
    bool ok = false;
    do {
        if (BCryptSetProperty(alg, BCRYPT_CHAINING_MODE,
                              reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_ECB)),
                              sizeof(BCRYPT_CHAIN_MODE_ECB), 0) != 0) {
            break;
        }
        BCRYPT_KEY_HANDLE keyHandle = nullptr;
        if (BCryptGenerateSymmetricKey(alg, &keyHandle, nullptr, 0,
                                       reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(key.data())),
                                       static_cast<ULONG>(key.size()), 0) != 0) {
            break;
        }
        ULONG produced = 0;
        const NTSTATUS status = BCryptEncrypt(
            keyHandle,
            reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(input.data())), static_cast<ULONG>(input.size()),
            nullptr, nullptr, 0,
            output.data(), static_cast<ULONG>(output.size()),
            &produced, 0);
        BCryptDestroyKey(keyHandle);
        ok = (status == 0 && produced == output.size());
    } while (false);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return value;
}

/// Read the advertised Product ID from Nintendo manufacturer data. `data` is the
/// WinRT ManufacturerData().Data() payload (company id already stripped):
/// [0..3]=01 00 03 7E prefix, [3..4]=vendor (LE), [5..6]=product (LE).
uint16_t ReadAdvertisedProductId(const std::vector<uint8_t>& data) {
    if (data.size() < 7) {
        return 0;
    }
    return static_cast<uint16_t>(data[5]) | (static_cast<uint16_t>(data[6]) << 8);
}

/// Fallback side detection from the device name when the advertised Product ID
/// is unavailable. Joy-Con 2 names look like "Joy-Con 2 (L)" / "Joy-Con 2 (R)".
ControllerType GuessControllerType(const std::wstring& name) {
    const std::wstring lowered = ToLower(name);
    if (lowered.find(L"left") != std::wstring::npos || lowered.find(L"(l)") != std::wstring::npos) {
        return ControllerType::LeftJoyCon;
    }
    if (lowered.find(L"right") != std::wstring::npos || lowered.find(L"(r)") != std::wstring::npos) {
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

ControllerType ControllerTypeFromProductId(uint16_t productId) {
    switch (productId) {
    case 0x2066: // JoyCon 2 (R)
    case 0x2070: // JoyCon 2 (R), safe mode
        return ControllerType::RightJoyCon;
    case 0x2067: // JoyCon 2 (L)
    case 0x2071: // JoyCon 2 (L), safe mode
        return ControllerType::LeftJoyCon;
    default:
        return ControllerType::Unknown; // Pro (0x2069), GameCube (0x2073), etc.
    }
}

struct ControllerConnection::State {
    BluetoothLEDevice device{ nullptr };
    GattCharacteristic inputChar{ nullptr };
    GattCharacteristic writeChar{ nullptr };
    GattCharacteristic responseBasicChar{ nullptr };
    GattCharacteristic responseExtChar{ nullptr };
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

bool ControllerConnection::PairToHost(uint64_t hostAddress, PairingResult& result, std::string& error) const {
    if (!state_ || !state_->writeChar) {
        error = "No command characteristic available for pairing.";
        return false;
    }
    if (!state_->responseBasicChar && !state_->responseExtChar) {
        error = "No command-response characteristic available for pairing.";
        return false;
    }

    // Collect command responses arriving on the notification characteristics.
    struct ResponseSync {
        std::mutex mutex;
        std::condition_variable cv;
        std::vector<std::vector<uint8_t>> items;
    } responses;

    auto onValueChanged = [&responses](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
        auto reader = DataReader::FromBuffer(args.CharacteristicValue());
        std::vector<uint8_t> bytes(reader.UnconsumedBufferLength());
        if (!bytes.empty()) {
            reader.ReadBytes(bytes);
        }
        {
            std::scoped_lock lock(responses.mutex);
            responses.items.push_back(std::move(bytes));
        }
        responses.cv.notify_all();
    };

    std::vector<std::pair<GattCharacteristic, event_token>> subscriptions;
    auto subscribe = [&](const GattCharacteristic& characteristic) {
        if (!characteristic) {
            return;
        }
        characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
        subscriptions.emplace_back(characteristic, characteristic.ValueChanged(onValueChanged));
    };

    try {
        subscribe(state_->responseBasicChar);
        subscribe(state_->responseExtChar);
    } catch (const hresult_error& ex) {
        error = winrt::to_string(ex.message());
        return false;
    }

    auto unsubscribe = [&]() {
        for (auto& [characteristic, token] : subscriptions) {
            characteristic.ValueChanged(token);
        }
    };

    // Response header is `cmd 01 01 sub 10 78 00 00`, optionally prefixed by
    // zero padding on the extended (0x001e) characteristic; scan for it.
    auto matchHeader = [](const std::vector<uint8_t>& buffer, uint8_t cmd, uint8_t sub, std::size_t& dataStart) {
        for (std::size_t i = 0; i + 8 <= buffer.size() && i <= 20; ++i) {
            if (buffer[i] == cmd && buffer[i + 1] == 0x01 && buffer[i + 3] == sub) {
                dataStart = i + 8;
                return true;
            }
        }
        return false;
    };

    auto sendCommand = [&](uint8_t cmd, uint8_t sub, const std::vector<uint8_t>& data,
                           std::vector<uint8_t>& outData) -> bool {
        std::vector<uint8_t> packet = {
            cmd, 0x91, 0x01, sub, 0x00, static_cast<uint8_t>(data.size()), 0x00, 0x00
        };
        packet.insert(packet.end(), data.begin(), data.end());

        {
            std::scoped_lock lock(responses.mutex);
            responses.items.clear();
        }

        DataWriter writer;
        writer.WriteBytes(packet);
        state_->writeChar.WriteValueAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();

        std::unique_lock lock(responses.mutex);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
        while (true) {
            for (const auto& buffer : responses.items) {
                std::size_t dataStart = 0;
                if (matchHeader(buffer, cmd, sub, dataStart)) {
                    outData.assign(buffer.begin() + static_cast<std::ptrdiff_t>(dataStart), buffer.end());
                    return true;
                }
            }
            if (responses.cv.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
    };

    bool success = false;
    try {
        const auto hostWire = AddressToWire(hostAddress);
        result = {};
        result.hostAddress = hostAddress;

        // 1) 0x15/0x01 exchange addresses -> controller address.
        {
            std::vector<uint8_t> data = { 0x00, 0x01 };
            data.insert(data.end(), hostWire.begin(), hostWire.end());
            std::vector<uint8_t> resp;
            if (!sendCommand(0x15, 0x01, data, resp)) {
                throw std::runtime_error("Pairing step 0x15/0x01 (exchange addresses) timed out.");
            }
            if (resp.size() >= 6) {
                result.controllerAddress = WireToAddress(resp.data() + (resp.size() - 6));
            }
        }

        // 2) 0x15/0x04 exchange keys -> B1; LTK = A1 ^ B1.
        std::array<uint8_t, 16> ltk{};
        {
            std::array<uint8_t, 16> a1{};
            FillRandomBytes(a1.data(), a1.size());
            std::vector<uint8_t> data = { 0x00 };
            data.insert(data.end(), a1.begin(), a1.end());
            std::vector<uint8_t> resp;
            if (!sendCommand(0x15, 0x04, data, resp)) {
                throw std::runtime_error("Pairing step 0x15/0x04 (exchange keys) timed out.");
            }
            std::array<uint8_t, 16> b1{};
            if (resp.size() >= 16) {
                std::copy(resp.end() - 16, resp.end(), b1.begin());
            }
            result.deviceKeyMatchedKnown = (b1 == kFixedDeviceKey);
            for (std::size_t i = 0; i < 16; ++i) {
                ltk[i] = static_cast<uint8_t>(a1[i] ^ b1[i]);
            }
            result.ltk = ltk;
        }

        // 3) 0x15/0x02 confirm: verify AES128-ECB(reverse(LTK), reverse(A2)) == B2.
        {
            std::array<uint8_t, 16> a2{};
            FillRandomBytes(a2.data(), a2.size());
            std::vector<uint8_t> data = { 0x00 };
            data.insert(data.end(), a2.begin(), a2.end());
            std::vector<uint8_t> resp;
            if (sendCommand(0x15, 0x02, data, resp) && resp.size() >= 16) {
                std::array<uint8_t, 16> b2{};
                std::copy(resp.end() - 16, resp.end(), b2.begin());
                std::array<uint8_t, 16> keyReversed{};
                std::array<uint8_t, 16> challengeReversed{};
                for (std::size_t i = 0; i < 16; ++i) {
                    keyReversed[i] = ltk[15 - i];
                    challengeReversed[i] = a2[15 - i];
                }
                std::array<uint8_t, 16> expected{};
                if (AesEcbEncryptBlock(keyReversed, challengeReversed, expected)) {
                    result.challengeVerified = (expected == b2);
                }
            }
        }

        // 4) 0x15/0x03 finalise.
        {
            std::vector<uint8_t> resp;
            sendCommand(0x15, 0x03, { 0x00 }, resp);
        }

        // 5) 0x03/0x07 send pairing info ([addr:6][LTK:16], both reverse byte-order).
        {
            std::vector<uint8_t> data;
            data.insert(data.end(), hostWire.begin(), hostWire.end());
            for (std::size_t i = 0; i < 16; ++i) {
                data.push_back(ltk[15 - i]);
            }
            std::vector<uint8_t> resp;
            sendCommand(0x03, 0x07, data, resp);
        }

        // 6) 0x03/0x09 store pairing info.
        {
            std::vector<uint8_t> resp;
            sendCommand(0x03, 0x09, {}, resp);
        }

        success = true;
        error.clear();
    } catch (const std::exception& ex) {
        error = ex.what();
        success = false;
    } catch (const hresult_error& ex) {
        error = winrt::to_string(ex.message());
        success = false;
    }

    unsubscribe();
    return success;
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

ControllerConnection ConnectByAddress(
    uint64_t controllerAddress,
    ControllerType expectedType,
    const ConnectionOptions& options) {

    auto device = BluetoothLEDevice::FromBluetoothAddressAsync(controllerAddress).get();
    if (!device) {
        throw std::runtime_error("Failed to open Bluetooth LE device.");
    }

    auto servicesResult = GetGattServicesWithRetry(device);
    if (servicesResult.Status() != GattCommunicationStatus::Success) {
        throw std::runtime_error("Failed to enumerate GATT services.");
    }

    auto state = std::shared_ptr<ControllerConnection::State>(new ControllerConnection::State());
    state->device = device;
    state->info.name = device.Name().c_str();
    state->info.bluetoothAddress = device.BluetoothAddress();
    // Prefer the authoritative advertised Product ID; fall back to the name.
    state->info.type = (expectedType != ControllerType::Unknown)
        ? expectedType
        : GuessControllerType(state->info.name);

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
            const auto uuid = characteristic.Uuid();
            if (uuid == guid(kInputReportUuid)) {
                state->inputChar = characteristic;
            } else if (uuid == guid(kWriteCommandUuid)) {
                state->writeChar = characteristic;
            } else if (uuid == guid(kResponseBasicUuid)) {
                state->responseBasicChar = characteristic;
            } else if (uuid == guid(kResponseExtLeftUuid) || uuid == guid(kResponseExtRightUuid)) {
                state->responseExtChar = characteristic;
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

    if (options.requestLowLatency) {
        try {
            auto params = BluetoothLEPreferredConnectionParameters::ThroughputOptimized();
            state->device.RequestPreferredConnectionParameters(params);
        } catch (...) {
            // Keep the transport layer resilient. Callers can still receive packets.
        }
    }

    return ControllerConnection(state);
}

ControllerConnection ConnectMatchingControllerImpl(
    std::wstring_view prompt,
    const ConnectionOptions& options,
    ControllerType expectedAdvertisedType,
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

    uint64_t foundAddress = 0;
    bool connected = false;
    ControllerType acceptedAdvertisedType = ControllerType::Unknown;

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

            // Decide left/right from the advertised Product ID before connecting,
            // so we never grab the wrong side. If the side is requested but the
            // advert resolves to a known different side, keep scanning.
            const ControllerType advertisedType = ControllerTypeFromProductId(ReadAdvertisedProductId(data));
            if (expectedAdvertisedType != ControllerType::Unknown
                && advertisedType != ControllerType::Unknown
                && advertisedType != expectedAdvertisedType) {
                continue;
            }

            foundAddress = args.BluetoothAddress();
            acceptedAdvertisedType = advertisedType;
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

        auto connection = ConnectByAddress(foundAddress, acceptedAdvertisedType, attemptOptions);
        if (matcher(connection.Info())) {
            return connection;
        }
    }
}

std::optional<uint64_t> GetHostBluetoothAddress() {
    try {
        auto adapter = BluetoothAdapter::GetDefaultAsync().get();
        if (adapter) {
            return adapter.BluetoothAddress();
        }
    } catch (...) {
    }
    return std::nullopt;
}

ControllerConnection ConnectToFirstController(
    std::wstring_view prompt,
    const ConnectionOptions& options) {
    return ConnectMatchingControllerImpl(prompt, options, ControllerType::Unknown, [](const ControllerInfo&) {
        return true;
    });
}

ControllerConnection ConnectJoyCon(
    JoyConSide side,
    std::wstring_view prompt,
    const ConnectionOptions& options) {
    const ControllerType expectedType = ToControllerType(side);
    return ConnectMatchingControllerImpl(prompt, options, expectedType, [expectedType](const ControllerInfo& info) {
        if (info.type == ControllerType::Unknown) {
            return true;
        }

        return IsJoyCon(info.type) && info.type == expectedType;
    });
}

} // namespace joycon::transport
