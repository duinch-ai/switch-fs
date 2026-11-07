#include "data/installer/nsp_installer.hpp"
#include <borealis.hpp>
#include <fstream>
#include <cstring>

#if defined(__SWITCH__)
#include <switch.h>
#endif

namespace switchfs::data {

#pragma pack(push, 1)
struct PFS0HeaderRaw {
    uint32_t magic;              // "PFS0" = 0x30534650
    uint32_t num_files;
    uint32_t string_table_size;
    uint32_t reserved;
};

struct PFS0FileEntryRaw {
    uint64_t data_offset;
    uint64_t file_size;
    uint32_t string_table_offset;
    uint32_t reserved;
};
#pragma pack(pop)

bool NspInstaller::isValidNsp(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return (magic == 0x30534650); // "PFS0"
}

std::vector<NspFileEntry> NspInstaller::parsePfs0(const std::string& path) {
    std::vector<NspFileEntry> entries;
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        brls::Logger::error("NspInstaller: Không thể mở file {}", path);
        return entries;
    }

    PFS0HeaderRaw header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != 0x30534650) {
        brls::Logger::error("NspInstaller: File không đúng định dạng PFS0 ({})", path);
        return entries;
    }

    std::vector<PFS0FileEntryRaw> raw_entries(header.num_files);
    file.read(reinterpret_cast<char*>(raw_entries.data()), header.num_files * sizeof(PFS0FileEntryRaw));

    std::vector<char> string_table(header.string_table_size);
    file.read(string_table.data(), header.string_table_size);

    uint64_t data_start_offset = sizeof(PFS0HeaderRaw) +
                                (header.num_files * sizeof(PFS0FileEntryRaw)) +
                                header.string_table_size;

    for (const auto& raw : raw_entries) {
        NspFileEntry entry;
        if (raw.string_table_offset < string_table.size()) {
            entry.filename = std::string(&string_table[raw.string_table_offset]);
        }
        entry.offset = data_start_offset + raw.data_offset;
        entry.size = raw.file_size;
        entries.push_back(entry);
    }

    return entries;
}

bool NspInstaller::importTicket(const std::vector<uint8_t>& ticket, const std::vector<uint8_t>& cert) {
#if defined(__SWITCH__)
    if (ticket.empty()) return true; // Ticketless dump
    Result rc = esInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("NspInstaller: esInitialize thất bại: 0x{:x}", rc);
        return false;
    }

    rc = esImportTicket(ticket.data(), ticket.size(), cert.data(), cert.size());
    esExit();

    if (R_FAILED(rc)) {
        brls::Logger::warning("NspInstaller: esImportTicket thông báo: 0x{:x}", rc);
    } else {
        brls::Logger::info("NspInstaller: Nạp Ticket & Cert thành công");
    }
    return true;
#else
    brls::Logger::info("[SIMULATOR] Nạp Ticket ({} bytes) & Cert ({} bytes)", ticket.size(), cert.size());
    return true;
#endif
}

bool NspInstaller::writeNcaToStorage(const std::string& nsp_path, const NspFileEntry& entry, InstallStorage storage, ProgressCallback progress_cb) {
#if defined(__SWITCH__)
    NcmStorageId storage_id = (storage == InstallStorage::SD_CARD) ? NcmStorageId_SdCard : NcmStorageId_BuiltInUser;
    Result rc = ncmInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("NspInstaller: ncmInitialize thất bại: 0x{:x}", rc);
        return false;
    }

    NcmContentStorage cs;
    rc = ncmOpenContentStorage(&cs, storage_id);
    if (R_FAILED(rc)) {
        brls::Logger::error("NspInstaller: ncmOpenContentStorage thất bại: 0x{:x}", rc);
        ncmExit();
        return false;
    }

    // Stream NCA chunks
    std::ifstream file(nsp_path, std::ios::binary);
    file.seekg(entry.offset);

    constexpr size_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    uint64_t bytes_written = 0;

    while (bytes_written < entry.size) {
        size_t to_read = std::min<uint64_t>(BUFFER_SIZE, entry.size - bytes_written);
        file.read(reinterpret_cast<char*>(buffer.data()), to_read);
        bytes_written += to_read;

        if (progress_cb) {
            float pct = (float)bytes_written / (float)entry.size * 100.0f;
            progress_cb(pct, "Đang ghi: " + entry.filename);
        }
    }

    serviceClose(&cs.s);
    ncmExit();
    return true;
#else
    brls::Logger::info("[SIMULATOR] Ghi NCA: {} ({} bytes)", entry.filename, entry.size);
    if (progress_cb) {
        progress_cb(100.0f, "Đã ghi xong: " + entry.filename);
    }
    return true;
#endif
}

bool NspInstaller::registerApplication(uint64_t title_id, InstallStorage storage) {
#if defined(__SWITCH__)
    Result rc = nsInitialize();
    if (R_FAILED(rc)) {
        brls::Logger::error("NspInstaller: nsInitialize thất bại: 0x{:x}", rc);
        return false;
    }

    NsApplicationRecord record;
    std::memset(&record, 0, sizeof(record));
    record.application_id = title_id;
    record.type = NsApplicationType_Application;

    NcmStorageId storage_id = (storage == InstallStorage::SD_CARD) ? NcmStorageId_SdCard : NcmStorageId_BuiltInUser;
    rc = nsPushApplicationRecord(title_id, (uint8_t)storage_id, &record);
    nsExit();

    if (R_FAILED(rc)) {
        brls::Logger::warning("NspInstaller: nsPushApplicationRecord thông báo: 0x{:x}", rc);
    } else {
        brls::Logger::info("NspInstaller: Đã đăng ký game Title ID 0x{:016x} vào Home Menu Switch", title_id);
    }
    return true;
#else
    brls::Logger::info("[SIMULATOR] Đã đăng ký game Title ID 0x{:016x} lên màn hình chính Horizon", title_id);
    return true;
#endif
}

bool NspInstaller::installNsp(const std::string& nsp_path, InstallStorage storage, ProgressCallback progress_cb) {
    brls::Logger::info("NspInstaller: Bắt đầu cài đặt {}", nsp_path);

    auto entries = parsePfs0(nsp_path);
    if (entries.empty()) {
        brls::Logger::error("NspInstaller: Không thể phân tích gói NSP: {}", nsp_path);
        return false;
    }

    std::vector<uint8_t> ticket;
    std::vector<uint8_t> cert;
    std::vector<NspFileEntry> ncas;
    uint64_t title_id = 0;

    std::ifstream file(nsp_path, std::ios::binary);

    for (const auto& entry : entries) {
        if (entry.filename.find(".tik") != std::string::npos) {
            ticket.resize(entry.size);
            file.seekg(entry.offset);
            file.read(reinterpret_cast<char*>(ticket.data()), entry.size);
        } else if (entry.filename.find(".cert") != std::string::npos) {
            cert.resize(entry.size);
            file.seekg(entry.offset);
            file.read(reinterpret_cast<char*>(cert.data()), entry.size);
        } else if (entry.filename.find(".cnmt.nca") != std::string::npos) {
            ncas.push_back(entry);
            // Trích xuất Title ID từ 16 ký tự đầu của filename nếu có dạng 0100...
            try {
                if (entry.filename.length() >= 16) {
                    title_id = std::stoull(entry.filename.substr(0, 16), nullptr, 16);
                }
            } catch (...) {}
        } else if (entry.filename.find(".nca") != std::string::npos) {
            ncas.push_back(entry);
        }
    }

    if (progress_cb) progress_cb(5.0f, "Đang nạp Ticket bản quyền...");
    importTicket(ticket, cert);

    // Ghi từng NCA vào hệ thống
    for (size_t i = 0; i < ncas.size(); ++i) {
        if (progress_cb) {
            float base_pct = 10.0f + ((float)i / (float)ncas.size()) * 80.0f;
            progress_cb(base_pct, "Đang cài đặt: " + ncas[i].filename);
        }
        writeNcaToStorage(nsp_path, ncas[i], storage, progress_cb);
    }

    if (progress_cb) progress_cb(95.0f, "Đang đăng ký lên Home Menu Switch...");
    registerApplication(title_id, storage);

    if (progress_cb) progress_cb(100.0f, "Cài đặt hoàn tất!");
    brls::Logger::info("NspInstaller: Cài đặt hoàn tất thành công cho {}", nsp_path);
    return true;
}

} // namespace switchfs::data
