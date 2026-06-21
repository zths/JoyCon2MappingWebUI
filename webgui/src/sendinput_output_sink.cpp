#include "sendinput_output_sink.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace joycon::webgui {
namespace {

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

void SendMouseWheel(int32_t delta) {
    INPUT input = {};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    SendInput(1, &input, sizeof(INPUT));
}

} // namespace

SendInputOutputSink::KeyRepeatSettings SendInputOutputSink::QueryOsKeyRepeatSettings() {
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

SendInputOutputSink::SendInputOutputSink()
    : repeatSettings_(QueryOsKeyRepeatSettings()) {
    repeatRunning_.store(true);
    repeatThread_ = std::thread([this]() { RepeatThreadMain(); });
}

SendInputOutputSink::~SendInputOutputSink() {
    {
        std::scoped_lock lock(repeatMutex_);
        repeatRunning_.store(false);
        repeatId_.clear();
    }
    repeatCondition_.notify_all();
    if (repeatThread_.joinable()) {
        repeatThread_.join();
    }
}

void SendInputOutputSink::MouseMoveRel(int dx, int dy) {
    SendRelativeMouseMove(dx, dy);
}

void SendInputOutputSink::EmitMouseButton(OutputMouseButton button, bool pressed) {
    switch (button) {
    case OutputMouseButton::Left:
        SendMouseButton(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP, pressed);
        break;
    case OutputMouseButton::Right:
        SendMouseButton(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP, pressed);
        break;
    case OutputMouseButton::Middle:
        SendMouseButton(MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, pressed);
        break;
    }
}

void SendInputOutputSink::MouseWheel(int32_t delta) {
    SendMouseWheel(delta);
}

void SendInputOutputSink::KeyboardEdge(std::string_view logicalInputId, uint16_t virtualKey, bool pressed, bool repeatEligible) {
    const std::string id(logicalInputId);

    std::unique_lock lock(repeatMutex_);

    if (!pressed) {
        // Releasing the active repeater stops repeating; previously-held keys do
        // NOT resume (matches Windows typematic behavior).
        if (repeatId_ == id) {
            repeatId_.clear();
        }
        repeatCondition_.notify_all();
        lock.unlock();
        if (virtualKey != 0) {
            SendKeyboardKey(static_cast<WORD>(virtualKey), false);
        }
        return;
    }

    if (virtualKey == 0) {
        return;
    }

    SendKeyboardKey(static_cast<WORD>(virtualKey), true);

    if (repeatEligible) {
        // This key becomes the sole repeater, superseding any earlier held key.
        repeatId_ = std::move(id);
        repeat_.virtualKey = virtualKey;
        repeat_.nextRepeatTime = std::chrono::steady_clock::now() + repeatSettings_.initialDelay;
        repeatCondition_.notify_all();
    }
    // Non-repeating keys (e.g. modifiers) are pressed without disturbing the
    // current repeater.
}

void SendInputOutputSink::CancelKeyboardRepeat(std::string_view logicalInputId) {
    std::scoped_lock lock(repeatMutex_);
    if (repeatId_ == logicalInputId) {
        repeatId_.clear();
    }
    repeatCondition_.notify_all();
}

void SendInputOutputSink::RepeatThreadMain() {
    std::unique_lock lock(repeatMutex_);
    while (true) {
        if (!repeatRunning_.load() && repeatId_.empty()) {
            break;
        }

        if (repeatId_.empty()) {
            repeatCondition_.wait(lock, [this]() {
                return !repeatRunning_.load() || !repeatId_.empty();
            });
            continue;
        }

        // Wake at the next repeat time, or earlier if the repeater changed/stopped.
        const std::string repeaterAtWait = repeatId_;
        const auto wakeTime = repeat_.nextRepeatTime;
        if (repeatCondition_.wait_until(lock, wakeTime, [this, &repeaterAtWait, wakeTime]() {
                return !repeatRunning_.load() || repeatId_.empty() ||
                    repeatId_ != repeaterAtWait || repeat_.nextRepeatTime != wakeTime;
            })) {
            continue;
        }

        if (repeatId_.empty()) {
            continue;
        }
        SendKeyboardKey(static_cast<WORD>(repeat_.virtualKey), true);
        repeat_.nextRepeatTime = std::chrono::steady_clock::now() + repeatSettings_.repeatInterval;
    }
}

} // namespace joycon::webgui
