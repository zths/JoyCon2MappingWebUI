#include "mapper_runtime.h"

#include <winrt/base.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <numbers>
#include <stdexcept>

namespace joycon::webgui {
namespace {

constexpr uint32_t LEFT_SL_MASK = 0x0020;
constexpr uint32_t LEFT_SR_MASK = 0x0010;
constexpr uint32_t LEFT_ZL_MASK = 0x0080;
constexpr uint32_t LEFT_L_MASK = 0x0040;
constexpr uint32_t LEFT_MINUS_MASK = 0x0100;
constexpr uint32_t LEFT_STICK_MASK = 0x0800;
constexpr uint32_t LEFT_UP_MASK = 0x0002;
constexpr uint32_t LEFT_DOWN_MASK = 0x0001;
constexpr uint32_t LEFT_LEFT_MASK = 0x0008;
constexpr uint32_t LEFT_RIGHT_MASK = 0x0004;
constexpr uint32_t LEFT_CAPTURE_MASK = 0x2000;

constexpr uint32_t RIGHT_ZR_MASK = 0x8000;
constexpr uint32_t RIGHT_R_MASK = 0x4000;
constexpr uint32_t RIGHT_PLUS_MASK = 0x0002;
constexpr uint32_t RIGHT_SL_MASK = 0x2000;
constexpr uint32_t RIGHT_SR_MASK = 0x1000;
constexpr uint32_t RIGHT_Y_MASK = 0x0100;
constexpr uint32_t RIGHT_B_MASK = 0x0400;
constexpr uint32_t RIGHT_X_MASK = 0x0200;
constexpr uint32_t RIGHT_A_MASK = 0x0800;
constexpr uint32_t RIGHT_STICK_MASK = 0x0004;
constexpr uint32_t RIGHT_HOME_MASK = 0x0010;
constexpr uint32_t RIGHT_C_MASK = 0x0040;

const std::vector<std::pair<std::string, uint32_t>>& LeftButtonMap() {
    static const std::vector<std::pair<std::string, uint32_t>> buttons = {
        { "ZL", LEFT_ZL_MASK },
        { "L", LEFT_L_MASK },
        { "Minus", LEFT_MINUS_MASK },
        { "L3", LEFT_STICK_MASK },
        { "Up", LEFT_UP_MASK },
        { "Down", LEFT_DOWN_MASK },
        { "Left", LEFT_LEFT_MASK },
        { "Right", LEFT_RIGHT_MASK },
        { "SL", LEFT_SL_MASK },
        { "SR", LEFT_SR_MASK },
        { "Capture", LEFT_CAPTURE_MASK },
    };
    return buttons;
}

const std::vector<std::pair<std::string, uint32_t>>& RightButtonMap() {
    static const std::vector<std::pair<std::string, uint32_t>> buttons = {
        { "ZR", RIGHT_ZR_MASK },
        { "R", RIGHT_R_MASK },
        { "Plus", RIGHT_PLUS_MASK },
        { "R3", RIGHT_STICK_MASK },
        { "A", RIGHT_A_MASK },
        { "B", RIGHT_B_MASK },
        { "X", RIGHT_X_MASK },
        { "Y", RIGHT_Y_MASK },
        { "SL", RIGHT_SL_MASK },
        { "SR", RIGHT_SR_MASK },
        { "Home", RIGHT_HOME_MASK },
        { "C", RIGHT_C_MASK },
    };
    return buttons;
}

WORD ActionToVirtualKey(const std::string& action) {
    if (action == "key_space") return VK_SPACE;
    if (action == "key_enter") return VK_RETURN;
    if (action == "key_escape") return VK_ESCAPE;
    if (action == "key_tab") return VK_TAB;
    if (action == "key_ctrl") return VK_CONTROL;
    if (action == "key_shift") return VK_SHIFT;
    if (action == "key_alt") return VK_MENU;
    if (action == "key_up") return VK_UP;
    if (action == "key_down") return VK_DOWN;
    if (action == "key_left") return VK_LEFT;
    if (action == "key_right") return VK_RIGHT;
    if (action == "key_w") return 'W';
    if (action == "key_a") return 'A';
    if (action == "key_s") return 'S';
    if (action == "key_d") return 'D';
    if (action == "key_q") return 'Q';
    if (action == "key_e") return 'E';
    if (action == "key_r") return 'R';
    if (action == "key_f") return 'F';
    if (action == "key_1") return '1';
    if (action == "key_2") return '2';
    if (action == "key_3") return '3';
    if (action == "key_4") return '4';
    if (action == "key_5") return '5';
    return 0;
}

WORD ParseCustomVirtualKey(const std::string& value) {
    if (value.empty()) {
        return 0;
    }

    std::string upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });

    if (upper == "SPACE") return VK_SPACE;
    if (upper == "ENTER") return VK_RETURN;
    if (upper == "ESC" || upper == "ESCAPE") return VK_ESCAPE;
    if (upper == "TAB") return VK_TAB;
    if (upper == "CTRL" || upper == "CONTROL") return VK_CONTROL;
    if (upper == "SHIFT") return VK_SHIFT;
    if (upper == "ALT") return VK_MENU;
    if (upper == "LEFT") return VK_LEFT;
    if (upper == "RIGHT") return VK_RIGHT;
    if (upper == "UP") return VK_UP;
    if (upper == "DOWN") return VK_DOWN;

    if (upper.size() == 1) {
        const char ch = upper[0];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return static_cast<WORD>(ch);
        }
    }

    if (upper.size() == 2 && upper[0] == 'F' && std::isdigit(static_cast<unsigned char>(upper[1]))) {
        return static_cast<WORD>(VK_F1 + (upper[1] - '1'));
    }
    if (upper.size() == 3 && upper[0] == 'F' && upper[1] == '1' && upper[2] >= '0' && upper[2] <= '2') {
        return static_cast<WORD>(VK_F10 + (upper[2] - '0'));
    }

    return 0;
}

void SendRelativeMouseMove(int dx, int dy) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &input, sizeof(INPUT));
}

void SendMouseButton(DWORD downFlag, DWORD upFlag, bool pressed) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = pressed ? downFlag : upFlag;
    SendInput(1, &input, sizeof(INPUT));
}

bool IsExtendedVirtualKey(WORD virtualKey) {
    switch (virtualKey) {
    case VK_RMENU:
    case VK_RCONTROL:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
    case VK_LEFT:
    case VK_RIGHT:
    case VK_NUMLOCK:
    case VK_DIVIDE:
    case VK_SNAPSHOT:
        return true;
    default:
        return false;
    }
}

void SendKeyboardKey(WORD virtualKey, bool pressed) {
    INPUT input = {};
    input.type = INPUT_KEYBOARD;
    UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC_EX);
    if (scanCode == 0) {
        scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
    }

    DWORD flags = pressed ? 0 : KEYEVENTF_KEYUP;
    if (scanCode != 0) {
        input.ki.wVk = 0;
        input.ki.wScan = static_cast<WORD>(scanCode & 0xFF);
        flags |= KEYEVENTF_SCANCODE;
        if (IsExtendedVirtualKey(virtualKey) || (scanCode & 0xFF00) == 0xE000 || (scanCode & 0xFF00) == 0xE100) {
            flags |= KEYEVENTF_EXTENDEDKEY;
        }
    } else {
        input.ki.wVk = virtualKey;
    }

    input.ki.dwFlags = flags;
    SendInput(1, &input, sizeof(INPUT));
}

int ClampStickDeadzone(int deadzone) {
    return std::clamp(deadzone, 0, 32767);
}

StickMapping NormalizeStickMapping(StickMapping mapping) {
    mapping.deadzone = ClampStickDeadzone(mapping.deadzone);
    mapping.hysteresis = ClampStickDeadzone(mapping.hysteresis);
    mapping.diagonalUnlockRadius = std::clamp(mapping.diagonalUnlockRadius, mapping.deadzone, 32767);
    mapping.fourWayHysteresisDegrees = std::clamp(mapping.fourWayHysteresisDegrees, 0.0, 45.0);
    mapping.eightWayHysteresisDegrees = std::clamp(mapping.eightWayHysteresisDegrees, 0.0, 22.5);
    return mapping;
}

const StickMapping& StickMappingForSide(const StickSettings& settings, JoyConSide side) {
    return (side == JoyConSide::Left) ? settings.left : settings.right;
}

StickMapping& StickMappingForSide(StickSettings& settings, JoyConSide side) {
    return (side == JoyConSide::Left) ? settings.left : settings.right;
}

const MouseSettings& MouseSettingsForSide(const MouseConfig& config, JoyConSide side) {
    return (side == JoyConSide::Left) ? config.left : config.right;
}

MouseSettings& MouseSettingsForSide(MouseConfig& config, JoyConSide side) {
    return (side == JoyConSide::Left) ? config.left : config.right;
}

double NormalizeStickAngle(double angleDegrees) {
    double angle = std::fmod(angleDegrees, 360.0);
    if (angle < 0.0) {
        angle += 360.0;
    }
    return angle;
}

bool IsAngleWithinRange(double angleDegrees, double centerDegrees, double halfWidthDegrees) {
    const double angle = NormalizeStickAngle(angleDegrees);
    const double start = NormalizeStickAngle(centerDegrees - halfWidthDegrees);
    const double end = NormalizeStickAngle(centerDegrees + halfWidthDegrees);
    if (start <= end) {
        return angle >= start && angle <= end;
    }
    return angle >= start || angle <= end;
}

int ComputeFourWayDirection(double normalizedAngle) {
    if (normalizedAngle > 45.0 && normalizedAngle <= 135.0) {
        return 0;
    }
    if (normalizedAngle > 135.0 && normalizedAngle <= 225.0) {
        return 3;
    }
    if (normalizedAngle > 225.0 && normalizedAngle <= 315.0) {
        return 2;
    }
    return 1;
}

int ComputeEightWaySector(double normalizedAngle) {
    if (normalizedAngle <= 22.5 || normalizedAngle > 337.5) return 0;
    if (normalizedAngle <= 67.5) return 1;
    if (normalizedAngle <= 112.5) return 2;
    if (normalizedAngle <= 157.5) return 3;
    if (normalizedAngle <= 202.5) return 4;
    if (normalizedAngle <= 247.5) return 5;
    if (normalizedAngle <= 292.5) return 6;
    return 7;
}

double FourWayDirectionCenter(int direction) {
    switch (direction) {
    case 0: return 90.0;
    case 1: return 0.0;
    case 2: return 270.0;
    case 3: return 180.0;
    default: return 0.0;
    }
}

double EightWaySectorCenter(int sector) {
    return std::fmod(static_cast<double>(sector) * 45.0, 360.0);
}

int ApplyAngularHysteresis(
    int candidate,
    int current,
    double normalizedAngle,
    double halfWidthDegrees,
    double hysteresisDegrees,
    bool eightWay) {

    if (candidate < 0) {
        return -1;
    }
    if (current < 0) {
        return candidate;
    }
    if (candidate == current) {
        return current;
    }

    const double center = eightWay ? EightWaySectorCenter(current) : FourWayDirectionCenter(current);
    if (IsAngleWithinRange(normalizedAngle, center, halfWidthDegrees + hysteresisDegrees)) {
        return current;
    }
    return candidate;
}

int ConvertFourWayDirectionToEightWaySector(int direction) {
    switch (direction) {
    case 0: return 2;
    case 1: return 0;
    case 2: return 6;
    case 3: return 4;
    default: return -1;
    }
}

void ApplySectorToDirections(int sector, bool& upPressed, bool& downPressed, bool& leftPressed, bool& rightPressed) {
    upPressed = false;
    downPressed = false;
    leftPressed = false;
    rightPressed = false;

    switch (sector) {
    case 0:
        rightPressed = true;
        break;
    case 1:
        rightPressed = true;
        upPressed = true;
        break;
    case 2:
        upPressed = true;
        break;
    case 3:
        upPressed = true;
        leftPressed = true;
        break;
    case 4:
        leftPressed = true;
        break;
    case 5:
        leftPressed = true;
        downPressed = true;
        break;
    case 6:
        downPressed = true;
        break;
    case 7:
        downPressed = true;
        rightPressed = true;
        break;
    default:
        break;
    }
}

std::string ActionForInputId(const AppConfig& config, JoyConSide side, const std::string& inputId) {
    const auto& buttonMapping = (side == JoyConSide::Left) ? config.mapping.left : config.mapping.right;
    if (const auto it = buttonMapping.find(inputId); it != buttonMapping.end()) {
        return it->second;
    }

    const auto& stickMapping = StickMappingForSide(config.sticks, side);
    if (inputId == "StickUp") return stickMapping.up;
    if (inputId == "StickDown") return stickMapping.down;
    if (inputId == "StickLeft") return stickMapping.left;
    if (inputId == "StickRight") return stickMapping.right;
    return "none";
}

AppConfig DefaultConfig() {
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

} // namespace

MapperRuntime::MapperRuntime()
    : config_(DefaultConfig()) {
    leftSlot_.side = JoyConSide::Left;
    rightSlot_.side = JoyConSide::Right;
    keyRepeatSettings_ = QueryKeyRepeatSettings();
    StartMouseOutputThread();
    StartKeyRepeatThread();
}

MapperRuntime::~MapperRuntime() {
    DisconnectSide(JoyConSide::Left);
    DisconnectSide(JoyConSide::Right);
    StopMouseOutputThread();
    StopKeyRepeatThread();
}

void MapperRuntime::ApplyConfig(const AppConfig& config) {
    std::scoped_lock lock(mutex_);
    ReleaseMappedInputs(leftSlot_);
    ReleaseMappedInputs(rightSlot_);
    config_ = config;
    config_.sticks.left = NormalizeStickMapping(config_.sticks.left);
    config_.sticks.right = NormalizeStickMapping(config_.sticks.right);
}

AppConfig MapperRuntime::CurrentConfig() const {
    std::scoped_lock lock(mutex_);
    return config_;
}

bool MapperRuntime::ConnectSide(JoyConSide side, std::string& errorMessage) {
    try {
        winrt::init_apartment();
    } catch (...) {
    }

    DisconnectSide(side);

    auto& slot = (side == JoyConSide::Left) ? leftSlot_ : rightSlot_;
    {
        std::scoped_lock lock(mutex_);
        slot.status = ConnectionStatus::Connecting;
        slot.error.clear();
        slot.deviceName.clear();
        slot.latestState = {};
        slot.latestButtonBits = 0;
        slot.packetCount = 0;
        slot.intervalSamples = 0;
        slot.intervalSumUs = 0.0;
        slot.lastPacketTime = {};
        slot.previousInputs.clear();
    }

    try {
        transport::ConnectionOptions connectionOptions;
        auto connection = transport::ConnectJoyCon(
            side,
            side == JoyConSide::Left
                ? L"Waiting for LEFT Joy-Con..."
                : L"Waiting for RIGHT Joy-Con...",
            connectionOptions);

        protocol::DecodeOptions decodeOptions;
        decodeOptions.controllerType = (side == JoyConSide::Left)
            ? ControllerType::LeftJoyCon
            : ControllerType::RightJoyCon;
        decodeOptions.orientation = JoyConOrientation::Upright;

        auto ownedConnection = std::make_unique<transport::ControllerConnection>(std::move(connection));
        ownedConnection->SetConnectionStatusCallback([this, side](transport::ControllerConnectionStatus status) {
            HandleConnectionStatusEvent(side, status);
        });
        const bool started = ownedConnection->StartInputStream(
            [this, side, decodeOptions](const transport::RawInputPacket& packet) {
                const auto state = protocol::DecodeInputPacket(packet.data, decodeOptions);
                if (!state.valid) {
                    return;
                }
                HandleDecodedState(side, state);
            });

        if (!started) {
            throw std::runtime_error("Failed to start input stream.");
        }

        transport::DeviceConfiguration deviceConfig;
        deviceConfig.sendDefaultInitSequence = true;
        deviceConfig.setPlayerLights = true;
        deviceConfig.playerLightPattern = (side == JoyConSide::Left) ? 0x08 : 0x02;
        deviceConfig.playConnectionRumble = true;
        ownedConnection->Configure(deviceConfig);

        {
            std::scoped_lock lock(mutex_);
            slot.connection = std::move(ownedConnection);
            slot.deviceName = slot.connection->Info().name;
            slot.status = ConnectionStatus::Connected;
            slot.error.clear();
        }

        errorMessage.clear();
        return true;
    } catch (const std::exception& ex) {
        std::scoped_lock lock(mutex_);
        slot.status = ConnectionStatus::Error;
        slot.error = ex.what();
        errorMessage = slot.error;
        return false;
    }
}

void MapperRuntime::DisconnectSide(JoyConSide side) {
    std::unique_ptr<transport::ControllerConnection> connection;
    {
        std::scoped_lock lock(mutex_);
        auto& slot = (side == JoyConSide::Left) ? leftSlot_ : rightSlot_;
        ReleaseMappedInputs(slot);
        connection = std::move(slot.connection);
        slot.status = ConnectionStatus::Disconnected;
        slot.error.clear();
        slot.deviceName.clear();
        slot.latestState = {};
        slot.latestButtonBits = 0;
        slot.packetCount = 0;
        slot.intervalSamples = 0;
        slot.intervalSumUs = 0.0;
        slot.lastPacketTime = {};
        slot.previousInputs.clear();
        slot.mouseAccumulator = {};
    }

    if (connection) {
        connection->StopInputStream();
    }
}

RuntimeSnapshot MapperRuntime::Snapshot() const {
    std::scoped_lock lock(mutex_);

    MouseStatsSnapshot mouseSnapshot;
    mouseSnapshot.movedPackets = mouseStats_.movedPackets;
    mouseSnapshot.injectedMoves = mouseStats_.injectedMoves;
    mouseSnapshot.gatedPackets = mouseStats_.gatedPackets;
    mouseSnapshot.averageDispatchUs = mouseStats_.dispatchSamples > 0
        ? (mouseStats_.dispatchSumUs / mouseStats_.dispatchSamples)
        : 0.0;
    mouseSnapshot.maxDispatchUs = mouseStats_.maxDispatchUs;
    mouseSnapshot.lastDistance = mouseStats_.lastDistance;
    mouseSnapshot.minDistance = mouseStats_.minDistance;
    mouseSnapshot.maxDistance = mouseStats_.maxDistance;

    return {
        .config = config_,
        .left = SnapshotFromSlot(leftSlot_),
        .right = SnapshotFromSlot(rightSlot_),
        .mouseStats = mouseSnapshot,
    };
}

void MapperRuntime::StartMouseOutputThread() {
    running_ = true;
    mouseOutputThread_ = std::thread([this]() {
        double carryX = 0.0;
        double carryY = 0.0;

        while (true) {
            std::unique_lock lock(mutex_);
            mouseCondition_.wait_for(lock, std::chrono::milliseconds(2), [this]() {
                const double pendingX = leftSlot_.mouseAccumulator.pendingX + rightSlot_.mouseAccumulator.pendingX;
                const double pendingY = leftSlot_.mouseAccumulator.pendingY + rightSlot_.mouseAccumulator.pendingY;
                return !running_ || std::abs(pendingX) >= 0.5 || std::abs(pendingY) >= 0.5;
            });

            const double pendingX = leftSlot_.mouseAccumulator.pendingX + rightSlot_.mouseAccumulator.pendingX;
            const double pendingY = leftSlot_.mouseAccumulator.pendingY + rightSlot_.mouseAccumulator.pendingY;
            if (!running_ && std::abs(pendingX) < 0.5 && std::abs(pendingY) < 0.5) {
                break;
            }

            const double sliceX = pendingX / 3.0;
            const double sliceY = pendingY / 3.0;
            const double desiredX = sliceX + carryX;
            const double desiredY = sliceY + carryY;
            const int moveX = static_cast<int>(std::trunc(desiredX));
            const int moveY = static_cast<int>(std::trunc(desiredY));
            carryX = desiredX - moveX;
            carryY = desiredY - moveY;

            if (moveX == 0 && moveY == 0) {
                continue;
            }

            const auto distributeMove = [](MapperRuntime::MouseAccumulator& accumulator, int totalMove, double totalPending, bool xAxis) {
                if (totalMove == 0 || std::abs(totalPending) < 1e-9) {
                    return;
                }
                double& pending = xAxis ? accumulator.pendingX : accumulator.pendingY;
                if (std::abs(pending) < 1e-9) {
                    return;
                }
                const double share = pending / totalPending;
                pending -= static_cast<double>(totalMove) * share;
            };
            distributeMove(leftSlot_.mouseAccumulator, moveX, pendingX, true);
            distributeMove(rightSlot_.mouseAccumulator, moveX, pendingX, true);
            distributeMove(leftSlot_.mouseAccumulator, moveY, pendingY, false);
            distributeMove(rightSlot_.mouseAccumulator, moveY, pendingY, false);
            const auto queueTime = std::max(leftSlot_.mouseAccumulator.lastEnqueueTime, rightSlot_.mouseAccumulator.lastEnqueueTime);
            lock.unlock();

            SendRelativeMouseMove(moveX, moveY);

            lock.lock();
            ++mouseStats_.injectedMoves;
            if (queueTime.time_since_epoch().count() != 0) {
                const double dispatchUs =
                    std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - queueTime).count();
                mouseStats_.dispatchSumUs += dispatchUs;
                mouseStats_.maxDispatchUs = std::max(mouseStats_.maxDispatchUs, dispatchUs);
                ++mouseStats_.dispatchSamples;
            }
        }
    });
}

void MapperRuntime::StopMouseOutputThread() {
    {
        std::scoped_lock lock(mutex_);
        running_ = false;
        leftSlot_.mouseAccumulator.pendingX = 0.0;
        leftSlot_.mouseAccumulator.pendingY = 0.0;
        rightSlot_.mouseAccumulator.pendingX = 0.0;
        rightSlot_.mouseAccumulator.pendingY = 0.0;
    }
    mouseCondition_.notify_all();
    if (mouseOutputThread_.joinable()) {
        mouseOutputThread_.join();
    }
}

void MapperRuntime::StartKeyRepeatThread() {
    keyRepeatThread_ = std::thread([this]() {
        std::unique_lock lock(mutex_);
        while (true) {
            if (!running_ && activeKeyRepeats_.empty()) {
                break;
            }

            if (activeKeyRepeats_.empty()) {
                keyRepeatCondition_.wait(lock, [this]() {
                    return !running_ || !activeKeyRepeats_.empty();
                });
                continue;
            }

            auto nextIt = std::min_element(
                activeKeyRepeats_.begin(),
                activeKeyRepeats_.end(),
                [](const auto& lhs, const auto& rhs) {
                    return lhs.second.nextRepeatTime < rhs.second.nextRepeatTime;
                });
            const auto wakeTime = nextIt->second.nextRepeatTime;
            if (keyRepeatCondition_.wait_until(lock, wakeTime, [this, wakeTime]() {
                    return !running_ || activeKeyRepeats_.empty() ||
                        std::any_of(activeKeyRepeats_.begin(), activeKeyRepeats_.end(), [wakeTime](const auto& entry) {
                            return entry.second.nextRepeatTime < wakeTime;
                        });
                })) {
                continue;
            }

            const auto now = std::chrono::steady_clock::now();
            std::vector<uint16_t> repeatKeys;
            for (auto& [inputId, state] : activeKeyRepeats_) {
                if (state.nextRepeatTime <= now) {
                    repeatKeys.push_back(state.virtualKey);
                    state.nextRepeatTime = now + keyRepeatSettings_.repeatInterval;
                }
            }

            lock.unlock();
            for (const uint16_t virtualKey : repeatKeys) {
                SendKeyboardKey(static_cast<WORD>(virtualKey), true);
            }
            lock.lock();
        }
    });
}

void MapperRuntime::StopKeyRepeatThread() {
    {
        std::scoped_lock lock(mutex_);
        activeKeyRepeats_.clear();
    }
    keyRepeatCondition_.notify_all();
    if (keyRepeatThread_.joinable()) {
        keyRepeatThread_.join();
    }
}

void MapperRuntime::HandleDecodedState(JoyConSide side, const protocol::DecodedInputState& state) {
    const auto now = std::chrono::steady_clock::now();
    std::scoped_lock lock(mutex_);
    auto& slot = (side == JoyConSide::Left) ? leftSlot_ : rightSlot_;

    ++slot.packetCount;
    if (slot.lastPacketTime.time_since_epoch().count() != 0) {
        slot.intervalSumUs += std::chrono::duration<double, std::micro>(now - slot.lastPacketTime).count();
        ++slot.intervalSamples;
    }
    slot.lastPacketTime = now;
    slot.latestState = state;
    slot.latestButtonBits = ExtractButtonBits(side, state);

    UpdateMappedButtons(slot);
    UpdateMappedStick(slot);
    UpdateMouseFromJoyCon(slot, now);
}

void MapperRuntime::HandleConnectionStatusEvent(
    JoyConSide side,
    transport::ControllerConnectionStatus status) {

    if (status == transport::ControllerConnectionStatus::Connected) {
        std::scoped_lock lock(mutex_);
        auto& slot = (side == JoyConSide::Left) ? leftSlot_ : rightSlot_;
        if (slot.connection) {
            slot.status = ConnectionStatus::Connected;
            slot.error.clear();
        }
        return;
    }

    {
        std::scoped_lock lock(mutex_);
        auto& slot = (side == JoyConSide::Left) ? leftSlot_ : rightSlot_;
        if (!slot.connection && slot.status == ConnectionStatus::Disconnected) {
            return;
        }

        ReleaseMappedInputs(slot);
        slot.connection.reset();
        slot.status = ConnectionStatus::Disconnected;
        slot.error = "Controller disconnected.";
        slot.deviceName.clear();
        slot.latestState = {};
        slot.latestButtonBits = 0;
        slot.packetCount = 0;
        slot.intervalSamples = 0;
        slot.intervalSumUs = 0.0;
        slot.lastPacketTime = {};
        slot.previousInputs.clear();
        slot.mouseAccumulator = {};
    }
}

void MapperRuntime::UpdateMouseFromJoyCon(
    ControllerSlot& slot,
    const std::chrono::steady_clock::time_point& callbackTime) {

    const auto& state = slot.latestState;
    const auto& settings = MouseSettingsForSide(config_.mouse, slot.side);
    auto& accumulator = slot.mouseAccumulator;

    mouseStats_.lastDistance = state.opticalDistance;
    mouseStats_.minDistance = std::min(mouseStats_.minDistance, state.opticalDistance);
    mouseStats_.maxDistance = std::max(mouseStats_.maxDistance, state.opticalDistance);

    if (!settings.enabled || !IsOpticalMouseActive(settings, state.opticalDistance)) {
        ++mouseStats_.gatedPackets;
        accumulator.hasOpticalSample = false;
        accumulator.pendingX = 0.0;
        accumulator.pendingY = 0.0;
        return;
    }

    const int16_t rawX = state.opticalMouse.x;
    const int16_t rawY = state.opticalMouse.y;

    if (!accumulator.hasOpticalSample) {
        accumulator.lastOpticalX = rawX;
        accumulator.lastOpticalY = rawY;
        accumulator.hasOpticalSample = true;
        return;
    }

    const int dx = WrapOpticalDelta(rawX, accumulator.lastOpticalX);
    const int dy = WrapOpticalDelta(rawY, accumulator.lastOpticalY);
    accumulator.lastOpticalX = rawX;
    accumulator.lastOpticalY = rawY;

    if (dx == 0 && dy == 0) {
        return;
    }

    ++mouseStats_.movedPackets;
    const double speed = std::hypot(static_cast<double>(dx), static_cast<double>(dy));
    const double gain = ComputeAccelerationGain(settings, speed);
    accumulator.pendingX += dx * gain;
    accumulator.pendingY += dy * gain;
    accumulator.lastEnqueueTime = callbackTime;
    mouseCondition_.notify_one();
}

void MapperRuntime::UpdateMappedButtons(ControllerSlot& slot) {
    const auto& buttonDefs = (slot.side == JoyConSide::Left) ? LeftButtonMap() : RightButtonMap();

    for (const auto& [buttonId, mask] : buttonDefs) {
        const bool pressed = (slot.latestButtonBits & mask) != 0;
        UpdateMappedInputState(slot, buttonId, pressed, ActionForInputId(config_, slot.side, buttonId));
    }
}

void MapperRuntime::UpdateMappedStick(ControllerSlot& slot) {
    const auto& mapping = StickMappingForSide(config_.sticks, slot.side);
    const auto& stick = (slot.side == JoyConSide::Left) ? slot.latestState.leftStick : slot.latestState.rightStick;
    const int deadzone = ClampStickDeadzone(mapping.deadzone);
    const int radialHysteresis = ClampStickDeadzone(mapping.hysteresis);
    const int activationRadius = slot.previousInputs["StickUp"] || slot.previousInputs["StickDown"]
        || slot.previousInputs["StickLeft"] || slot.previousInputs["StickRight"]
        ? deadzone
        : std::min(32767, deadzone + radialHysteresis);
    const int radius = static_cast<int>(std::hypot(static_cast<double>(stick.x), static_cast<double>(stick.y)));

    bool upPressed = false;
    bool downPressed = false;
    bool leftPressed = false;
    bool rightPressed = false;

    if (radius < activationRadius) {
        slot.lockedFourWayDirection = -1;
        slot.activeEightWaySector = -1;
        slot.diagonalModeActive = false;
    } else {
        const double angle = std::atan2(-static_cast<double>(stick.y), static_cast<double>(stick.x)) * 180.0 / std::numbers::pi;
        const double normalizedAngle = NormalizeStickAngle(angle);
        const int diagonalPressRadius = std::max(activationRadius, mapping.diagonalUnlockRadius);
        const int diagonalReleaseRadius = std::max(
            deadzone,
            std::max(mapping.diagonalUnlockRadius - radialHysteresis, 0));
        const bool useEightWay = slot.diagonalModeActive
            ? (radius >= diagonalReleaseRadius)
            : (radius >= diagonalPressRadius);

        if (useEightWay) {
            const int candidateSector = ComputeEightWaySector(normalizedAngle);
            slot.activeEightWaySector = ApplyAngularHysteresis(
                candidateSector,
                slot.activeEightWaySector,
                normalizedAngle,
                22.5,
                mapping.eightWayHysteresisDegrees,
                true);
            slot.diagonalModeActive = true;
            slot.lockedFourWayDirection = -1;
            ApplySectorToDirections(slot.activeEightWaySector, upPressed, downPressed, leftPressed, rightPressed);
        } else {
            const int candidateDirection = ComputeFourWayDirection(normalizedAngle);
            slot.lockedFourWayDirection = ApplyAngularHysteresis(
                candidateDirection,
                slot.lockedFourWayDirection,
                normalizedAngle,
                45.0,
                mapping.fourWayHysteresisDegrees,
                false);
            slot.activeEightWaySector = ConvertFourWayDirectionToEightWaySector(slot.lockedFourWayDirection);
            slot.diagonalModeActive = false;
            ApplySectorToDirections(slot.activeEightWaySector, upPressed, downPressed, leftPressed, rightPressed);
        }
    }

    UpdateMappedInputState(slot, "StickUp", upPressed, mapping.up);
    UpdateMappedInputState(slot, "StickDown", downPressed, mapping.down);
    UpdateMappedInputState(slot, "StickLeft", leftPressed, mapping.left);
    UpdateMappedInputState(slot, "StickRight", rightPressed, mapping.right);
}

void MapperRuntime::ReleaseMappedInputs(ControllerSlot& slot) {
    for (auto& [inputId, pressed] : slot.previousInputs) {
        if (!pressed) {
            continue;
        }

        const std::string action = ActionForInputId(config_, slot.side, inputId);
        if (action != "none") {
            InjectMappedAction(action, false);
        }
        activeKeyRepeats_.erase(inputId);
        pressed = false;
    }
    slot.lockedFourWayDirection = -1;
    slot.activeEightWaySector = -1;
    slot.diagonalModeActive = false;
    keyRepeatCondition_.notify_all();
}

void MapperRuntime::UpdateMappedInputState(
    ControllerSlot& slot,
    const std::string& inputId,
    bool pressed,
    const std::string& action) {

    const bool previousPressed = slot.previousInputs[inputId];
    if (pressed == previousPressed) {
        return;
    }

    slot.previousInputs[inputId] = pressed;
    if (action == "none") {
        activeKeyRepeats_.erase(inputId);
        keyRepeatCondition_.notify_all();
        return;
    }

    InjectMappedAction(action, pressed);
    const uint16_t virtualKey = ResolveKeyboardVirtualKey(action);
    if (pressed && virtualKey != 0 && ShouldAutoRepeatVirtualKey(virtualKey)) {
        KeyRepeatState state;
        state.virtualKey = virtualKey;
        state.nextRepeatTime = std::chrono::steady_clock::now() + keyRepeatSettings_.initialDelay;
        activeKeyRepeats_[inputId] = state;
        keyRepeatCondition_.notify_all();
    } else {
        activeKeyRepeats_.erase(inputId);
        keyRepeatCondition_.notify_all();
    }
}

uint32_t MapperRuntime::ExtractButtonBits(JoyConSide side, const protocol::DecodedInputState& state) {
    return (side == JoyConSide::Left) ? state.leftButtons : state.rightButtons;
}

bool MapperRuntime::IsOpticalMouseActive(const MouseSettings& settings, uint8_t opticalDistance) {
    return opticalDistance != 0x0C && opticalDistance <= settings.distanceThreshold;
}

int16_t MapperRuntime::WrapOpticalDelta(int16_t current, int16_t previous) {
    const int delta = (static_cast<int>(current) - static_cast<int>(previous) + 32768) % 65536 - 32768;
    return static_cast<int16_t>(delta);
}

double MapperRuntime::ComputeAccelerationGain(const MouseSettings& settings, double speed) {
    constexpr double deadzone = 1.5;
    if (speed <= deadzone) {
        return settings.baseSensitivity;
    }

    const double accelerated = settings.baseSensitivity +
        (settings.acceleration * std::pow(speed - deadzone, settings.exponent));
    return std::min(accelerated, settings.maxGain);
}

void MapperRuntime::InjectMappedAction(const std::string& action, bool pressed) {
    if (action == "mouse_left") {
        SendMouseButton(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP, pressed);
        return;
    }
    if (action == "mouse_right") {
        SendMouseButton(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP, pressed);
        return;
    }
    if (action == "mouse_middle") {
        SendMouseButton(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, pressed);
        return;
    }

    const uint16_t virtualKey = ResolveKeyboardVirtualKey(action);
    if (virtualKey != 0) {
        SendKeyboardKey(static_cast<WORD>(virtualKey), pressed);
    }
}

uint16_t MapperRuntime::ResolveKeyboardVirtualKey(const std::string& action) {
    if (action.rfind("key_custom:", 0) == 0) {
        return static_cast<uint16_t>(ParseCustomVirtualKey(action.substr(std::strlen("key_custom:"))));
    }
    return static_cast<uint16_t>(ActionToVirtualKey(action));
}

bool MapperRuntime::ShouldAutoRepeatVirtualKey(uint16_t virtualKey) {
    return virtualKey != VK_SHIFT && virtualKey != VK_CONTROL && virtualKey != VK_MENU;
}

MapperRuntime::KeyRepeatSettings MapperRuntime::QueryKeyRepeatSettings() {
    KeyRepeatSettings settings;

    int delay = 1;
    if (SystemParametersInfoA(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) {
        settings.initialDelay = std::chrono::milliseconds(250 * (1 + std::clamp(delay, 0, 3)));
    }

    int speed = 31;
    if (SystemParametersInfoA(SPI_GETKEYBOARDSPEED, 0, &speed, 0)) {
        const double charsPerSecond = 2.5 + (std::clamp(speed, 0, 31) / 31.0) * 27.5;
        settings.repeatInterval = std::chrono::milliseconds(
            std::max(16, static_cast<int>(std::lround(1000.0 / charsPerSecond))));
    }

    return settings;
}

ControllerStateSnapshot MapperRuntime::SnapshotFromSlot(const ControllerSlot& slot) {
    ControllerStateSnapshot snapshot;
    snapshot.side = slot.side;
    snapshot.status = slot.status;
    snapshot.error = slot.error;
    snapshot.deviceName = slot.deviceName;
    snapshot.packetCount = slot.packetCount;
    snapshot.averageIntervalMs = slot.intervalSamples > 0
        ? (slot.intervalSumUs / slot.intervalSamples) / 1000.0
        : 0.0;
    snapshot.rateHz = snapshot.averageIntervalMs > 0.0
        ? (1000.0 / snapshot.averageIntervalMs)
        : 0.0;
    snapshot.buttonBits = slot.latestButtonBits;
    snapshot.decoded = slot.latestState;
    return snapshot;
}

} // namespace joycon::webgui
