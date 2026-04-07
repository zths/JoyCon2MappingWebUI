#pragma once

#include <cstdint>
#include <string_view>

namespace joycon::webgui {

enum class OutputMouseButton {
    Left,
    Right,
    Middle,
};

/// Pluggable output for injected keyboard/mouse events. Keyboard auto-repeat (typematic) is a
/// backend concern: SendInput needs software repeat; a HID keyboard report can hold the key and
/// omit repeat, letting the OS generate it (or not).
class IOutputSink {
public:
    virtual ~IOutputSink() = default;

    virtual void MouseMoveRel(int dx, int dy) = 0;
    virtual void EmitMouseButton(OutputMouseButton button, bool pressed) = 0;
    virtual void MouseWheel(int32_t delta) = 0;

    /// Per logical mapping source (e.g. "L:StickUp" / "R:A"). Used to key repeat state.
    virtual void KeyboardEdge(std::string_view logicalInputId, uint16_t virtualKey, bool pressed, bool repeatEligible) = 0;

    /// Stop typematic repeat for this logical input only (no key up/down). Used when the mapping action is "none".
    virtual void CancelKeyboardRepeat(std::string_view logicalInputId) = 0;
};

} // namespace joycon::webgui
