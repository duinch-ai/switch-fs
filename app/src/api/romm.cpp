#include "api/romm.hpp"
#include "data/settings_manager.hpp"
#include <curl/curl.h>
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace switchfs::api {

static size_t rommWriteCb(void* ptr, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(ptr), total);
    return total;
}

static std::string base64Encode(const std::string& in) {
    static const char* b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(b64[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string RommApi::normalizeBaseUrl(std::string url) {
    while (!url.empty() && (url.back() == '/' || url.back() == ' ')) url.pop_back();
    while (!url.empty() && url.front() == ' ') url.erase(0, 1);
    if (!url.empty() && url.find("http://") != 0 && url.find("https://") != 0) {
        url = "http://" + url;
    }
    if (url.size() > 4 && url.substr(url.size() - 4) == "/api") {
        url = url.substr(0, url.size() - 4);
    }
    return url;
}

std::string RommApi::getAuthHeader() {
    const auto& s = data::SettingsManager::getInstance().getSettings();
    if (!s.romm_token.empty()) {
        return "Authorization: Bearer " + s.romm_token;
    }
    if (!s.romm_username.empty() || !s.romm_password.empty()) {
        return "Authorization: Basic " + base64Encode(s.romm_username + ":" + s.romm_password);
    }
    return "";
}

static std::string formatBytesSize(int64_t bytes) {
    if (bytes <= 0) return "Unknown";
    double gb = (double)bytes / (1024.0 * 1024.0 * 1024.0);
    std::ostringstream oss;
    if (gb >= 1.0) {
        oss << std::fixed << std::setprecision(2) << gb << " GB";
    } else {
        double mb = (double)bytes / (1024.0 * 1024.0);
        oss << std::fixed << std::setprecision(1) << mb << " MB";
    }
    return oss.str();
}

static GameItem parseSingleRomNode(const nlohmann::json& node, const std::string& base_url) {
    GameItem item;
    item.id = node.value("id", 0ULL);
    std::string fs_name = node.value("fs_name", "");
    std::string name = node.value("name", "");
    if (name.empty()) name = node.value("fs_name_no_ext", fs_name);
    item.title = name.empty() ? "Untitled ROM" : name;

    int64_t size_bytes = node.value("fs_size_bytes", 0LL);
    item.size = formatBytesSize(size_bytes);

    std::string ext = node.value("fs_extension", "");
    if (ext.empty() && fs_name.rfind('.') != std::string::npos) {
        ext = fs_name.substr(fs_name.rfind('.') + 1);
    }
    for (char& c : ext) c = (char)std::toupper((unsigned char)c);
    item.format = ext.empty() ? "NSP" : ext;

    std::string updated = node.value("updated_at", node.value("created_at", "RomM LAN"));
    item.date = updated.size() >= 10 ? updated.substr(0, 10) : "RomM LAN";
    item.language = node.value("reg_code", "Multi");

    // Cover URL: prefer small cached cover path on RomM server for low RAM
    std::string cover = node.value("path_cover_s", "");
    if (cover.empty()) cover = node.value("path_cover_small", "");
    if (cover.empty()) cover = node.value("url_cover", "");
    if (!cover.empty() && cover.front() == '/') {
        cover = base_url + cover;
    }
    item.cover_url = cover;

    // Build direct RomM content download URL: /api/roms/{id}/content/{fs_name}
    if (item.id > 0) {
        std::string safe_fname = fs_name.empty() ? (item.title + ".nsp") : fs_name;
        CURL* curl = curl_easy_init();
        char* esc = curl ? curl_easy_escape(curl, safe_fname.c_str(), (int)safe_fname.length()) : nullptr;
        std::string encoded_fname = esc ? esc : safe_fname;
        if (esc) curl_free(esc);
        if (curl) curl_easy_cleanup(curl);

        DownloadLink dl;
        dl.host_name = "RomM";
        dl.filename = safe_fname;
        dl.url = base_url + "/api/roms/" + std::to_string(item.id) + "/content/" + encoded_fname;
        item.download_links.push_back(dl);
    }

    return item;
}

std::vector<GameItem> RommApi::parseRomsResponse(const std::string& json_str, const std::string& base_url) {
    std::vector<GameItem> items;
    if (json_str.empty()) return items;
    std::string norm_base = normalizeBaseUrl(base_url);

    try {
        auto root = nlohmann::json::parse(json_str);
        if (root.is_array()) {
            for (const auto& node : root) {
                if (node.is_object()) items.push_back(parseSingleRomNode(node, norm_base));
            }
        } else if (root.is_object()) {
            if (root.contains("items") && root["items"].is_array()) {
                for (const auto& node : root["items"]) {
                    if (node.is_object()) items.push_back(parseSingleRomNode(node, norm_base));
                }
            } else if (root.contains("results") && root["results"].is_array()) {
                for (const auto& node : root["results"]) {
                    if (node.is_object()) items.push_back(parseSingleRomNode(node, norm_base));
                }
            }
        }
    } catch (const std::exception& e) {
        brls::Logger::error("RommApi::parseRomsResponse JSON error: {}", e.what());
    }
    return items;
}

std::vector<GameItem> RommApi::getRoms(int page, int per_page, const std::string& search_term) {
    auto& settings = data::SettingsManager::getInstance().getSettings();
    std::string base_url = normalizeBaseUrl(settings.romm_url);
    if (base_url.empty()) return {};

    CURL* curl = curl_easy_init();
    if (!curl) return {};

    int offset = (page - 1) * per_page;
    std::string url = base_url + "/api/roms?limit=" + std::to_string(per_page) + "&offset=" + std::to_string(offset);
    if (!search_term.empty()) {
        char* esc = curl_easy_escape(curl, search_term.c_str(), (int)search_term.length());
        if (esc) {
            url += "&search_term=" + std::string(esc);
            curl_free(esc);
        }
    }

    auto executeGet = [&](const std::string& auth_hdr) -> std::string {
        std::string resp;
        struct curl_slist* hdrs = curl_slist_append(nullptr, "Accept: application/json");
        if (!auth_hdr.empty()) hdrs = curl_slist_append(hdrs, auth_hdr.c_str());

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 12L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rommWriteCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
        curl_easy_perform(curl);
        curl_slist_free_all(hdrs);
        return resp;
    };

    std::string response = executeGet(getAuthHeader());

    // If Basic Auth returned 401/403 and username/password are set, try OAuth2 /api/token exchange
    if ((response.empty() || response.find("\"detail\"") != std::string::npos) &&
        !settings.romm_username.empty() && settings.romm_token.empty()) {
        std::string tok_resp;
        char* eu = curl_easy_escape(curl, settings.romm_username.c_str(), (int)settings.romm_username.length());
        char* ep = curl_easy_escape(curl, settings.romm_password.c_str(), (int)settings.romm_password.length());
        std::string post_fields = "grant_type=password&username=" + std::string(eu ? eu : "") +
                                  "&password=" + std::string(ep ? ep : "") +
                                  "&scope=roms.read+platforms.read";
        if (eu) curl_free(eu);
        if (ep) curl_free(ep);

        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, (base_url + "/api/token").c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rommWriteCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &tok_resp);
        if (curl_easy_perform(curl) == CURLE_OK && !tok_resp.empty()) {
            try {
                auto tj = nlohmann::json::parse(tok_resp);
                std::string access_tok = tj.value("access_token", "");
                if (!access_tok.empty()) {
                    settings.romm_token = access_tok;
                    data::SettingsManager::getInstance().save();
                    response = executeGet(getAuthHeader());
                }
            } catch (...) {}
        }
    }

    curl_easy_cleanup(curl);
    return parseRomsResponse(response, base_url);
}

} // namespace switchfs::api
