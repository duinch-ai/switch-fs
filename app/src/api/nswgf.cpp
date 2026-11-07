#include "api/nswgf.hpp"
#include "utils/http.hpp"
#include <curl/curl.h>
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <regex>

namespace switchfs::api {

const std::string NswgfApi::BASE_URL = "https://nswgf.com/wp-json/wp/v2";

static std::string decodeHtmlEntities(std::string str) {
    // Decode common HTML entities like &#8211;, &amp;, etc.
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

GameItem NswgfApi::parsePostContent(uint64_t id, const std::string& title, const std::string& date, const std::string& html_content) {
    GameItem item;
    item.id = id;
    item.title = decodeHtmlEntities(title);
    item.date = date.substr(0, 10); // Lấy YYYY-MM-DD

    // 1. Bóc tách ảnh bìa (thẻ img đầu tiên)
    std::regex img_regex(R"(<img[^>]+src=["']([^"'<>\s]+)["'])", std::regex::icase);
    std::smatch img_match;
    if (std::regex_search(html_content, img_match, img_regex) && img_match.size() > 1) {
        item.cover_url = img_match[1].str();
    }

    // 2. Bóc tách Format (NSP, XCI, NSZ)
    if (html_content.find("XCI") != std::string::npos) item.format = "XCI";
    else if (html_content.find("NSZ") != std::string::npos) item.format = "NSZ";
    else item.format = "NSP";

    // 3. Bóc tách Size (Dung lượng)
    std::regex size_regex(R"((?:size|dung lượng)[\s\S]*?(?:<td[^>]*>[\s\S]*?<span[^>]*>|:\s*)([\d\.]+\s*(?:GB|MB|KB)))", std::regex::icase);
    std::smatch size_match;
    if (std::regex_search(html_content, size_match, size_regex) && size_match.size() > 1) {
        item.size = size_match[1].str();
    } else {
        item.size = "Unknown";
    }

    // 4. Bóc tách Language
    std::regex lang_regex(R"((?:language|languages|ngôn ngữ)[\s\S]*?(?:<td[^>]*>[\s\S]*?<span[^>]*>|:\s*)([a-zA-Z,\s]+?)(?:<|$|\n|\r))", std::regex::icase);
    std::smatch lang_match;
    if (std::regex_search(html_content, lang_match, lang_regex) && lang_match.size() > 1) {
        std::string l = lang_match[1].str();
        // Trim trailing spaces/punctuation
        while (!l.empty() && (l.back() == ' ' || l.back() == ',')) l.pop_back();
        item.language = l;
    } else {
        item.language = "Multi";
    }

    // 5. Bóc tách danh sách Download Host links
    std::regex link_regex(R"(href=["'](https?://(?:1fichier|megaup|gofile|send\.now|datanodes|mediafire)[^"'<>\s]+)["'])", std::regex::icase);
    auto words_begin = std::sregex_iterator(html_content.begin(), html_content.end(), link_regex);
    auto words_end = std::sregex_iterator();

    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        std::string raw_url = match[1].str();
        // Decode &amp; in URLs
        size_t amp_pos = 0;
        while ((amp_pos = raw_url.find("&amp;", amp_pos)) != std::string::npos) {
            raw_url.replace(amp_pos, 5, "&");
            amp_pos += 1;
        }

        DownloadLink dl;
        dl.url = raw_url;

        if (raw_url.find("gofile.io") != std::string::npos) dl.host_name = "Gofile";
        else if (raw_url.find("1fichier.com") != std::string::npos) dl.host_name = "1fichier";
        else if (raw_url.find("send.now") != std::string::npos) dl.host_name = "Send.now";
        else if (raw_url.find("megaup.net") != std::string::npos) dl.host_name = "MegaUp";
        else if (raw_url.find("datanodes.to") != std::string::npos) dl.host_name = "Datanodes";
        else dl.host_name = "Direct";

        item.download_links.push_back(dl);
    }

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
