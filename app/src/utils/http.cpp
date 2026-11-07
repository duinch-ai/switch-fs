#include "utils/http.hpp"
#include <curl/curl.h>
#include <borealis.hpp>

namespace switchfs::utils {

std::atomic<bool> HttpClient::abort_all{false};

void HttpClient::setAbortAll(bool abort) {
    abort_all.store(abort);
}

bool HttpClient::isAborting() {
    return abort_all.load();
}

static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    if (HttpClient::isAborting()) return 0;
    size_t realsize = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), realsize);
    return realsize;
}

static int abortXferCallback(void*, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return HttpClient::isAborting() ? 1 : 0;
}

std::string HttpClient::get(const std::string& url, int timeout_seconds) {
    if (isAborting()) return "";

    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_seconds);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; SwitchFS/0.1.0)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, abortXferCallback);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        if (!isAborting()) {
            brls::Logger::error("HTTP GET failed: {} - {}", url, curl_easy_strerror(res));
        }
        response.clear();
    }

    curl_easy_cleanup(curl);
    return response;
}

} // namespace switchfs::utils
