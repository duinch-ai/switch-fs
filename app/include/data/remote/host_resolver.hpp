#pragma once

#include <string>
#include <atomic>

namespace switchfs::data {

struct ResolvedStream {
    std::string direct_url;
    std::string cookie_header;
    std::string referer;
    std::string error;

    bool ok() const { return !direct_url.empty(); }
};

class HostResolver {
public:
    /// Resolve a file hoster URL (Send.now, 1fichier, Gofile, MegaUp, Datanodes, MediaFire, or Debrid)
    /// into a direct binary download stream URL.
    static ResolvedStream resolve(const std::string& url,
                                  const std::string& host_name,
                                  std::atomic<bool>* should_abort = nullptr);

    /// Compute SHA-256 hex digest (used for Gofile X-Website-Token generation and unit tests)
    static std::string sha256Hex(const std::string& input);

private:
    static ResolvedStream resolveDebrid(const std::string& url, const std::string& api_key);
    static ResolvedStream resolveSendNow(const std::string& url);
    static ResolvedStream resolve1Fichier(const std::string& url, std::atomic<bool>* should_abort);
    static ResolvedStream resolveGofile(const std::string& url);
    static ResolvedStream resolveMegaUp(const std::string& url, std::atomic<bool>* should_abort);
    static ResolvedStream resolveDatanodes(const std::string& url, std::atomic<bool>* should_abort);
    static ResolvedStream resolveMediaFire(const std::string& url);
};

} // namespace switchfs::data
