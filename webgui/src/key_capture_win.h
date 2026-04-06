#pragma once

#include <chrono>
#include <string>

namespace joycon::webgui {

struct CaptureKeyResult {
    bool ok = false;
    std::string token;
    std::string action;
    std::string error;
};

CaptureKeyResult CaptureKeyCustomToken(std::chrono::milliseconds timeout = std::chrono::milliseconds(45000));

} // namespace joycon::webgui
