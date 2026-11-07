#pragma once

#include "api/nswgf.hpp"
#include <string>
#include <vector>

namespace switchfs::data {

struct LocalDownloadedFile {
    std::string filename;
    std::string full_path;
    std::string size_str;
    int64_t size_bytes = 0;
    bool is_installable_nsp = false;
};

class LibraryManager {
public:
    static LibraryManager& getInstance();

    // Favorites management
    bool isFavorite(uint64_t id, const std::string& title) const;
    bool toggleFavorite(const api::GameItem& item);
    const std::vector<api::GameItem>& getFavorites() const { return favorites; }

    // Installed games tracking
    void recordInstalled(const api::GameItem& item);
    const std::vector<api::GameItem>& getInstalledGames() const { return installed_games; }

    // Downloaded files scanner
    std::vector<LocalDownloadedFile> getDownloadedFiles() const;
    bool deleteDownloadedFile(const std::string& full_path) const;

    void save();
    void load();
    static std::string getLibraryPath();

private:
    LibraryManager();
    std::vector<api::GameItem> favorites;
    std::vector<api::GameItem> installed_games;
};

} // namespace switchfs::data
