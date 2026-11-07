#pragma once

#include "api/nswgf.hpp"
#include <borealis.hpp>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace switchfs::download {

enum class DownloadState {
    PENDING,
    DOWNLOADING,
    PAUSED,
    COMPLETED,
    FAILED
};

struct DownloadTask {
    uint64_t id = 0;
    std::string title;
    std::string cover_url;
    std::string host_name;
    std::string original_url;
    std::string direct_url;
    std::string dest_path;
    DownloadState state = DownloadState::PENDING;
    double progress = 0.0; // 0.0 - 100.0 %
    double speed_bps = 0.0; // bytes/sec
    int64_t downloaded_bytes = 0;
    int64_t total_bytes = 0;
    std::string error_message;
    bool auto_install = true;
};

class DownloadManager {
public:
    static DownloadManager& getInstance();

    // Enqueue a new download from GameItem
    void enqueue(const api::GameItem& game, const std::string& host_url, const std::string& host_name);

    // Get snapshot of all tasks
    std::vector<DownloadTask> getTasks();

    // Control tasks
    void pauseTask(uint64_t id);
    void resumeTask(uint64_t id);
    void cancelTask(uint64_t id);

    // Event on state/progress change
    brls::Event<DownloadTask>& getProgressEvent();

    // Get downloads directory
    static std::string getDownloadDir();

private:
    DownloadManager();
    ~DownloadManager();

    DownloadManager(const DownloadManager&) = delete;
    DownloadManager& operator=(const DownloadManager&) = delete;

    void workerLoop();
    void processTask(DownloadTask& task);

    std::mutex tasks_mutex;
    std::vector<DownloadTask> tasks;
    std::thread worker_thread;
    std::atomic<bool> is_running{true};
    std::atomic<bool> should_abort_current{false};
    uint64_t current_active_id{0};

    brls::Event<DownloadTask> progress_event;
};

} // namespace switchfs::download
