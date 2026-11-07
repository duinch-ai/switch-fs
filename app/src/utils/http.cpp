#include "utils/http.hpp"
#include <curl/curl.h>
#include <borealis.hpp>

namespace switchfs::utils {

static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), realsize);
    return realsize;
}

std::string HttpClient::get(const std::string& url, int timeout_seconds) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; SwitchFS/0.1.0)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Tránh lỗi cert trên console
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        brls::Logger::error("HTTP GET failed: {} - {}", url, curl_easy_strerror(res));
        response.clear();
    }

    curl_easy_cleanup(curl);
    return response;
}

} // namespace switchfs::utils
