#pragma once

#include <string>

namespace switchfs::utils {

class HttpClient {
public:
    /// Thực hiện HTTP GET và trả về body string
    static std::string get(const std::string& url, int timeout_seconds = 15);

    /// Tải file về đường dẫn đích với callback tiến độ
    static bool downloadFile(const std::string& url, const std::string& dest_path,
                             void (*progress_cb)(size_t downloaded, size_t total) = nullptr);
};

} // namespace switchfs::utils
