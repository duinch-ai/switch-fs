#pragma once

#include <string>
#include <atomic>

namespace switchfs::utils {

class HttpClient {
public:
    static void setAbortAll(bool abort);
    static bool isAborting();

    /// Thực hiện HTTP GET và trả về body string
    static std::string get(const std::string& url, int timeout_seconds = 10);

    /// Tải file về đường dẫn đích với callback tiến độ
    static bool downloadFile(const std::string& url, const std::string& dest_path,
                             void (*progress_cb)(size_t downloaded, size_t total) = nullptr);

private:
    static std::atomic<bool> abort_all;
};

} // namespace switchfs::utils
