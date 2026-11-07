#pragma once

#include "api/nswgf.hpp"
#include <string>
#include <vector>

namespace switchfs::api {

class RommApi {
public:
    /// Normalize RomM base server URL (e.g. "http://192.168.1.50:8080")
    static std::string normalizeBaseUrl(std::string url);

    /// Fetch paginated ROMs from self-hosted RomM server
    static std::vector<GameItem> getRoms(int page = 1, int per_page = 20, const std::string& search_term = "");

    /// Parse a RomM /api/roms JSON payload (array or {"items": [...]}) into GameItem list
    static std::vector<GameItem> parseRomsResponse(const std::string& json_str, const std::string& base_url);

    /// Build Basic or Bearer authorization header for RomM requests and downloads
    static std::string getAuthHeader();
};

} // namespace switchfs::api
