#include "key_capture_win.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace joycon::webgui {
namespace {

std::mutex g_captureMutex;

HHOOK g_hook = nullptr;
std::atomic<int> g_outcome{ 0 };
WORD g_capturedVk = 0;

std::optional<std::string> VirtualKeyToCustomToken(WORD vk) {
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return std::string(1, static_cast<char>('0' + (vk - VK_NUMPAD0)));
    }
    if (vk >= '0' && vk <= '9') {
        return std::string(1, static_cast<char>(vk));
    }
    if (vk >= 'A' && vk <= 'Z') {
        return std::string(1, static_cast<char>(vk));
    }
    switch (vk) {
    case VK_SPACE:
        return std::string("SPACE");
    case VK_RETURN:
        return std::string("ENTER");
    case VK_ESCAPE:
        return std::string("ESC");
    case VK_TAB:
        return std::string("TAB");
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
        return std::string("CTRL");
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
        return std::string("SHIFT");
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
        return std::string("ALT");
    case VK_LEFT:
        return std::string("LEFT");
    case VK_RIGHT:
        return std::string("RIGHT");
    case VK_UP:
        return std::string("UP");
    case VK_DOWN:
        return std::string("DOWN");
    default:
        break;
    }
    if (vk >= VK_F1 && vk <= VK_F12) {
        return "F" + std::to_string(vk - VK_F1 + 1);
    }
    return std::nullopt;
}

bool IsIgnoredStandalone(WORD vk) {
    switch (vk) {
    case VK_LWIN:
    case VK_RWIN:
    case VK_APPS:
    case VK_SNAPSHOT:
    case VK_PAUSE:
        return true;
    default:
        return false;
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code != HC_ACTION) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
    if (wParam != WM_KEYDOWN && wParam != WM_SYSKEYDOWN) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    const WORD vk = static_cast<WORD>(info->vkCode);
    if (IsIgnoredStandalone(vk)) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
    if (vk == VK_ESCAPE) {
        g_outcome = 2;
        return 1;
    }
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_CONTROL || vk == VK_LCONTROL
        || vk == VK_RCONTROL || vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) {
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
    g_capturedVk = vk;
    g_outcome = 1;
    return 1;
}

} // namespace

CaptureKeyResult CaptureKeyCustomToken(std::chrono::milliseconds timeout) {
    std::lock_guard lock(g_captureMutex);
    CaptureKeyResult result;

    g_outcome = 0;
    g_capturedVk = 0;
    g_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
    if (!g_hook) {
        result.error = "hook_failed";
        return result;
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    MSG msg{};
    while (g_outcome.load() == 0 && std::chrono::steady_clock::now() < deadline) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    UnhookWindowsHookEx(g_hook);
    g_hook = nullptr;

    const int outcome = g_outcome.load();
    if (outcome == 1) {
        if (const auto token = VirtualKeyToCustomToken(g_capturedVk)) {
            result.ok = true;
            result.token = *token;
            result.action = "key_custom:" + *token;
        } else {
            result.error = "unsupported_key";
        }
    } else if (outcome == 2) {
        result.error = "cancelled";
    } else {
        result.error = "timeout";
    }

    return result;
}

} // namespace joycon::webgui
