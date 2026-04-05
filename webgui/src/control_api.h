#pragma once

#include "config_store.h"
#include "mapper_runtime.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>

namespace joycon::webgui {

class ControlApiServer {
public:
    ControlApiServer(MapperRuntime& runtime, ConfigStore& configStore, std::filesystem::path staticRoot);
    ~ControlApiServer();

    ControlApiServer(const ControlApiServer&) = delete;
    ControlApiServer& operator=(const ControlApiServer&) = delete;

    bool Start(uint16_t port, std::string& errorMessage);
    void Stop();

    [[nodiscard]] uint16_t Port() const;

private:
    void AcceptLoop();
    void HandleClient(uintptr_t clientSocket);

    MapperRuntime& runtime_;
    ConfigStore& configStore_;
    std::filesystem::path staticRoot_;
    std::atomic<bool> running_{ false };
    uintptr_t listenSocket_ = 0;
    uint16_t port_ = 0;
    std::thread acceptThread_;
};

} // namespace joycon::webgui
