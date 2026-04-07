#pragma once

#include "output_sink.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace joycon::webgui {

class SendInputOutputSink final : public IOutputSink {
public:
    SendInputOutputSink();
    ~SendInputOutputSink() override;

    SendInputOutputSink(const SendInputOutputSink&) = delete;
    SendInputOutputSink& operator=(const SendInputOutputSink&) = delete;

    void MouseMoveRel(int dx, int dy) override;
    void EmitMouseButton(OutputMouseButton button, bool pressed) override;
    void MouseWheel(int32_t delta) override;
    void KeyboardEdge(std::string_view logicalInputId, uint16_t virtualKey, bool pressed, bool repeatEligible) override;
    void CancelKeyboardRepeat(std::string_view logicalInputId) override;

private:
    struct RepeatEntry {
        uint16_t virtualKey = 0;
        std::chrono::steady_clock::time_point nextRepeatTime{};
    };

    struct KeyRepeatSettings {
        std::chrono::milliseconds initialDelay{ 500 };
        std::chrono::milliseconds repeatInterval{ 40 };
    };

    static KeyRepeatSettings QueryOsKeyRepeatSettings();

    void RepeatThreadMain();

    KeyRepeatSettings repeatSettings_;
    std::mutex repeatMutex_;
    std::condition_variable repeatCondition_;
    std::map<std::string, RepeatEntry> activeRepeats_;
    std::atomic<bool> repeatRunning_{ false };
    std::thread repeatThread_;
};

} // namespace joycon::webgui
