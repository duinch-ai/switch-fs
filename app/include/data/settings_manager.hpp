#pragma once

#include <string>

namespace switchfs::data {

struct AppSettings {
    int default_storage = 0; // 0 = SD Card, 1 = NAND
    bool auto_install = true;
    bool auto_delete_archive = true;
    std::string gofile_token;
    std::string onefichier_key;
    std::string debrid_key;
};

class SettingsManager {
public:
    static SettingsManager& getInstance();

    const AppSettings& getSettings() const { return settings; }
    AppSettings& getSettings() { return settings; }

    void save();
    void load();

    static std::string getConfigPath();

private:
    SettingsManager();
    AppSettings settings;
};

} // namespace switchfs::data
