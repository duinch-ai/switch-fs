#include "api/nswgf.hpp"
#include "utils/http.hpp"
#include <curl/curl.h>
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <cctype>
#include <algorithm>

namespace switchfs::api {

const std::string NswgfApi::BASE_URL = "https://nswgf.com/wp-json/wp/v2";

static std::string decodeHtmlEntities(std::string str) {
    static const std::vector<std::pair<std::string, std::string>> entities = {
        {"&amp;", "&"},
        {"&quot;", "\""},
        {"&#039;", "'"},
        {"&apos;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&#8211;", "-"},
        {"&#8217;", "'"},
        {"&#8220;", "\""},
        {"&#8221;", "\""},
    };
    for (const auto& [entity, replacement] : entities) {
        size_t pos = 0;
        while ((pos = str.find(entity, pos)) != std::string::npos) {
            str.replace(pos, entity.length(), replacement);
            pos += replacement.length();
        }
    }
    return str;
}

static std::string extractCoverUrl(const std::string& html) {
    size_t img_pos = html.find("<img");
    if (img_pos != std::string::npos) {
        size_t src_pos = html.find("src=", img_pos);
        if (src_pos != std::string::npos) {
            src_pos += 4;
            if (src_pos < html.length()) {
                char quote = html[src_pos];
                if (quote == '"' || quote == '\'') {
                    src_pos++;
                    size_t end_quote = html.find(quote, src_pos);
                    if (end_quote != std::string::npos) {
                        std::string url = html.substr(src_pos, end_quote - src_pos);
                        while (!url.empty() && (url.back() == ' ' || url.back() == '\t' || url.back() == '\r' || url.back() == '\n')) {
                            url.pop_back();
                        }
                        while (!url.empty() && (url.front() == ' ' || url.front() == '\t')) {
                            url.erase(0, 1);
                        }
                        // Downscale Google Blogger /s1600/ image to /s240/ thumbnail (~98% less RAM/VRAM)
                        size_t s1600 = url.find("/s1600/");
                        if (s1600 != std::string::npos) {
                            url.replace(s1600, 7, "/s240/");
                        }
                        return url;
                    }
                }
            }
        }
    }
    return "";
}

static std::string extractSize(const std::string& html) {
    std::string lower = html;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);

    size_t pos = lower.find("size");
    if (pos == std::string::npos) pos = lower.find("dung l");
    if (pos != std::string::npos) {
        size_t window_end = std::min(lower.length(), pos + 300);
        for (const std::string& unit : {"gb", "mb", "kb"}) {
            size_t u_pos = lower.find(unit, pos);
            if (u_pos != std::string::npos && u_pos < window_end) {
                size_t num_end = u_pos;
                while (num_end > pos && (lower[num_end - 1] == ' ' || lower[num_end - 1] == '\t')) {
                    num_end--;
                }
                size_t num_start = num_end;
                while (num_start > pos && (std::isdigit((unsigned char)lower[num_start - 1]) || lower[num_start - 1] == '.')) {
                    num_start--;
                }
                if (num_start < num_end) {
                    std::string num_str = html.substr(num_start, num_end - num_start);
                    std::string unit_str = html.substr(u_pos, unit.length());
                    for (char& c : unit_str) c = (char)std::toupper((unsigned char)c);
                    return num_str + " " + unit_str;
                }
            }
        }
    }
    return "Unknown";
}

static std::string extractLanguage(const std::string& html) {
    std::string lower = html;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);

    size_t pos = lower.find("language");
    if (pos == std::string::npos) pos = lower.find("ngôn ng");
    if (pos != std::string::npos) {
        size_t window_end = std::min(lower.length(), pos + 250);
        size_t colon = lower.find(':', pos);
        if (colon != std::string::npos && colon < window_end) {
            pos = colon + 1;
        } else {
            size_t tag = lower.find('>', pos);
            if (tag != std::string::npos && tag < window_end) {
                pos = tag + 1;
            }
        }
        while (pos < window_end && (html[pos] == ' ' || html[pos] == '<')) {
            if (html[pos] == '<') {
                size_t close = html.find('>', pos);
                if (close == std::string::npos || close >= window_end) break;
                pos = close + 1;
            } else {
                pos++;
            }
        }
        size_t val_end = pos;
        while (val_end < window_end && html[val_end] != '<' && html[val_end] != '\n' && html[val_end] != '\r') {
            val_end++;
        }
        if (val_end > pos) {
            std::string lang = html.substr(pos, val_end - pos);
            while (!lang.empty() && (lang.front() == ' ' || lang.front() == ':')) lang.erase(0, 1);
            while (!lang.empty() && (lang.back() == ' ' || lang.back() == ',')) lang.pop_back();
            if (!lang.empty()) return lang;
        }
    }
    return "Multi";
}

static std::vector<DownloadLink> extractDownloadLinks(const std::string& html) {
    std::vector<DownloadLink> links;
    static const std::vector<std::string> hosts = {
        "gofile.io", "1fichier.com", "send.now", "megaup.net", "datanodes.to", "mediafire.com"
    };

    size_t pos = 0;
    while ((pos = html.find("href=", pos)) != std::string::npos) {
        pos += 5;
        if (pos >= html.length()) break;
        char quote = html[pos];
        if (quote != '"' && quote != '\'') continue;
        pos++;
        size_t end_quote = html.find(quote, pos);
        if (end_quote == std::string::npos) break;

        std::string raw_url = html.substr(pos, end_quote - pos);
        pos = end_quote + 1;

        for (const auto& h : hosts) {
            if (raw_url.find(h) != std::string::npos) {
                size_t amp_pos = 0;
                while ((amp_pos = raw_url.find("&amp;", amp_pos)) != std::string::npos) {
                    raw_url.replace(amp_pos, 5, "&");
                    amp_pos += 1;
                }

                DownloadLink dl;
                dl.url = raw_url;
                if (h == "gofile.io") dl.host_name = "Gofile";
                else if (h == "1fichier.com") dl.host_name = "1fichier";
                else if (h == "send.now") dl.host_name = "Send.now";
                else if (h == "megaup.net") dl.host_name = "MegaUp";
                else if (h == "datanodes.to") dl.host_name = "Datanodes";
                else dl.host_name = "Direct";

                links.push_back(dl);
                break;
            }
        }
    }
    return links;
}

GameItem NswgfApi::parsePostContent(uint64_t id, const std::string& title, const std::string& date, const std::string& html_content) {
    GameItem item;
    item.id = id;
    item.title = decodeHtmlEntities(title);
    item.date = date.substr(0, 10);

    // 1. Cover URL
    item.cover_url = extractCoverUrl(html_content);

    // 2. Format (NSP, XCI, NSZ)
    if (html_content.find("XCI") != std::string::npos) item.format = "XCI";
    else if (html_content.find("NSZ") != std::string::npos) item.format = "NSZ";
    else item.format = "NSP";

    // 3. Size
    item.size = extractSize(html_content);

    // 4. Language
    item.language = extractLanguage(html_content);

    // 5. Download Links
    item.download_links = extractDownloadLinks(html_content);

    return item;
}

std::vector<GameItem> NswgfApi::getLatestGames(int page, int per_page) {
    std::string url = BASE_URL + "/posts?categories=" + std::to_string(SWITCH_CATEGORY_ID) +
                      "&per_page=" + std::to_string(per_page) +
                      "&page=" + std::to_string(page);

    std::string response = utils::HttpClient::get(url);
    if (response.empty()) {
        brls::Logger::error("NswgfApi::getLatestGames: rỗng hoặc lỗi kết nối");
        return {};
    }

    std::vector<GameItem> games;
    try {
        auto json_arr = nlohmann::json::parse(response);
        for (const auto& post : json_arr) {
            uint64_t id = post.value("id", 0ULL);
            std::string title = post["title"].value("rendered", "");
            std::string date = post.value("date", "");
            std::string content = post["content"].value("rendered", "");
            games.push_back(parsePostContent(id, title, date, content));
        }
    } catch (const std::exception& e) {
        brls::Logger::error("NswgfApi::getLatestGames JSON parse error: {}", e.what());
    }

    return games;
}

std::vector<GameItem> NswgfApi::searchGames(const std::string& keyword, int page, int per_page) {
    CURL* curl = curl_easy_init();
    char* escaped = curl ? curl_easy_escape(curl, keyword.c_str(), keyword.length()) : nullptr;
    std::string encoded_query = escaped ? escaped : keyword;
    if (escaped) curl_free(escaped);
    if (curl) curl_easy_cleanup(curl);

    std::string url = BASE_URL + "/posts?categories=" + std::to_string(SWITCH_CATEGORY_ID) +
                      "&search=" + encoded_query +
                      "&per_page=" + std::to_string(per_page) +
                      "&page=" + std::to_string(page);

    std::string response = utils::HttpClient::get(url);
    if (response.empty()) return {};

    std::vector<GameItem> games;
    try {
        auto json_arr = nlohmann::json::parse(response);
        for (const auto& post : json_arr) {
            uint64_t id = post.value("id", 0ULL);
            std::string title = post["title"].value("rendered", "");
            std::string date = post.value("date", "");
            std::string content = post["content"].value("rendered", "");
            games.push_back(parsePostContent(id, title, date, content));
        }
    } catch (const std::exception& e) {
        brls::Logger::error("NswgfApi::searchGames JSON parse error: {}", e.what());
    }

    return games;
}

} // namespace switchfs::api
