#include "data/extractor/archive_extractor.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <borealis.hpp>
#include <sys/stat.h>
#include <algorithm>

namespace switchfs::data {

bool ArchiveExtractor::isArchive(const std::string& path) {
    std::string ext;
    size_t dot_pos = path.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = path.substr(dot_pos);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    return (ext == ".rar" || ext == ".zip" || ext == ".7z" || ext == ".tar" || ext == ".gz");
}

std::vector<std::string> ArchiveExtractor::listEntries(const std::string& archive_path) {
    std::vector<std::string> entries;
    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    int r = archive_read_open_filename(a, archive_path.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        brls::Logger::error("ArchiveExtractor: Failed to open archive {}: {}", archive_path, archive_error_string(a));
        archive_read_free(a);
        return entries;
    }

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        entries.push_back(archive_entry_pathname(entry));
        archive_read_data_skip(a);
    }

    archive_read_close(a);
    archive_read_free(a);
    return entries;
}

static int copyData(struct archive* ar, struct archive* aw) {
    const void* buff;
    size_t size;
    la_int64_t offset;

    for (;;) {
        int r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return ARCHIVE_OK;
        if (r < ARCHIVE_OK) return r;
        r = archive_write_data_block(aw, buff, size, offset);
        if (r < ARCHIVE_OK) {
            brls::Logger::error("ArchiveExtractor: write error: {}", archive_error_string(aw));
            return r;
        }
    }
}

bool ArchiveExtractor::extract(const std::string& archive_path,
                               const std::string& dest_dir,
                               ProgressCallback progress_cb) {
    struct archive* a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_all(a);

    struct archive* ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_SECURE_NODOTDOT);
    archive_write_disk_set_standard_lookup(ext);

    int r = archive_read_open_filename(a, archive_path.c_str(), 10240);
    if (r != ARCHIVE_OK) {
        brls::Logger::error("ArchiveExtractor: Failed to open {}: {}", archive_path, archive_error_string(a));
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    // Ensure dest_dir exists
    std::string safe_dest = dest_dir;
    if (!safe_dest.empty() && safe_dest.back() != '/') {
        safe_dest += '/';
    }
#if defined(_WIN32)
    mkdir(safe_dest.c_str());
#else
    mkdir(safe_dest.c_str(), 0777);
#endif

    struct archive_entry* entry;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char* current_file = archive_entry_pathname(entry);
        std::string full_output_path = safe_dest + current_file;
        archive_entry_set_pathname(entry, full_output_path.c_str());

        if (progress_cb) {
            progress_cb(0.0f, current_file);
        }

        r = archive_write_header(ext, entry);
        if (r < ARCHIVE_OK) {
            brls::Logger::warning("ArchiveExtractor: Header warning for {}: {}", current_file, archive_error_string(ext));
        } else if (archive_entry_size(entry) > 0) {
            r = copyData(a, ext);
            if (r < ARCHIVE_OK) {
                brls::Logger::error("ArchiveExtractor: Data extraction error for {}: {}", current_file, archive_error_string(ext));
            }
        }

        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_OK) {
            brls::Logger::warning("ArchiveExtractor: Finish entry warning for {}: {}", current_file, archive_error_string(ext));
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return (r == ARCHIVE_EOF || r == ARCHIVE_OK);
}

} // namespace switchfs::data
