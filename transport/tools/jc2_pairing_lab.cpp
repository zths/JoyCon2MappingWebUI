// JoyCon 2 / Switch 2 controller experiment lab.
//
// Two modes (pick via argv[1], default "sniff"):
//
//   sniff : BLE advertisement sniffer. Understand what a controller broadcasts
//           in each state (open/discoverable vs reconnection-to-bonded-host).
//
//   pair  : OOB pairing prototype. Connect to a controller in OPEN mode
//           (long-press SYNC to power on), then run Nintendo's custom
//           Out-Of-Band pairing so THIS PC's Bluetooth address + LTK get
//           stored in the controller's pairing table. Goal: enable
//           "press any button -> reconnect to PC" instead of always syncing.
//
// References: switch2_controller_research/{bluetooth_interface.md,commands.md}
//   - Pairing command 0x15: 01 exchange-addrs, 04 exchange-keys (LTK=A1^B1,
//     B1 fixed 5CF6EE792CDF05E1BA2B6325C41A5F10), 02 confirm (AES128-ECB),
//     03 finalise.
//   - JoyCon also stores via command 0x03: 07 send-pairing-info (addr+LTK,
//     byte-reversed), 09 store.
//
// Self-contained: does NOT touch the transport library's public API.

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winrt/base.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>

#include <Windows.h>
#include <bcrypt.h>
#include <conio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace winrt;
using namespace Windows::Devices::Bluetooth;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace Windows::Storage::Streams;

namespace {

// Nintendo manufacturer / company id (little-endian 0x53 0x05 in the raw advert).
constexpr uint16_t kNintendoCompanyId = 0x0553;

// WinRT strips the 2-byte company id, so Data() starts at doc offset 0x2.
// Prefix shared by all controller->host adverts: doc 0x2..0x5 = 01 00 03 7E.
const std::vector<uint8_t> kControllerPrefix = { 0x01, 0x00, 0x03, 0x7E };

// GATT UUIDs (see bluetooth_interface.md "GATT Attributes").
const wchar_t* kInputReportCommon = L"ab7de9be-89fe-49ad-828f-118f09df7fd2"; // 0x000a
const wchar_t* kWriteBasicCmd     = L"649d4ac9-8eb7-4e6c-af44-1ea54fe5f005"; // 0x0014 command
const wchar_t* kRespBasic         = L"c765a961-d9d8-4d36-a20a-5315b111836a"; // 0x001a response
const wchar_t* kRespExtLeft       = L"63a3810f-aec7-474b-9010-3d52403cb996"; // 0x001e JC(L)
const wchar_t* kRespExtRight      = L"640ca58e-0e88-410c-a7f3-426faf2b690b"; // 0x001e JC(R)

// Controller device public key B1 is fixed (per research docs).
const std::array<uint8_t, 16> kFixedB1 = {
    0x5C, 0xF6, 0xEE, 0x79, 0x2C, 0xDF, 0x05, 0xE1,
    0xBA, 0x2B, 0x63, 0x25, 0xC4, 0x1A, 0x5F, 0x10
};

std::string FormatMac(uint64_t address) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (int shift = 40; shift >= 0; shift -= 8) {
        oss << std::setw(2) << ((address >> shift) & 0xFF);
        if (shift != 0) {
            oss << ':';
        }
    }
    return oss.str();
}

std::string FormatHex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
        if (i + 1 != bytes.size()) {
            oss << ' ';
        }
    }
    return oss.str();
}

template <std::size_t N>
std::string FormatHex(const std::array<uint8_t, N>& bytes) {
    return FormatHex(std::vector<uint8_t>(bytes.begin(), bytes.end()));
}

std::string AdvertisementTypeName(BluetoothLEAdvertisementType type) {
    switch (type) {
    case BluetoothLEAdvertisementType::ConnectableUndirected:    return "ConnectableUndirected (ADV_IND)";
    case BluetoothLEAdvertisementType::ConnectableDirected:      return "ConnectableDirected (ADV_DIRECT_IND)";
    case BluetoothLEAdvertisementType::ScannableUndirected:      return "ScannableUndirected (ADV_SCAN_IND)";
    case BluetoothLEAdvertisementType::NonConnectableUndirected: return "NonConnectableUndirected (ADV_NONCONN_IND)";
    case BluetoothLEAdvertisementType::ScanResponse:             return "ScanResponse (SCAN_RSP)";
    default:                                                     return "Unknown";
    }
}

std::vector<uint8_t> ReadBuffer(const IBuffer& buffer) {
    std::vector<uint8_t> bytes(buffer.Length());
    if (!bytes.empty()) {
        auto reader = DataReader::FromBuffer(buffer);
        reader.ReadBytes(bytes);
    }
    return bytes;
}

bool HasControllerPrefix(const std::vector<uint8_t>& data) {
    if (data.size() < kControllerPrefix.size()) {
        return false;
    }
    return std::equal(kControllerPrefix.begin(), kControllerPrefix.end(), data.begin());
}

uint64_t LocalAdapterAddress() {
    try {
        auto adapter = BluetoothAdapter::GetDefaultAsync().get();
        if (adapter) {
            return adapter.BluetoothAddress();
        }
    } catch (const hresult_error&) {
    }
    return 0;
}

// 6-byte wire form (reverse byte-order / little-endian) of a BT address.
std::array<uint8_t, 6> AddressToWire(uint64_t address) {
    std::array<uint8_t, 6> out{};
    for (int i = 0; i < 6; ++i) {
        out[static_cast<std::size_t>(i)] = static_cast<uint8_t>((address >> (8 * i)) & 0xFF);
    }
    return out;
}

void RandomBytes(uint8_t* data, std::size_t len) {
    if (BCryptGenRandom(nullptr, data, static_cast<ULONG>(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (std::size_t i = 0; i < len; ++i) {
            data[i] = static_cast<uint8_t>(std::rand() & 0xFF);
        }
    }
}

// AES-128 ECB encrypt a single 16-byte block. Returns false on failure.
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
        const NTSTATUS st = BCryptEncrypt(
            keyHandle,
            reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(input.data())), static_cast<ULONG>(input.size()),
            nullptr,
            nullptr, 0,
            output.data(), static_cast<ULONG>(output.size()),
            &produced, 0);
        BCryptDestroyKey(keyHandle);
        ok = (st == 0 && produced == output.size());
    } while (false);
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

// ---------------------------------------------------------------------------
// Phase 1: advertisement sniffer
// ---------------------------------------------------------------------------

void PrintDecodedAdvert(const std::vector<uint8_t>& data) {
    if (data.size() >= 7) {
        const int productId = data[5] | (data[6] << 8);
        std::cout << "    productId      : 0x" << std::hex << std::uppercase
                  << std::setw(4) << std::setfill('0') << productId << std::dec << "\n";
    }
    if (data.size() >= 10) {
        const int flag = data[9];
        std::cout << "    flag@0xB       : 0x" << std::hex << std::uppercase
                  << std::setw(2) << std::setfill('0') << flag << std::dec;
        if (flag == 0x81)      std::cout << "  (WAKE advertisement)";
        else if (flag == 0x00) std::cout << "  (standard / reconnection)";
        std::cout << "\n";
    }
    if (data.size() >= 16) {
        std::vector<uint8_t> hostRev(data.begin() + 10, data.begin() + 16);
        const bool allZero = std::all_of(hostRev.begin(), hostRev.end(), [](uint8_t b) { return b == 0; });
        std::vector<uint8_t> hostAddr(hostRev.rbegin(), hostRev.rend());
        std::cout << "    hostAddress    : " << FormatHex(hostAddr);
        if (allZero) std::cout << "  (ALL ZERO -> open/discoverable, no bonded host)";
        else         std::cout << "  (bonded host -> RECONNECTION target)";
        std::cout << "\n";
    }
}

int RunSniffer() {
    std::cout << "=== JoyCon2 advertisement sniffer ===\n";
    std::cout << "Local BT adapter address: " << FormatMac(LocalAdapterAddress()) << "\n";
    std::cout << "Watching for Nintendo BLE advertisements. Press Enter to stop.\n\n";
    std::cout << "Try, while this runs:\n";
    std::cout << "  1) Long-press SYNC to power on (expect host = ALL ZERO).\n";
    std::cout << "  2) After a normal connect+disconnect, press ANY button\n";
    std::cout << "     (watch whether it advertises, and what host it targets).\n\n";

    std::mutex mutex;
    std::map<uint64_t, std::vector<uint8_t>> lastData;

    BluetoothLEAdvertisementWatcher watcher;
    watcher.ScanningMode(BluetoothLEScanningMode::Active);
    watcher.Received([&](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args) {
        const auto manufacturerData = args.Advertisement().ManufacturerData();
        for (uint32_t i = 0; i < manufacturerData.Size(); ++i) {
            const auto section = manufacturerData.GetAt(i);
            if (section.CompanyId() != kNintendoCompanyId) continue;
            const std::vector<uint8_t> data = ReadBuffer(section.Data());
            if (!HasControllerPrefix(data)) continue;

            const uint64_t address = args.BluetoothAddress();
            {
                std::scoped_lock lock(mutex);
                auto it = lastData.find(address);
                if (it != lastData.end() && it->second == data) return;
                lastData[address] = data;
            }
            const auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch()).count() % 100000;
            std::cout << "[" << std::setw(5) << std::setfill('0') << t << "ms] "
                      << FormatMac(address) << "  rssi=" << args.RawSignalStrengthInDBm() << "dBm\n";
            std::cout << "    pduType        : " << AdvertisementTypeName(args.AdvertisementType()) << "\n";
            std::cout << "    mfrData        : " << FormatHex(data) << "\n";
            PrintDecodedAdvert(data);
            std::cout << std::endl;
        }
    });
    watcher.Start();

    std::string dummy;
    std::getline(std::cin, dummy);
    watcher.Stop();
    std::cout << "Stopped.\n";
    return 0;
}

// ---------------------------------------------------------------------------
// Phase 2: OOB pairing prototype
// ---------------------------------------------------------------------------

class PairingSession {
public:
    bool ConnectOpenController(std::chrono::seconds timeout) {
        std::cout << "[pair] Scanning for a Nintendo controller (long-press SYNC now)...\n";
        const uint64_t address = ScanForController(timeout);
        if (address == 0) {
            std::cout << "[pair] No controller found within timeout.\n";
            return false;
        }

        device_ = BluetoothLEDevice::FromBluetoothAddressAsync(address).get();
        if (!device_) {
            std::cout << "[pair] FromBluetoothAddressAsync failed.\n";
            return false;
        }
        std::wcout << L"[pair] Connected device: " << device_.Name().c_str()
                   << L" (" << FormatMac(address).c_str() << L")\n";

        if (!DiscoverCharacteristics()) {
            return false;
        }
        SubscribeResponses();
        return true;
    }

    // Reconnect + stream test: confirm the controller streams input after a
    // button-wake reconnection, WITHOUT us being able to supply a link LTK.
    bool RunStreamTest() {
        if (!inputChar_) {
            std::cout << "[stream] input report characteristic (0x000a) not found.\n";
            return false;
        }
        // Minimal init (mirrors transport SendDefaultInitSequence).
        SendRaw({ 0x0C, 0x91, 0x01, 0x02, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 });
        SendRaw({ 0x0C, 0x91, 0x01, 0x04, 0x00, 0x04, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 });

        try {
            const auto st = inputChar_.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
            std::cout << "[stream] subscribe input(0x000a): "
                      << (st == GattCommunicationStatus::Success ? "ok" : "failed") << "\n";
            if (st != GattCommunicationStatus::Success) {
                return false;
            }
        } catch (const hresult_error& e) {
            std::wcout << L"[stream] subscribe input threw: " << e.message().c_str() << L"\n";
            return false;
        }

        inputPackets_ = 0;
        streamStart_ = std::chrono::steady_clock::now();
        inputChar_.ValueChanged([this](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
            const uint64_t n = ++inputPackets_;
            if (n <= 3) {
                std::cout << "    input #" << n << ": " << FormatHex(ReadBuffer(args.CharacteristicValue())) << "\n";
            } else if (n % 30 == 0) {
                // Heartbeat so it's obvious packets are still flowing.
                const double secs = std::chrono::duration<double>(
                    std::chrono::steady_clock::now() - streamStart_).count();
                std::cout << "    ...streaming: " << n << " packets"
                          << (secs > 0.0 ? " (" + std::to_string(static_cast<int>(n / secs)) + " Hz)" : "")
                          << "\r" << std::flush;
            }
        });

        std::cout << "[stream] Subscribed. Move sticks/press buttons; packets should flow.\n";
        std::cout << "[stream] Press Enter to stop and see the count.\n";
        std::string dummy;
        std::getline(std::cin, dummy);

        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - streamStart_).count();
        const uint64_t total = inputPackets_;
        std::cout << "[stream] Received " << total << " input packets in "
                  << std::fixed << std::setprecision(1) << secs << "s";
        if (secs > 0.0) std::cout << " (" << (total / secs) << " Hz)";
        std::cout << "\n";
        std::cout << (total > 0
                          ? "[stream] SUCCESS: streaming works on this connection (no custom LTK needed).\n"
                          : "[stream] NO PACKETS: controller may require an encrypted link on reconnect.\n");
        return total > 0;
    }

    bool RunPairing(uint64_t hostAddress) {
        const auto hostWire = AddressToWire(hostAddress);
        std::cout << "[pair] Host (PC) address wire bytes: "
                  << FormatHex(std::vector<uint8_t>(hostWire.begin(), hostWire.end())) << "\n";

        // 1) 0x15/0x01 exchange addresses: [00][count][addr...]
        {
            std::vector<uint8_t> data = { 0x00, 0x01 };
            data.insert(data.end(), hostWire.begin(), hostWire.end());
            std::vector<uint8_t> resp;
            if (!SendCommand(0x15, 0x01, data, resp)) {
                std::cout << "[pair] 0x15/0x01 exchange-addresses: NO RESPONSE\n";
                return false;
            }
            std::cout << "[pair] 0x15/0x01 ok. controller addr (resp): " << FormatHex(resp) << "\n";
        }

        // 2) 0x15/0x04 exchange keys: [00][A1:16] -> [B1:16]; LTK = A1 ^ B1
        std::array<uint8_t, 16> a1{};
        RandomBytes(a1.data(), a1.size());
        std::array<uint8_t, 16> ltk{};
        {
            std::vector<uint8_t> data = { 0x00 };
            data.insert(data.end(), a1.begin(), a1.end());
            std::vector<uint8_t> resp;
            if (!SendCommand(0x15, 0x04, data, resp)) {
                std::cout << "[pair] 0x15/0x04 exchange-keys: NO RESPONSE\n";
                return false;
            }
            std::array<uint8_t, 16> b1{};
            if (resp.size() >= 16) {
                std::copy(resp.end() - 16, resp.end(), b1.begin());
            }
            std::cout << "[pair] A1: " << FormatHex(a1) << "\n";
            std::cout << "[pair] B1: " << FormatHex(b1)
                      << (b1 == kFixedB1 ? "  (matches known fixed B1)" : "  (UNEXPECTED B1!)") << "\n";
            for (std::size_t i = 0; i < 16; ++i) ltk[i] = static_cast<uint8_t>(a1[i] ^ b1[i]);
            std::cout << "[pair] LTK = A1 ^ B1: " << FormatHex(ltk) << "\n";
        }

        // 3) 0x15/0x02 confirm LTK: [00][A2:16] -> [B2:16]; verify with AES.
        {
            std::array<uint8_t, 16> a2{};
            RandomBytes(a2.data(), a2.size());
            std::vector<uint8_t> data = { 0x00 };
            data.insert(data.end(), a2.begin(), a2.end());
            std::vector<uint8_t> resp;
            if (!SendCommand(0x15, 0x02, data, resp)) {
                std::cout << "[pair] 0x15/0x02 confirm: NO RESPONSE (continuing anyway)\n";
            } else {
                std::array<uint8_t, 16> b2{};
                if (resp.size() >= 16) std::copy(resp.end() - 16, resp.end(), b2.begin());
                std::cout << "[pair] B2 (resp): " << FormatHex(b2) << "\n";

                // python: B2 = AES_ECB(reverse(LTK)).encrypt(reverse(A2))
                std::array<uint8_t, 16> keyRev{}, a2Rev{}, expected{};
                for (std::size_t i = 0; i < 16; ++i) { keyRev[i] = ltk[15 - i]; a2Rev[i] = a2[15 - i]; }
                if (AesEcbEncryptBlock(keyRev, a2Rev, expected)) {
                    std::array<uint8_t, 16> expectedRev{};
                    for (std::size_t i = 0; i < 16; ++i) expectedRev[i] = expected[15 - i];
                    std::cout << "[pair] expected B2          : " << FormatHex(expected)
                              << (expected == b2 ? "  (MATCH)" : "") << "\n";
                    std::cout << "[pair] expected B2 (reversed): " << FormatHex(expectedRev)
                              << (expectedRev == b2 ? "  (MATCH)" : "") << "\n";
                }
            }
        }

        // 4) 0x15/0x03 finalise: [00]
        {
            std::vector<uint8_t> resp;
            const bool got = SendCommand(0x15, 0x03, { 0x00 }, resp);
            std::cout << "[pair] 0x15/0x03 finalise: " << (got ? "ok" : "no response") << "\n";
        }

        // 5) JoyCon: 0x03/0x07 send pairing info [addr:6][LTK:16] (both reversed)
        {
            std::vector<uint8_t> data;
            data.insert(data.end(), hostWire.begin(), hostWire.end());
            for (std::size_t i = 0; i < 16; ++i) data.push_back(ltk[15 - i]); // LTK reversed
            std::vector<uint8_t> resp;
            const bool got = SendCommand(0x03, 0x07, data, resp);
            std::cout << "[pair] 0x03/0x07 send-pairing-info: " << (got ? "ok" : "no response") << "\n";
        }

        // 6) JoyCon: 0x03/0x09 store
        {
            std::vector<uint8_t> resp;
            const bool got = SendCommand(0x03, 0x09, {}, resp);
            std::cout << "[pair] 0x03/0x09 store-pairing-info: " << (got ? "ok" : "no response") << "\n";
        }

        std::cout << "\n[pair] Sequence complete. Now:\n";
        std::cout << "       - Disconnect / power off the controller.\n";
        std::cout << "       - Run this tool in 'sniff' mode and press ANY button.\n";
        std::cout << "       - Check whether it advertises a reconnection targeting THIS PC:\n";
        std::cout << "         " << FormatMac(hostAddress) << "\n";
        return true;
    }

private:
    uint64_t ScanForController(std::chrono::seconds timeout) {
        std::mutex m;
        std::condition_variable cv;
        uint64_t found = 0;

        BluetoothLEAdvertisementWatcher watcher;
        watcher.ScanningMode(BluetoothLEScanningMode::Active);
        watcher.Received([&](BluetoothLEAdvertisementWatcher const&, BluetoothLEAdvertisementReceivedEventArgs const& args) {
            const auto md = args.Advertisement().ManufacturerData();
            for (uint32_t i = 0; i < md.Size(); ++i) {
                const auto section = md.GetAt(i);
                if (section.CompanyId() != kNintendoCompanyId) continue;
                if (!HasControllerPrefix(ReadBuffer(section.Data()))) continue;
                std::scoped_lock lock(m);
                if (found == 0) {
                    found = args.BluetoothAddress();
                    cv.notify_one();
                }
                return;
            }
        });
        watcher.Start();
        {
            std::unique_lock lock(m);
            cv.wait_for(lock, timeout, [&] { return found != 0; });
        }
        watcher.Stop();
        return found;
    }

    bool DiscoverCharacteristics() {
        auto result = device_.GetGattServicesAsync().get();
        if (result.Status() != GattCommunicationStatus::Success) {
            std::cout << "[pair] GetGattServicesAsync failed.\n";
            return false;
        }
        const auto services = result.Services();
        for (uint32_t i = 0; i < services.Size(); ++i) {
            const auto svc = services.GetAt(i);
            auto chars = svc.GetCharacteristicsAsync().get();
            if (chars.Status() != GattCommunicationStatus::Success) continue;
            const auto list = chars.Characteristics();
            for (uint32_t j = 0; j < list.Size(); ++j) {
                const auto c = list.GetAt(j);
                const auto uuid = c.Uuid();
                if (uuid == guid(kWriteBasicCmd))     writeChar_ = c;
                else if (uuid == guid(kRespBasic))    respBasic_ = c;
                else if (uuid == guid(kRespExtLeft))  respExt_ = c;
                else if (uuid == guid(kRespExtRight)) respExt_ = c;
                else if (uuid == guid(kInputReportCommon)) inputChar_ = c;
            }
        }
        std::cout << "[pair] chars found: writeCmd=" << (writeChar_ ? "Y" : "N")
                  << " respBasic(0x001a)=" << (respBasic_ ? "Y" : "N")
                  << " respExt(0x001e)=" << (respExt_ ? "Y" : "N") << "\n";
        if (!writeChar_) {
            std::cout << "[pair] ERROR: command write characteristic (0x0014) not found.\n";
            return false;
        }
        return true;
    }

    void SubscribeOne(const GattCharacteristic& c, const char* label) {
        if (!c) return;
        try {
            const auto st = c.WriteClientCharacteristicConfigurationDescriptorAsync(
                GattClientCharacteristicConfigurationDescriptorValue::Notify).get();
            std::cout << "[pair] subscribe " << label << ": "
                      << (st == GattCommunicationStatus::Success ? "ok" : "failed") << "\n";
        } catch (const hresult_error& e) {
            std::wcout << L"[pair] subscribe " << label << L" threw: " << e.message().c_str() << L"\n";
        }
        c.ValueChanged([this, label](GattCharacteristic const&, GattValueChangedEventArgs const& args) {
            std::vector<uint8_t> bytes = ReadBuffer(args.CharacteristicValue());
            {
                std::scoped_lock lock(respMutex_);
                responses_.push_back(bytes);
            }
            respCv_.notify_all();
            std::cout << "    <- [" << label << "] " << FormatHex(bytes) << "\n";
        });
    }

    void SubscribeResponses() {
        SubscribeOne(respBasic_, "0x001a");
        SubscribeOne(respExt_, "0x001e");
    }

    void SendRaw(const std::vector<uint8_t>& packet) {
        std::cout << "    -> raw " << FormatHex(packet) << "\n";
        try {
            DataWriter writer;
            writer.WriteBytes(array_view<const uint8_t>(packet.data(), packet.data() + packet.size()));
            writeChar_.WriteValueAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();
        } catch (const hresult_error& e) {
            std::wcout << L"    raw write threw: " << e.message().c_str() << L"\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Build header + data, write to command characteristic, wait for matching response.
    bool SendCommand(uint8_t cmd, uint8_t sub, const std::vector<uint8_t>& data, std::vector<uint8_t>& outData) {
        std::vector<uint8_t> packet = {
            cmd, 0x91, 0x01, sub, 0x00, static_cast<uint8_t>(data.size()), 0x00, 0x00
        };
        packet.insert(packet.end(), data.begin(), data.end());

        std::cout << "    -> 0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                  << static_cast<int>(cmd) << "/0x" << std::setw(2) << static_cast<int>(sub) << std::dec
                  << "  " << FormatHex(packet) << "\n";

        {
            std::scoped_lock lock(respMutex_);
            responses_.clear();
        }

        try {
            DataWriter writer;
            writer.WriteBytes(array_view<const uint8_t>(packet.data(), packet.data() + packet.size()));
            writeChar_.WriteValueAsync(writer.DetachBuffer(), GattWriteOption::WriteWithoutResponse).get();
        } catch (const hresult_error& e) {
            std::wcout << L"    write threw: " << e.message().c_str() << L"\n";
            return false;
        }

        return WaitForResponse(cmd, sub, outData, std::chrono::milliseconds(1500));
    }

    bool WaitForResponse(uint8_t cmd, uint8_t sub, std::vector<uint8_t>& outData, std::chrono::milliseconds timeout) {
        std::unique_lock lock(respMutex_);
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            for (const auto& buf : responses_) {
                std::size_t dataStart = 0;
                if (MatchResponseHeader(buf, cmd, sub, dataStart)) {
                    outData.assign(buf.begin() + static_cast<std::ptrdiff_t>(dataStart), buf.end());
                    return true;
                }
            }
            if (respCv_.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
    }

    // Response header: cmd 01 01 sub 10 78 00 00. On 0x001e it is prefixed by
    // ~0x0E zero bytes, so scan the first part of the buffer for the header.
    static bool MatchResponseHeader(const std::vector<uint8_t>& buf, uint8_t cmd, uint8_t sub, std::size_t& dataStart) {
        for (std::size_t i = 0; i + 8 <= buf.size() && i <= 20; ++i) {
            if (buf[i] == cmd && buf[i + 1] == 0x01 && buf[i + 3] == sub) {
                dataStart = i + 8;
                return true;
            }
        }
        return false;
    }

    BluetoothLEDevice device_{ nullptr };
    GattCharacteristic writeChar_{ nullptr };
    GattCharacteristic respBasic_{ nullptr };
    GattCharacteristic respExt_{ nullptr };
    GattCharacteristic inputChar_{ nullptr };

    std::mutex respMutex_;
    std::condition_variable respCv_;
    std::deque<std::vector<uint8_t>> responses_;

    std::atomic<uint64_t> inputPackets_{ 0 };
    std::chrono::steady_clock::time_point streamStart_{};
};

int RunPairing() {
    std::cout << "=== JoyCon2 OOB pairing prototype ===\n";
    const uint64_t host = LocalAdapterAddress();
    std::cout << "Local BT adapter (host) address: " << FormatMac(host) << "\n\n";
    if (host == 0) {
        std::cout << "ERROR: could not read local Bluetooth adapter address.\n";
        return 1;
    }

    PairingSession session;
    if (!session.ConnectOpenController(std::chrono::seconds(30))) {
        return 1;
    }
    session.RunPairing(host);

    std::cout << "\nPress Enter to exit.\n";
    std::string dummy;
    std::getline(std::cin, dummy);
    return 0;
}

int RunStream() {
    std::cout << "=== JoyCon2 reconnect + stream test ===\n";
    std::cout << "If the controller is asleep/disconnected, press ANY button to wake it.\n\n";

    PairingSession session;
    if (!session.ConnectOpenController(std::chrono::seconds(30))) {
        return 1;
    }
    session.RunStreamTest();
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    init_apartment();

    std::string mode = (argc > 1) ? argv[1] : "sniff";
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (mode == "pair") {
        return RunPairing();
    }
    if (mode == "stream") {
        return RunStream();
    }
    if (mode != "sniff") {
        std::cout << "Unknown mode '" << mode << "'. Usage: jc2_pairing_lab [sniff|pair|stream]\n";
        std::cout << "Falling back to sniff.\n\n";
    }
    return RunSniffer();
}
