# transport

`transport` is now organized as a small SDK-style library instead of a demo-first project.

It does not depend on `ViGEm`, and it does not inject keyboard or mouse input. It is intended to be the reusable base layer for your own mapper, game integration, or tooling.

## Public headers

- `JoyconSdk.h`
  - umbrella include for the full SDK surface
- `JoyconTypes.h`
  - shared enums and data structs
- `NintendoControllerTransport.h`
  - BLE scan/connect/configure/start-stream APIs
- `NintendoControllerProtocol.h`
  - raw packet decoding APIs
- `JoyCon2InputReportButtons.h`
  - Joy-Con 2 input report 24-bit button field masks (shared by decode and host mapping)

## Library responsibilities

- discover and connect to supported Nintendo BLE controllers
- find GATT characteristics
- send default init/configuration commands
- start a raw input packet stream
- decode packets into a controller-agnostic state model

## Build targets

- `joycon_transport`
  - reusable static library

## Typical usage

1. Include `JoyconSdk.h`
2. Connect with `ConnectJoyCon(...)` or `ConnectToFirstController(...)`
3. Configure the device with `ControllerConnection::Configure(...)`
4. Start streaming with `ControllerConnection::StartInputStream(...)`
5. Decode packets with `protocol::DecodeInputPacket(...)`
