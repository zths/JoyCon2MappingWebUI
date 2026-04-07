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
    /// Optical distance u16 LE @0x16; mouse gating: active when value <= configured near threshold (typ. desk ~100–200, air ~3000).
    uint16_t opticalDistance = 0xFFFF;
    MotionData motion{};
    uint8_t leftTrigger = 0;
    uint8_t rightTrigger = 0;

    /// Joy-Con 2 full-report offsets (also read on older layouts; may be garbage if unused).
    uint16_t batteryVoltageMv = 0; ///< u16 LE @ 0x1F (JC2 full input report); 0 = unavailable or failed plausibility. Rated 3.89 V (Nintendo).
    int16_t batteryCurrentRaw = 0; ///< int16 LE @ 0x22; often ~raw/100 ≈ mA.
    int16_t magnetometerX = 0; ///< s16 LE @ 0x19
    int16_t magnetometerY = 0; ///< s16 LE @ 0x1B
    int16_t magnetometerZ = 0; ///< s16 LE @ 0x1D
    /// R-JC: primary temp s16 LE @0x2E (cold/warm validated); rough degC = 25 + raw/127.
    /// L-JC: primary temp-related u16 LE @0x2C (monotonic with heat; hotter = larger; degC TBD).
    int32_t temperatureRaw = 0;
    /// L-JC only: s16 LE @0x2E secondary thermal / status (small swing vs main); R-JC unused (0).
    int32_t temperatureSecondaryRaw = 0;
    float temperatureCelsius = 0.f;
    bool temperatureValid = false;

    /// Byte 0x07 bits 6–7 (as a 2-bit value 0–3): observed 0 when USB/charging connected, 3 when unplugged (Joy-Con 2).
    uint8_t chargerBits67 = 0;
    bool chargerCableConnected = false;
};

struct DecodeOptions {
    ControllerType controllerType = ControllerType::Unknown;
    JoyConOrientation orientation = JoyConOrientation::Upright;
};

uint32_t ExtractButtonState24(const std::vector<uint8_t>& buffer, std::size_t offset);
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

DecodedInputState DecodeInputPacket(const std::vector<uint8_t>& buffer, const DecodeOptions& options);

} // namespace joycon::protocol
