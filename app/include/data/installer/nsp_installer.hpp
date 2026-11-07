#pragma once

#include <string>
#include <functional>
#include <vector>
#include <cstdint>

namespace switchfs::data {

enum class InstallStorage {
    SD_CARD,
    INTERNAL_NAND
};

struct NspFileEntry {
    std::string filename;
    uint64_t offset = 0;
    uint64_t size = 0;
};

class NspInstaller {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;

    /// Cài đặt file NSP trực tiếp vào hệ thống Horizon OS (SD Card hoặc NAND)
    static bool installNsp(const std::string& nsp_path,
                           InstallStorage storage = InstallStorage::SD_CARD,
                           ProgressCallback progress_cb = nullptr);

    /// Kiểm tra file có phải định dạng NSP hợp lệ (PFS0)
    static bool isValidNsp(const std::string& path);

    /// Phân tích danh sách file con trong gói NSP (PFS0)
    static std::vector<NspFileEntry> parsePfs0(const std::string& path);

private:
    static bool importTicket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& cert);
    static bool writeNcaToStorage(const std::string& nsp_path, const NspFileEntry& entry, InstallStorage storage, ProgressCallback progress_cb);
    static bool registerApplication(uint64_t title_id, InstallStorage storage);
};

} // namespace switchfs::data
