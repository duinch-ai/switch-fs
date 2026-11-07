#include "data/remote/host_resolver.hpp"
#include "data/settings_manager.hpp"
#include <curl/curl.h>
#include <borealis.hpp>
#include <borealis/extern/nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <map>

namespace switchfs::data {

static const char* BROWSER_UA =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36";

static size_t writeStrCb(void* ptr, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(ptr), total);
    return total;
}

static size_t headerLocCb(char* buffer, size_t size, size_t nitems, void* userp) {
    size_t total = size * nitems;
    std::string line(buffer, total);
    std::string lower = line;
    for (char& c : lower) c = (char)std::tolower((unsigned char)c);
    if (lower.rfind("location:", 0) == 0) {
        std::string loc = line.substr(9);
        while (!loc.empty() && (loc.front() == ' ' || loc.front() == '\t')) loc.erase(0, 1);
        while (!loc.empty() && (loc.back() == '\r' || loc.back() == '\n' || loc.back() == ' ')) loc.pop_back();
        *static_cast<std::string*>(userp) = loc;
    }
    return total;
}

static void setupBaseCurl(CURL* curl, const std::string& url, std::string* response_buf) {
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, BROWSER_UA);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStrCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buf);
}

static bool sleepAbortable(int milliseconds, std::atomic<bool>* should_abort) {
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 200) {
        if (should_abort && should_abort->load()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    return true;
}

std::string HostResolver::sha256Hex(const std::string& input) {
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    auto rotr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    std::vector<uint8_t> msg(input.begin(), input.end());
    uint64_t bit_len = (uint64_t)msg.size() * 8;
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i) msg.push_back((uint8_t)((bit_len >> (i * 8)) & 0xFF));

    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = ((uint32_t)msg[offset + i * 4] << 24) | ((uint32_t)msg[offset + i * 4 + 1] << 16) |
                   ((uint32_t)msg[offset + i * 4 + 2] << 8) | ((uint32_t)msg[offset + i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;
            hh = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) oss << std::hex << std::setw(8) << std::setfill('0') << h[i];
    return oss.str();
}

static std::string extractAttr(const std::string& tag, const std::string& attr) {
    std::string pat = attr + "=\"";
    size_t pos = tag.find(pat);
    char q = '"';
    if (pos == std::string::npos) { pat = attr + "='"; pos = tag.find(pat); q = '\''; }
    if (pos == std::string::npos) return "";
    pos += pat.length();
    size_t end = tag.find(q, pos);
    return (end != std::string::npos) ? tag.substr(pos, end - pos) : "";
}

static std::map<std::string, std::string> parseFormInputs(const std::string& html) {
    std::map<std::string, std::string> fields;
    std::string scope = html;
    size_t f_pos = 0;
    while ((f_pos = html.find("<form", f_pos)) != std::string::npos) {
        size_t f_end = html.find("</form>", f_pos);
        if (f_end == std::string::npos) break;
        std::string candidate = html.substr(f_pos, f_end - f_pos);
        if (candidate.find("download2") != std::string::npos ||
            candidate.find("download1") != std::string::npos ||
            candidate.find("name=\"F1\"") != std::string::npos) {
            scope = candidate;
            break;
        }
        f_pos = f_end + 7;
    }
    size_t pos = 0;
    while ((pos = scope.find("<input", pos)) != std::string::npos) {
        size_t close = scope.find('>', pos);
        if (close == std::string::npos) break;
        std::string tag = scope.substr(pos, close - pos + 1);
        pos = close + 1;
        std::string name = extractAttr(tag, "name");
        if (!name.empty()) fields[name] = extractAttr(tag, "value");
    }
    return fields;
}

static std::string urlEncodeMap(CURL* curl, const std::map<std::string, std::string>& fields) {
    std::string out;
    for (const auto& [k, v] : fields) {
        if (!out.empty()) out += "&";
        char* ek = curl_easy_escape(curl, k.c_str(), (int)k.length());
        char* ev = curl_easy_escape(curl, v.c_str(), (int)v.length());
        if (ek && ev) out += std::string(ek) + "=" + std::string(ev);
        if (ek) curl_free(ek);
        if (ev) curl_free(ev);
    }
    return out;
}

ResolvedStream HostResolver::resolveDebrid(const std::string& url, const std::string& api_key) {
    ResolvedStream res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;
    std::string body;
    char* esc = curl_easy_escape(curl, url.c_str(), (int)url.length());
    std::string post_data = "link=" + std::string(esc ? esc : "");
    if (esc) curl_free(esc);

    struct curl_slist* headers = curl_slist_append(nullptr, ("Authorization: Bearer " + api_key).c_str());
    setupBaseCurl(curl, "https://api.real-debrid.com/rest/1.0/unrestrict/link", &body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    if (curl_easy_perform(curl) == CURLE_OK && !body.empty()) {
        try {
            auto j = nlohmann::json::parse(body);
            if (j.contains("download") && j["download"].is_string()) res.direct_url = j["download"].get<std::string>();
        } catch (...) {}
    }
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolveSendNow(const std::string& url) {
    ResolvedStream res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;

    std::string html;
    setupBaseCurl(curl, url, &html);
    if (curl_easy_perform(curl) != CURLE_OK || html.empty()) {
        res.error = "Send.now: Failed to fetch initial page";
        curl_easy_cleanup(curl);
        return res;
    }

    auto fields = parseFormInputs(html);
    if (fields.find("op") == fields.end()) {
        fields["op"] = "download2";
        size_t slash = url.rfind('/');
        if (slash != std::string::npos) fields["id"] = url.substr(slash + 1);
    }

    std::string post_data = urlEncodeMap(curl, fields), location, resp2;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
    curl_easy_setopt(curl, CURLOPT_REFERER, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerLocCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &location);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp2);
    if (curl_easy_perform(curl) == CURLE_OK && !location.empty()) {
        res.direct_url = location;
        res.referer = url;
    } else {
        res.error = "Send.now: No direct redirect returned";
    }
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolve1Fichier(const std::string& url, std::atomic<bool>* should_abort) {
    ResolvedStream res;
    std::string clean_url = url;
    size_t af_pos = clean_url.find("&af=");
    if (af_pos != std::string::npos) clean_url = clean_url.substr(0, af_pos);
    CURL* curl = curl_easy_init();
    if (!curl) return res;

    std::string api_key = SettingsManager::getInstance().getSettings().onefichier_key;
    if (!api_key.empty()) {
        std::string resp, json_body = "{\"url\":\"" + clean_url + "\",\"inline\":1}";
        struct curl_slist* hdrs = curl_slist_append(nullptr, ("Authorization: Bearer " + api_key).c_str());
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
        setupBaseCurl(curl, "https://api.1fichier.com/v1/download/get_token.cgi", &resp);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        if (curl_easy_perform(curl) == CURLE_OK) {
            try {
                auto j = nlohmann::json::parse(resp);
                if (j.value("status", "") == "OK" && j.contains("url")) res.direct_url = j["url"].get<std::string>();
            } catch (...) {}
        }
        curl_slist_free_all(hdrs);
        if (res.ok()) { curl_easy_cleanup(curl); return res; }
    }

    auto attemptPost = [&](std::string& out_html) -> std::string {
        out_html.clear();
        curl_easy_reset(curl);
        setupBaseCurl(curl, clean_url, &out_html);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "dl_no_ssl=on&dlinline=on");
        curl_easy_setopt(curl, CURLOPT_REFERER, clean_url.c_str());
        if (curl_easy_perform(curl) != CURLE_OK) return "";
        for (size_t pos = 0; (pos = out_html.find("href=\"", pos)) != std::string::npos; ) {
            pos += 6;
            size_t end = out_html.find('"', pos);
            if (end == std::string::npos) break;
            std::string href = out_html.substr(pos, end - pos);
            pos = end + 1;
            if (href.find(".1fichier.com/") != std::string::npos && href.find("img.1fichier.com") == std::string::npos) return href;
        }
        return "";
    };

    std::string html, dl = attemptPost(html);
    if (dl.empty()) {
        size_t ct_pos = html.find("var ct = ");
        int wait_sec = (ct_pos != std::string::npos) ? std::atoi(html.c_str() + ct_pos + 9) : 0;
        if (wait_sec > 0 && wait_sec <= 90 && sleepAbortable((wait_sec + 1) * 1000, should_abort)) dl = attemptPost(html);
    }
    if (!dl.empty()) { res.direct_url = dl; res.referer = clean_url; }
    else { res.error = "1fichier: Cooldown active or link unavailable"; }
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolveGofile(const std::string& url) {
    ResolvedStream res;
    size_t d_pos = url.find("/d/");
    if (d_pos == std::string::npos) { res.error = "Gofile: Invalid URL"; return res; }
    std::string content_id = url.substr(d_pos + 3);
    size_t slash = content_id.find('/');
    if (slash != std::string::npos) content_id = content_id.substr(0, slash);
    CURL* curl = curl_easy_init();
    if (!curl) return res;

    std::string token = SettingsManager::getInstance().getSettings().gofile_token;
    if (token.empty()) {
        std::string acc_resp;
        setupBaseCurl(curl, "https://api.gofile.io/accounts", &acc_resp);
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        if (curl_easy_perform(curl) == CURLE_OK && !acc_resp.empty()) {
            try { token = nlohmann::json::parse(acc_resp)["data"].value("token", ""); } catch (...) {}
        }
    }
    if (token.empty()) { res.error = "Gofile: Failed guest token"; curl_easy_cleanup(curl); return res; }

    long long bucket = (long long)(std::time(nullptr) / 14400);
    for (const std::string& salt : {"gf2026", "5d4f7g8sd45fsd", "4fd6sg89d7s6"}) {
        std::string wt = sha256Hex(std::string(BROWSER_UA) + "::en-US::" + token + "::" + std::to_string(bucket) + "::" + salt), body;
        struct curl_slist* hdrs = curl_slist_append(nullptr, ("Authorization: Bearer " + token).c_str());
        hdrs = curl_slist_append(hdrs, ("X-Website-Token: " + wt).c_str());
        hdrs = curl_slist_append(hdrs, "X-BL: en-US");
        curl_easy_reset(curl);
        setupBaseCurl(curl, "https://api.gofile.io/contents/" + content_id + "?wt=" + wt, &body);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        if (curl_easy_perform(curl) == CURLE_OK && !body.empty()) {
            try {
                auto j = nlohmann::json::parse(body);
                if (j.value("status", "") == "ok" && j["data"].contains("children")) {
                    for (auto& [k, child] : j["data"]["children"].items()) {
                        std::string link = child.value("link", "");
                        if (!link.empty()) { res.direct_url = link; res.cookie_header = "accountToken=" + token; res.referer = "https://gofile.io/"; break; }
                    }
                }
            } catch (...) {}
        }
        curl_slist_free_all(hdrs);
        if (res.ok()) break;
    }
    if (!res.ok()) res.error = "Gofile: Token rejected";
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolveMegaUp(const std::string& url, std::atomic<bool>* should_abort) {
    ResolvedStream res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;
    std::string html;
    setupBaseCurl(curl, url, &html);
    if (curl_easy_perform(curl) == CURLE_OK && !html.empty()) {
        size_t dl_pos = html.find("https://download.megaup.net/?url=");
        if (dl_pos != std::string::npos) {
            size_t end_q = html.find_first_of("'\"", dl_pos);
            std::string step2_url = (end_q != std::string::npos) ? html.substr(dl_pos, end_q - dl_pos) : "";
            if (!step2_url.empty() && sleepAbortable(5500, should_abort)) {
                std::string location, body2;
                curl_easy_setopt(curl, CURLOPT_URL, step2_url.c_str());
                curl_easy_setopt(curl, CURLOPT_REFERER, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerLocCb);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &location);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body2);
                if (curl_easy_perform(curl) == CURLE_OK && !location.empty()) {
                    res.direct_url = location;
                    res.referer = url;
                }
            }
        }
    }
    if (!res.ok()) res.error = "MegaUp: Cloudflare challenge active";
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolveDatanodes(const std::string& url, std::atomic<bool>* should_abort) {
    ResolvedStream res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;
    std::string html1;
    setupBaseCurl(curl, url, &html1);
    if (curl_easy_perform(curl) == CURLE_OK && !html1.empty()) {
        size_t form_pos = html1.find("id=\"downloadForm\"");
        auto fields1 = parseFormInputs(form_pos != std::string::npos ? html1.substr(form_pos) : html1);
        fields1["method_free"] = "Free Download >>";
        std::string post1 = urlEncodeMap(curl, fields1), html2;
        curl_easy_setopt(curl, CURLOPT_URL, "https://datanodes.to/download");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post1.c_str());
        curl_easy_setopt(curl, CURLOPT_REFERER, "https://datanodes.to/download");
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &html2);
        if (curl_easy_perform(curl) == CURLE_OK && !html2.empty()) {
            std::string rand_val = extractAttr(html2, "rand"), dltok = extractAttr(html2, "dl-token"), code_val = extractAttr(html2, "code");
            if (!rand_val.empty() && !code_val.empty() && sleepAbortable(10500, should_abort)) {
                std::map<std::string, std::string> fields2 = {
                    {"op", "download2"}, {"id", code_val}, {"rand", rand_val},
                    {"referer", "https://datanodes.to/download"}, {"method_free", "Free Download >>"},
                    {"method_premium", ""}, {"g_captch__a", "1"}, {"dl_token", dltok}
                };
                std::string post2 = urlEncodeMap(curl, fields2), json_resp;
                struct curl_slist* hdrs = curl_slist_append(nullptr, "X-Dn-Dl: 1");
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post2.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_resp);
                if (curl_easy_perform(curl) == CURLE_OK && !json_resp.empty() && json_resp.front() == '{') {
                    try {
                        auto j = nlohmann::json::parse(json_resp);
                        if (j.contains("url") && j["url"].is_string()) {
                            int out_len = 0;
                            std::string raw_u = j["url"].get<std::string>();
                            char* unesc = curl_easy_unescape(curl, raw_u.c_str(), (int)raw_u.length(), &out_len);
                            res.direct_url = unesc ? std::string(unesc, out_len) : raw_u;
                            if (unesc) curl_free(unesc);
                        }
                    } catch (...) {}
                }
                curl_slist_free_all(hdrs);
            }
        }
    }
    if (!res.ok()) res.error = "Datanodes: Turnstile challenge active";
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolveMediaFire(const std::string& url) {
    ResolvedStream res;
    CURL* curl = curl_easy_init();
    if (!curl) return res;
    std::string html;
    setupBaseCurl(curl, url, &html);
    if (curl_easy_perform(curl) == CURLE_OK) {
        size_t pos = html.find("https://download"), end = (pos != std::string::npos) ? html.find('"', pos) : std::string::npos;
        if (end != std::string::npos) res.direct_url = html.substr(pos, end - pos);
    }
    if (!res.ok()) res.error = "MediaFire: Direct link not found";
    curl_easy_cleanup(curl);
    return res;
}

ResolvedStream HostResolver::resolve(const std::string& url, const std::string& host_name, std::atomic<bool>* should_abort) {
    std::string debrid_key = SettingsManager::getInstance().getSettings().debrid_key;
    if (!debrid_key.empty()) {
        auto deb = resolveDebrid(url, debrid_key);
        if (deb.ok()) return deb;
    }
    if (url.find("send.now") != std::string::npos || url.find("send.cm") != std::string::npos || host_name == "Send.now") return resolveSendNow(url);
    if (url.find("1fichier.com") != std::string::npos || host_name == "1fichier") return resolve1Fichier(url, should_abort);
    if (url.find("gofile.io") != std::string::npos || host_name == "Gofile") return resolveGofile(url);
    if (url.find("megaup.net") != std::string::npos || host_name == "MegaUp") return resolveMegaUp(url, should_abort);
    if (url.find("datanodes.to") != std::string::npos || host_name == "Datanodes") return resolveDatanodes(url, should_abort);
    if (url.find("mediafire.com") != std::string::npos || host_name == "MediaFire") return resolveMediaFire(url);
    return {url, "", "", ""};
}
} // namespace switchfs::data
