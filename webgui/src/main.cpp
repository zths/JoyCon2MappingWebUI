#include "config_json.h"
#include "config_store.h"
#include "control_api.h"
#include "mapper_runtime.h"

#include <winrt/base.h>

#include <Windows.h>
#include <shellapi.h>
#include <conio.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, length)).parent_path();
}

uint16_t ConfiguredPort(const joycon::webgui::MapperRuntime& runtime) {
    const auto config = runtime.CurrentConfig();
    return config.server.port == 0 ? 17777 : config.server.port;
}

std::wstring LocalUrl(uint16_t port) {
    return L"http://127.0.0.1:" + std::to_wstring(port) + L"/";
}

} // namespace

int main() {
    winrt::init_apartment();

    joycon::webgui::MapperRuntime runtime;

    const std::filesystem::path executableDir = ExecutableDirectory();
    const std::filesystem::path configPath = executableDir / "config.json";
    joycon::webgui::ConfigStore configStore(configPath);

    auto config = runtime.CurrentConfig();
    const bool configFileExists = std::filesystem::exists(configPath);
    std::string loadError;
    if (configStore.Load(config, loadError)) {
        runtime.ApplyConfig(config);
        std::string saveError;
        if (!configStore.Save(config, saveError)) {
            std::cerr << "Failed to backfill config file defaults: " << saveError << "\n";
        }
    } else if (!configFileExists) {
        std::string saveError;
        if (!configStore.Save(config, saveError)) {
            std::cerr << "Failed to write default config file: " << saveError << "\n";
        }
    }

    joycon::webgui::ControlApiServer server(
        runtime,
        configStore,
        executableDir / "web");

    std::string error;
    const uint16_t initialPort = ConfiguredPort(runtime);
    if (!server.Start(initialPort, error)) {
        std::cerr << "Failed to start Web GUI server: " << error << "\n";
        return 1;
    }

    const std::wstring url = LocalUrl(server.Port());
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);

    std::wcout << L"Web GUI running at " << url << L"\n";
    std::wcout << L"Press Enter to stop.\n";

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const uint16_t desiredPort = ConfiguredPort(runtime);
        if (desiredPort != server.Port()) {
            server.Stop();
            if (!server.Start(desiredPort, error)) {
                std::cerr << "Failed to switch Web GUI server port to " << desiredPort << ": " << error << "\n";
                return 1;
            }

            std::wcout << L"Web GUI moved to " << LocalUrl(server.Port()) << L"\n";
        }

        if (_kbhit()) {
            std::string dummy;
            std::getline(std::cin, dummy);
            break;
        }
    }

    server.Stop();
    return 0;
}
