#include "NintendoControllerProtocol.h"

#include <algorithm>
#include <cmath>

namespace joycon::protocol {
namespace {

int16_t ToSigned16(uint8_t lsb, uint8_t msb) {
    return static_cast<int16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
}

constexpr uint32_t BUTTON_A_MASK_RIGHT = 0x000800;
constexpr uint32_t BUTTON_B_MASK_RIGHT = 0x000200;
constexpr uint32_t BUTTON_X_MASK_RIGHT = 0x000400;
constexpr uint32_t BUTTON_Y_MASK_RIGHT = 0x000100;
constexpr uint32_t BUTTON_PLUS_MASK_RIGHT = 0x000002;
constexpr uint32_t BUTTON_R_MASK_RIGHT = 0x004000;
constexpr uint32_t BUTTON_STICK_MASK_RIGHT = 0x000004;

constexpr uint32_t BUTTON_UP_MASK_LEFT = 0x000002;
constexpr uint32_t BUTTON_DOWN_MASK_LEFT = 0x000001;
constexpr uint32_t BUTTON_LEFT_MASK_LEFT = 0x000008;
constexpr uint32_t BUTTON_RIGHT_MASK_LEFT = 0x000004;
constexpr uint32_t BUTTON_MINUS_MASK_LEFT = 0x000100;
constexpr uint32_t BUTTON_L_MASK_LEFT = 0x000040;
constexpr uint32_t BUTTON_STICK_MASK_LEFT = 0x000800;

constexpr uint64_t BUTTON_A_MASK = 0x000800000000;
constexpr uint64_t BUTTON_B_MASK = 0x000400000000;
constexpr uint64_t BUTTON_X_MASK = 0x000200000000;
constexpr uint64_t BUTTON_Y_MASK = 0x000100000000;
constexpr uint64_t BUTTON_R_SHOULDER = 0x004000000000;
constexpr uint64_t BUTTON_L_SHOULDER = 0x000000400000;
constexpr uint64_t BUTTON_DPAD_UP = 0x000000020000;
constexpr uint64_t BUTTON_DPAD_RIGHT = 0x000000040000;
constexpr uint64_t BUTTON_DPAD_DOWN = 0x000000010000;
constexpr uint64_t BUTTON_DPAD_LEFT = 0x000000080000;
constexpr uint64_t BUTTON_GUIDE = 0x000010000000;
constexpr uint64_t BUTTON_BACK = 0x000001000000;
constexpr uint64_t BUTTON_START = 0x000002000000;
constexpr uint64_t BUTTON_R_THUMB = 0x000004000000;
constexpr uint64_t BUTTON_L_THUMB = 0x000008000000;

constexpr uint64_t TRIGGER_LT_MASK = 0x000000800000;
constexpr uint64_t TRIGGER_RT_MASK = 0x008000000000;

void DecodeTriggersAndShoulders(
    uint32_t state,
    bool isLeft,
    bool upright,
    uint8_t& leftTrigger,
    uint8_t& rightTrigger) {

    leftTrigger = (state & 0x000080U) ? 255 : 0;
    rightTrigger = (state & 0x008000U) ? 255 : 0;

    if (upright) {
        if (state & 0x000040U) {
            leftTrigger = std::max<uint8_t>(leftTrigger, isLeft ? 255 : 0);
        }
        if (state & 0x004000U) {
            rightTrigger = std::max<uint8_t>(rightTrigger, isLeft ? 0 : 255);
        }
        return;
    }

    if (state & (isLeft ? 0x000020U : 0x002000U)) {
        leftTrigger = 255;
    }
    if (state & (isLeft ? 0x000010U : 0x001000U)) {
        rightTrigger = 255;
    }
}

std::pair<int16_t, int16_t> DecodePackedStick(const uint8_t* data) {
    if (data == nullptr) {
        return { static_cast<int16_t>(0), static_cast<int16_t>(0) };
    }

    const int xRaw = ((data[1] & 0x0F) << 8) | data[0];
    const int yRaw = (data[2] << 4) | ((data[1] & 0xF0) >> 4);

    float x = (xRaw - 2048) / 2048.0f;
    float y = (yRaw - 2048) / 2048.0f;

    constexpr float deadzone = 0.08f;
    if (std::abs(x) < deadzone && std::abs(y) < deadzone) {
        return { static_cast<int16_t>(0), static_cast<int16_t>(0) };
    }

    x = std::clamp(x * 1.7f, -1.0f, 1.0f);
    y = std::clamp(y * 1.7f, -1.0f, 1.0f);

    return {
        static_cast<int16_t>(x * 32767.0f),
        static_cast<int16_t>(y * 32767.0f)
    };
}

} // namespace

uint32_t ExtractButtonState24(const std::vector<uint8_t>& buffer, std::size_t offset) {
    if (buffer.size() < offset + 3) {
        return 0;
    }

    return
        (static_cast<uint32_t>(buffer[offset]) << 16) |
        (static_cast<uint32_t>(buffer[offset + 1]) << 8) |
        static_cast<uint32_t>(buffer[offset + 2]);
}

uint64_t ExtractButtonState48(const std::vector<uint8_t>& buffer, std::size_t offset) {
    if (buffer.size() < offset + 6) {
        return 0;
    }

    uint64_t value = 0;
    for (std::size_t i = 0; i < 6; ++i) {
        value = (value << 8) | buffer[offset + i];
    }
    return value;
}

std::pair<int16_t, int16_t> GetRawOpticalMouse(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x18) {
        return { static_cast<int16_t>(0), static_cast<int16_t>(0) };
    }

    return {
        ToSigned16(buffer[0x10], buffer[0x11]),
        ToSigned16(buffer[0x12], buffer[0x13])
    };
}

StickData DecodeJoystick(
    const std::vector<uint8_t>& buffer,
    JoyConSide side,
    JoyConOrientation orientation) {

    if (buffer.size() < 16) {
        return {};
    }

    const bool isLeft = (side == JoyConSide::Left);
    const bool upright = (orientation == JoyConOrientation::Upright);
    const uint8_t* data = isLeft ? &buffer[10] : &buffer[13];

    auto [x, y] = DecodePackedStick(data);
    float fx = x / 32767.0f;
    float fy = y / 32767.0f;

    if (!upright) {
        const float tx = fx;
        const float ty = fy;
        fx = isLeft ? -ty : ty;
        fy = isLeft ? tx : -tx;
    }

    return {
        static_cast<int16_t>(std::clamp(fx, -1.0f, 1.0f) * 32767),
        static_cast<int16_t>(-std::clamp(fy, -1.0f, 1.0f) * 32767)
    };
}

MotionData DecodeMotion(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x3C) {
        return {};
    }

    MotionData motion;
    motion.gyroX = ToSigned16(buffer[0x36], buffer[0x37]);
    motion.gyroY = ToSigned16(buffer[0x38], buffer[0x39]);
    motion.gyroZ = ToSigned16(buffer[0x3A], buffer[0x3B]);
    motion.accelX = ToSigned16(buffer[0x30], buffer[0x31]);
    motion.accelY = ToSigned16(buffer[0x32], buffer[0x33]);
    motion.accelZ = ToSigned16(buffer[0x34], buffer[0x35]);
    return motion;
}

DecodedInputState DecodeJoyConReport(
    const std::vector<uint8_t>& buffer,
    JoyConSide side,
    JoyConOrientation orientation) {

    if (buffer.size() < 0x3C) {
        return {};
    }

    DecodedInputState state;
    state.valid = true;
    state.controllerType = (side == JoyConSide::Left)
        ? ControllerType::LeftJoyCon
        : ControllerType::RightJoyCon;
    state.motion = DecodeMotion(buffer);

    const bool isLeft = (side == JoyConSide::Left);
    const std::size_t buttonOffset = isLeft ? 4 : 3;
    const uint32_t buttons = ExtractButtonState24(buffer, buttonOffset);
    state.buttons = buttons;

    if (isLeft) {
        state.leftButtons = buttons;
        state.leftStick = DecodeJoystick(buffer, side, orientation);

        if (buttons & BUTTON_UP_MASK_LEFT) {
            state.buttons |= BUTTON_DPAD_UP;
        }
        if (buttons & BUTTON_DOWN_MASK_LEFT) {
            state.buttons |= BUTTON_DPAD_DOWN;
        }
        if (buttons & BUTTON_LEFT_MASK_LEFT) {
            state.buttons |= BUTTON_DPAD_LEFT;
        }
        if (buttons & BUTTON_RIGHT_MASK_LEFT) {
            state.buttons |= BUTTON_DPAD_RIGHT;
        }
        if (buttons & BUTTON_MINUS_MASK_LEFT) {
            state.buttons |= BUTTON_BACK;
        }
        if (buttons & BUTTON_L_MASK_LEFT) {
            state.buttons |= BUTTON_L_SHOULDER;
        }
        if (buttons & BUTTON_STICK_MASK_LEFT) {
            state.buttons |= BUTTON_L_THUMB;
        }
    } else {
        state.rightButtons = buttons;
        state.rightStick = DecodeJoystick(buffer, side, orientation);

        if (buttons & BUTTON_A_MASK_RIGHT) {
            state.buttons |= BUTTON_A_MASK;
        }
        if (buttons & BUTTON_B_MASK_RIGHT) {
            state.buttons |= BUTTON_B_MASK;
        }
        if (buttons & BUTTON_X_MASK_RIGHT) {
            state.buttons |= BUTTON_X_MASK;
        }
        if (buttons & BUTTON_Y_MASK_RIGHT) {
            state.buttons |= BUTTON_Y_MASK;
        }
        if (buttons & BUTTON_PLUS_MASK_RIGHT) {
            state.buttons |= BUTTON_START;
        }
        if (buttons & BUTTON_R_MASK_RIGHT) {
            state.buttons |= BUTTON_R_SHOULDER;
        }
        if (buttons & BUTTON_STICK_MASK_RIGHT) {
            state.buttons |= BUTTON_R_THUMB;
        }

    }

    const auto [opticalX, opticalY] = GetRawOpticalMouse(buffer);
    state.opticalMouse = { opticalX, opticalY };
    if (buffer.size() > 0x17) {
        state.opticalDistance = buffer[0x17];
    }

    DecodeTriggersAndShoulders(
        buttons,
        isLeft,
        orientation == JoyConOrientation::Upright,
        state.leftTrigger,
        state.rightTrigger);

    return state;
}

DecodedInputState DecodeDualJoyConReport(
    const std::vector<uint8_t>& leftBuffer,
    const std::vector<uint8_t>& rightBuffer) {

    if (leftBuffer.size() < 0x3C || rightBuffer.size() < 0x3C) {
        return {};
    }

    auto left = DecodeJoyConReport(leftBuffer, JoyConSide::Left, JoyConOrientation::Upright);
    auto right = DecodeJoyConReport(rightBuffer, JoyConSide::Right, JoyConOrientation::Upright);

    if (!left.valid || !right.valid) {
        return {};
    }

    DecodedInputState combined;
    combined.valid = true;
    combined.controllerType = ControllerType::DualJoyCon;
    combined.buttons = left.buttons | right.buttons;
    combined.leftButtons = left.leftButtons;
    combined.rightButtons = right.rightButtons;
    combined.leftStick = left.leftStick;
    combined.rightStick = right.rightStick;
    combined.opticalMouse = right.opticalMouse;
    combined.leftTrigger = left.leftTrigger;
    combined.rightTrigger = right.rightTrigger;

    auto average = [](int16_t a, int16_t b) -> int16_t {
        if (a == 0) {
            return b;
        }
        if (b == 0) {
            return a;
        }
        return static_cast<int16_t>((a / 2) + (b / 2));
    };

    combined.motion.accelX = average(left.motion.accelX, right.motion.accelX);
    combined.motion.accelY = average(left.motion.accelY, right.motion.accelY);
    combined.motion.accelZ = average(left.motion.accelZ, right.motion.accelZ);
    combined.motion.gyroX = average(left.motion.gyroX, right.motion.gyroX);
    combined.motion.gyroY = average(left.motion.gyroY, right.motion.gyroY);
    combined.motion.gyroZ = average(left.motion.gyroZ, right.motion.gyroZ);

    return combined;
}

DecodedInputState DecodeProControllerReport(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x3C) {
        return {};
    }

    DecodedInputState state;
    state.valid = true;
    state.controllerType = ControllerType::ProController;
    state.buttons = ExtractButtonState48(buffer, 3);
    state.motion = DecodeMotion(buffer);
    state.leftTrigger = (state.buttons & TRIGGER_LT_MASK) ? 255 : 0;
    state.rightTrigger = (state.buttons & TRIGGER_RT_MASK) ? 255 : 0;

    auto [lx, ly] = DecodePackedStick(&buffer[10]);
    auto [rx, ry] = DecodePackedStick(&buffer[13]);
    state.leftStick = { lx, static_cast<int16_t>(-ly) };
    state.rightStick = { rx, static_cast<int16_t>(-ry) };

    return state;
}

DecodedInputState DecodeNSOGameCubeReport(const std::vector<uint8_t>& buffer) {
    if (buffer.size() < 0x3E) {
        return {};
    }

    DecodedInputState state = DecodeProControllerReport(buffer);
    if (!state.valid) {
        return state;
    }

    state.leftTrigger = buffer[0x3C];
    state.rightTrigger = buffer[0x3D];
    state.controllerType = ControllerType::NSOGameCubeController;
    return state;
}

DecodedInputState DecodeInputPacket(const std::vector<uint8_t>& buffer, const DecodeOptions& options) {
    switch (options.controllerType) {
    case ControllerType::LeftJoyCon:
        return DecodeJoyConReport(buffer, JoyConSide::Left, options.orientation);
    case ControllerType::RightJoyCon:
        return DecodeJoyConReport(buffer, JoyConSide::Right, options.orientation);
    case ControllerType::ProController:
        return DecodeProControllerReport(buffer);
    case ControllerType::NSOGameCubeController:
        return DecodeNSOGameCubeReport(buffer);
    case ControllerType::DualJoyCon:
    case ControllerType::Unknown:
    default:
        return {};
    }
}

} // namespace joycon::protocol
