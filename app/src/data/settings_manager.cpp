#include "data/settings_manager.hpp"
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

namespace switchfs::data {

std::string SettingsManager::getConfigPath() {
#if defined(__SWITCH__)
    return "sdmc:/switch/SwitchFS/config.json";
#else
    return "./config.json";
#endif
}

SettingsManager& SettingsManager::getInstance() {
    static SettingsManager instance;
    return instance;
}

SettingsManager::SettingsManager() {
    this->load();
}

void SettingsManager::load() {
    std::string path = getConfigPath();
    std::ifstream file(path);
    if (!file.is_open()) return;

    try {
        nlohmann::json j;
        file >> j;
        this->settings.default_storage = j.value("default_storage", 0);
        this->settings.auto_install = j.value("auto_install", true);
        this->settings.auto_delete_archive = j.value("auto_delete_archive", true);
        this->settings.gofile_token = j.value("gofile_token", "");
        this->settings.onefichier_key = j.value("onefichier_key", "");
        this->settings.debrid_key = j.value("debrid_key", "");
        brls::Logger::info("SettingsManager: Đã nạp cấu hình từ {}", path);
    } catch (const std::exception& e) {
        brls::Logger::error("SettingsManager: Lỗi đọc file cấu hình: {}", e.what());
    }
}

void SettingsManager::save() {
    std::string path = getConfigPath();
    try {
        nlohmann::json j;
        j["default_storage"] = this->settings.default_storage;
        j["auto_install"] = this->settings.auto_install;
        j["auto_delete_archive"] = this->settings.auto_delete_archive;
        j["gofile_token"] = this->settings.gofile_token;
        j["onefichier_key"] = this->settings.onefichier_key;
        j["debrid_key"] = this->settings.debrid_key;

        std::ofstream file(path);
        if (file.is_open()) {
            file << j.dump(4);
            brls::Logger::info("SettingsManager: Đã lưu cấu hình vào {}", path);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("SettingsManager: Lỗi lưu file cấu hình: {}", e.what());
    }
}

} // namespace switchfs::data
