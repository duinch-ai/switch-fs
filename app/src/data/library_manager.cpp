#include "data/library_manager.hpp"
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace switchfs::data {

std::string LibraryManager::getLibraryPath() {
#if defined(__SWITCH__)
    return "sdmc:/switch/SwitchFS/library.json";
#else
    return "./library.json";
#endif
}

LibraryManager& LibraryManager::getInstance() {
    static LibraryManager instance;
    return instance;
}

LibraryManager::LibraryManager() {
    this->load();
}

static nlohmann::json gameToJson(const api::GameItem& g) {
    nlohmann::json j;
    j["id"] = g.id;
    j["title"] = g.title;
    j["date"] = g.date;
    j["cover_url"] = g.cover_url;
    j["format"] = g.format;
    j["size"] = g.size;
    j["language"] = g.language;
    nlohmann::json links = nlohmann::json::array();
    for (const auto& dl : g.download_links) {
        links.push_back({{"host_name", dl.host_name}, {"url", dl.url}, {"filename", dl.filename}});
    }
    j["download_links"] = links;
    return j;
}

static api::GameItem jsonToGame(const nlohmann::json& j) {
    api::GameItem g;
    g.id = j.value("id", 0ULL);
    g.title = j.value("title", "");
    g.date = j.value("date", "");
    g.cover_url = j.value("cover_url", "");
    g.format = j.value("format", "NSP");
    g.size = j.value("size", "Unknown");
    g.language = j.value("language", "Multi");
    if (j.contains("download_links") && j["download_links"].is_array()) {
        for (const auto& l : j["download_links"]) {
            api::DownloadLink dl;
            dl.host_name = l.value("host_name", "");
            dl.url = l.value("url", "");
            dl.filename = l.value("filename", "");
            g.download_links.push_back(dl);
        }
    }
    return g;
}

void LibraryManager::load() {
    std::ifstream file(getLibraryPath());
    if (!file.is_open()) return;
    try {
        nlohmann::json j;
        file >> j;
        this->favorites.clear();
        this->installed_games.clear();
        if (j.contains("favorites") && j["favorites"].is_array()) {
            for (const auto& node : j["favorites"]) {
                this->favorites.push_back(jsonToGame(node));
            }
        }
        if (j.contains("installed") && j["installed"].is_array()) {
            for (const auto& node : j["installed"]) {
                this->installed_games.push_back(jsonToGame(node));
            }
        }
    } catch (const std::exception& e) {
        brls::Logger::error("LibraryManager load error: {}", e.what());
    }
}

void LibraryManager::save() {
    try {
        nlohmann::json j;
        j["favorites"] = nlohmann::json::array();
        for (const auto& g : this->favorites) j["favorites"].push_back(gameToJson(g));
        j["installed"] = nlohmann::json::array();
        for (const auto& g : this->installed_games) j["installed"].push_back(gameToJson(g));

        std::ofstream file(getLibraryPath());
        if (file.is_open()) {
            file << j.dump(2);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("LibraryManager save error: {}", e.what());
    }
}

bool LibraryManager::isFavorite(uint64_t id, const std::string& title) const {
    for (const auto& g : this->favorites) {
        if ((id != 0 && g.id == id) || (!title.empty() && g.title == title)) {
            return true;
        }
    }
    return false;
}

bool LibraryManager::toggleFavorite(const api::GameItem& item) {
    for (auto it = this->favorites.begin(); it != this->favorites.end(); ++it) {
        if ((item.id != 0 && it->id == item.id) || (!item.title.empty() && it->title == item.title)) {
            this->favorites.erase(it);
            this->save();
            return false;
        }
    }
    this->favorites.insert(this->favorites.begin(), item);
    this->save();
    return true;
}

void LibraryManager::recordInstalled(const api::GameItem& item) {
    for (const auto& g : this->installed_games) {
        if ((item.id != 0 && g.id == item.id) || (!item.title.empty() && g.title == item.title)) {
            return;
        }
    }
    this->installed_games.insert(this->installed_games.begin(), item);
    this->save();
}

std::vector<LocalDownloadedFile> LibraryManager::getDownloadedFiles() const {
    std::vector<LocalDownloadedFile> files;
#if defined(__SWITCH__)
    std::string dir = "sdmc:/switch/SwitchFS/downloads/";
#else
    std::string dir = "./downloads/";
#endif
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) return files;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string fname = entry.path().filename().string();
        std::string lower = fname;
        for (char& c : lower) c = (char)std::tolower((unsigned char)c);

        bool is_nsp = (lower.rfind(".nsp") != std::string::npos ||
                       lower.rfind(".nsz") != std::string::npos ||
                       lower.rfind(".xci") != std::string::npos);
        bool is_arc = (lower.rfind(".rar") != std::string::npos ||
                       lower.rfind(".zip") != std::string::npos ||
                       lower.rfind(".7z") != std::string::npos);
        if (!is_nsp && !is_arc) continue;

        LocalDownloadedFile lf;
        lf.filename = fname;
        lf.full_path = entry.path().string();
        lf.size_bytes = (int64_t)entry.file_size(ec);
        lf.is_installable_nsp = is_nsp;

        double gb = (double)lf.size_bytes / (1024.0 * 1024.0 * 1024.0);
        std::ostringstream oss;
        if (gb >= 1.0) {
            oss << std::fixed << std::setprecision(2) << gb << " GB";
        } else {
            double mb = (double)lf.size_bytes / (1024.0 * 1024.0);
            oss << std::fixed << std::setprecision(1) << mb << " MB";
        }
        lf.size_str = oss.str();
        files.push_back(lf);
    }
    return files;
}

bool LibraryManager::deleteDownloadedFile(const std::string& full_path) const {
    return remove(full_path.c_str()) == 0;
}

} // namespace switchfs::data
