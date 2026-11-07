#pragma once

#include <string>
#include <functional>
#include <vector>

namespace switchfs::data {

class ArchiveExtractor {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& current_file)>;

    /// Extract an archive (.rar, .zip, .7z, .tar) to target directory
    static bool extract(const std::string& archive_path,
                        const std::string& dest_dir,
                        ProgressCallback progress_cb = nullptr);

    /// Check if path is a supported archive
    static bool isArchive(const std::string& path);

    /// List files inside archive without extracting
    static std::vector<std::string> listEntries(const std::string& archive_path);
};

} // namespace switchfs::data
