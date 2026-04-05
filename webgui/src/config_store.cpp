#include "config_store.h"

#include "config_json.h"

#include <fstream>
#include <sstream>

namespace joycon::webgui {

using json = nlohmann::json;

ConfigStore::ConfigStore(std::filesystem::path configPath)
    : configPath_(std::move(configPath)) {}

const std::filesystem::path& ConfigStore::Path() const {
    return configPath_;
}

bool ConfigStore::Save(const AppConfig& config, std::string& errorMessage) const {
    try {
        std::filesystem::create_directories(configPath_.parent_path());

        std::ofstream file(configPath_, std::ios::binary | std::ios::trunc);
        if (!file) {
            errorMessage = "Failed to open config file for writing.";
            return false;
        }

        file << ConfigToJson(config).dump(2) << "\n";

        errorMessage.clear();
        return true;
    } catch (const std::exception& ex) {
        errorMessage = ex.what();
        return false;
    }
}

bool ConfigStore::Load(AppConfig& config, std::string& errorMessage) const {
    try {
        std::ifstream file(configPath_, std::ios::binary);
        if (!file) {
            errorMessage = "Config file not found.";
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const json root = json::parse(buffer.str());
        UpdateConfigFromJson(root, config);
        errorMessage.clear();
        return true;
    } catch (const std::exception& ex) {
        errorMessage = ex.what();
        return false;
    }
}

} // namespace joycon::webgui
