#pragma once

#include <string>
#include <vector>
#include <utility>

namespace switchfs::api {

struct DownloadLink {
    std::string host_name;  // "1fichier", "Gofile", "MegaUp", "Send.now", etc.
    std::string url;
    std::string filename;
};

struct GameItem {
    uint64_t id = 0;
    std::string title;
    std::string date;
    std::string cover_url;
    std::string format;     // NSP, XCI, NSZ
    std::string size;       // e.g. "4.2 GB"
    std::string year;       // e.g. "2024"
    std::string language;   // e.g. "En, Ja, Fr"
    std::vector<DownloadLink> download_links;
};

class NswgfApi {
public:
    static const std::string BASE_URL;
    static constexpr int SWITCH_CATEGORY_ID = 6829;

    /// Lấy danh sách game phân trang từ nswgf.com
    static std::vector<GameItem> getLatestGames(int page = 1, int per_page = 20);

    /// Tìm kiếm game theo tên
    static std::vector<GameItem> searchGames(const std::string& keyword, int page = 1, int per_page = 20);

    /// Phân tích nội dung HTML bài viết thành GameItem có đầy đủ link tải và metadata
    static GameItem parsePostContent(uint64_t id, const std::string& title, const std::string& date, const std::string& html_content);
};

} // namespace switchfs::api
