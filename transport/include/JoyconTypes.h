#pragma once

#include <cstdint>

namespace joycon {

enum class ControllerType {
    Unknown = 0,
    LeftJoyCon = 1,
    RightJoyCon = 2,
    DualJoyCon = 3,
};

enum class JoyConSide {
    Left,
    Right
};

enum class JoyConOrientation {
    Upright,
    Sideways
};

struct StickValue {
    int16_t x = 0;
    int16_t y = 0;
};

struct ImuSample {
    int16_t gyroX = 0;
    int16_t gyroY = 0;
    int16_t gyroZ = 0;
    int16_t accelX = 0;
    int16_t accelY = 0;
    int16_t accelZ = 0;
};

} // namespace joycon
