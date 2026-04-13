#pragma once

#include <cstdint>

namespace joycon::protocol {

/// Joy-Con 2 full input report: 24-bit button field from `ExtractButtonState24` (offset 4 = left, 3 = right).
/// Single source for decode (`NintendoControllerProtocol`) and Web GUI mapping (`mapper_runtime`).

namespace JoyCon2LeftReport24 {
inline constexpr uint32_t Down = 0x000001;
inline constexpr uint32_t Up = 0x000002;
inline constexpr uint32_t Left = 0x000008;
inline constexpr uint32_t Right = 0x000004;
inline constexpr uint32_t SL = 0x000020;
inline constexpr uint32_t SR = 0x000010;
inline constexpr uint32_t ZL = 0x000080;
inline constexpr uint32_t L = 0x000040;
inline constexpr uint32_t Minus = 0x000100;
inline constexpr uint32_t StickPress = 0x000800;
inline constexpr uint32_t Capture = 0x002000;
} // namespace JoyCon2LeftReport24

namespace JoyCon2RightReport24 {
inline constexpr uint32_t Plus = 0x000002;
inline constexpr uint32_t StickPress = 0x000004;
inline constexpr uint32_t Home = 0x000010;
inline constexpr uint32_t C = 0x000040;
inline constexpr uint32_t Y = 0x000100;
inline constexpr uint32_t B = 0x000400;
inline constexpr uint32_t X = 0x000200;
inline constexpr uint32_t A = 0x000800;
inline constexpr uint32_t R = 0x004000;
inline constexpr uint32_t ZR = 0x008000;
inline constexpr uint32_t SL = 0x002000;
inline constexpr uint32_t SR = 0x001000;
} // namespace JoyCon2RightReport24

} // namespace joycon::protocol
