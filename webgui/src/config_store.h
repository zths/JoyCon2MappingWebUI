#pragma once

#include "mapper_runtime.h"

#include <filesystem>
#include <string>

namespace joycon::webgui {

class ConfigStore {
public:
    explicit ConfigStore(std::filesystem::path configPath);

    [[nodiscard]] const std::filesystem::path& Path() const;
    [[nodiscard]] bool Save(const AppConfig& config, std::string& errorMessage) const;
    [[nodiscard]] bool Load(AppConfig& config, std::string& errorMessage) const;

private:
    std::filesystem::path configPath_;
};

} // namespace joycon::webgui
