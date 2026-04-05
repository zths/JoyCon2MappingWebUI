#pragma once

#include "JoyconTypes.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace joycon::protocol {

using StickData = ::joycon::StickValue;
using MotionData = ::joycon::ImuSample;

struct DecodedInputState {
    bool valid = false;
    ControllerType controllerType = ControllerType::Unknown;
    uint64_t buttons = 0;
    uint32_t leftButtons = 0;
    uint32_t rightButtons = 0;
    StickData leftStick{};
    StickData rightStick{};
    StickData opticalMouse{};
    uint8_t opticalDistance = 0xFF;
    MotionData motion{};
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;
};

struct DecodeOptions {
    ControllerType controllerType = ControllerType::Unknown;
    JoyConOrientation orientation = JoyConOrientation::Upright;
};

uint32_t ExtractButtonState24(const std::vector<uint8_t>& buffer, std::size_t offset);
uint64_t ExtractButtonState48(const std::vector<uint8_t>& buffer, std::size_t offset);
std::pair<int16_t, int16_t> GetRawOpticalMouse(const std::vector<uint8_t>& buffer);
StickData DecodeJoystick(const std::vector<uint8_t>& buffer, JoyConSide side, JoyConOrientation orientation);
MotionData DecodeMotion(const std::vector<uint8_t>& buffer);

DecodedInputState DecodeJoyConReport(
    const std::vector<uint8_t>& buffer,
    JoyConSide side,
    JoyConOrientation orientation);

DecodedInputState DecodeDualJoyConReport(
    const std::vector<uint8_t>& leftBuffer,
    const std::vector<uint8_t>& rightBuffer);

DecodedInputState DecodeProControllerReport(const std::vector<uint8_t>& buffer);
DecodedInputState DecodeNSOGameCubeReport(const std::vector<uint8_t>& buffer);
DecodedInputState DecodeInputPacket(const std::vector<uint8_t>& buffer, const DecodeOptions& options);

} // namespace joycon::protocol
