#include "download/download_manager.hpp"
#include "data/extractor/archive_extractor.hpp"
#include <curl/curl.h>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>

namespace switchfs::download {

DownloadManager& DownloadManager::getInstance() {
    static DownloadManager instance;
    return instance;
}

std::string DownloadManager::getDownloadDir() {
#if defined(__SWITCH__)
    return "sdmc:/switch/SwitchFS/downloads/";
#else
    return "./downloads/";
#endif
}

DownloadManager::DownloadManager() {
    // Create downloads directory if not exists
    std::string dir = getDownloadDir();
#if defined(_WIN32)
    mkdir(dir.c_str());
#else
    mkdir(dir.c_str(), 0777);
#endif

    this->worker_thread = std::thread(&DownloadManager::workerLoop, this);
}

DownloadManager::~DownloadManager() {
    this->is_running = false;
    this->should_abort_current = true;
    if (this->worker_thread.joinable()) {
        this->worker_thread.join();
    }
}

void DownloadManager::enqueue(const api::GameItem& game, const std::string& host_url, const std::string& host_name) {
    std::lock_guard<std::mutex> lock(this->tasks_mutex);

    DownloadTask task;
    task.id = game.id != 0 ? game.id : std::chrono::system_clock::now().time_since_epoch().count();
    task.title = game.title;
    task.cover_url = game.cover_url;
    task.host_name = host_name;
    task.original_url = host_url;
    task.direct_url = host_url; // Direct or resolved URL
    task.state = DownloadState::PENDING;

    // Sanitize title for filename
    std::string safe_title = game.title;
    for (char& c : safe_title) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    task.dest_path = getDownloadDir() + safe_title + ".rar";

    this->tasks.push_back(task);
    brls::Logger::info("DownloadManager: Enqueued task '{}' from {}", task.title, host_name);
}

std::vector<DownloadTask> DownloadManager::getTasks() {
    std::lock_guard<std::mutex> lock(this->tasks_mutex);
    return this->tasks;
}

void DownloadManager::pauseTask(uint64_t id) {
    std::lock_guard<std::mutex> lock(this->tasks_mutex);
    for (auto& t : this->tasks) {
        if (t.id == id) {
            if (t.state == DownloadState::DOWNLOADING) {
                if (this->current_active_id == id) {
                    this->should_abort_current = true;
                }
            }
            t.state = DownloadState::PAUSED;
            break;
        }
    }
}

void DownloadManager::resumeTask(uint64_t id) {
    std::lock_guard<std::mutex> lock(this->tasks_mutex);
    for (auto& t : this->tasks) {
        if (t.id == id) {
            t.state = DownloadState::PENDING;
            break;
        }
    }
}

void DownloadManager::cancelTask(uint64_t id) {
    std::lock_guard<std::mutex> lock(this->tasks_mutex);
    for (auto it = this->tasks.begin(); it != this->tasks.end(); ++it) {
        if (it->id == id) {
            if (this->current_active_id == id) {
                this->should_abort_current = true;
            }
            // Remove incomplete file
            remove(it->dest_path.c_str());
            this->tasks.erase(it);
            break;
        }
    }
}

brls::Event<DownloadTask>& DownloadManager::getProgressEvent() {
    return this->progress_event;
}

struct CurlProgressContext {
    DownloadTask* task;
    std::atomic<bool>* should_abort;
    curl_off_t existing_bytes;
    std::chrono::steady_clock::time_point last_notify_time;
    std::chrono::steady_clock::time_point last_speed_time;
    curl_off_t last_dl_bytes;
    brls::Event<DownloadTask>* event;
};

static size_t writeDataCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    return fwrite(ptr, size, nmemb, (FILE*)stream);
}

static int xferInfoCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    auto* ctx = static_cast<CurlProgressContext*>(clientp);
    if (!ctx) return 0;
    if (ctx->should_abort && ctx->should_abort->load()) {
        return 1; // Abort transfer
    }

    auto now = std::chrono::steady_clock::now();
    ctx->task->downloaded_bytes = ctx->existing_bytes + dlnow;
    ctx->task->total_bytes = ctx->existing_bytes + dltotal;

    if (ctx->task->total_bytes > 0) {
        ctx->task->progress = (double)ctx->task->downloaded_bytes / (double)ctx->task->total_bytes * 100.0;
    }

    // Update speed every 500ms
    auto speed_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->last_speed_time).count();
    if (speed_elapsed >= 500) {
        curl_off_t bytes_diff = dlnow - ctx->last_dl_bytes;
        ctx->task->speed_bps = (double)bytes_diff / ((double)speed_elapsed / 1000.0);
        ctx->last_dl_bytes = dlnow;
        ctx->last_speed_time = now;
    }

    // Fire event every 250ms
    auto notify_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - ctx->last_notify_time).count();
    if (notify_elapsed >= 250) {
        ctx->last_notify_time = now;
        if (ctx->event) {
            ctx->event->fire(*ctx->task);
        }
    }

    return 0;
}

void DownloadManager::processTask(DownloadTask& task) {
    this->current_active_id = task.id;
    this->should_abort_current = false;
    task.state = DownloadState::DOWNLOADING;

    FILE* fp = fopen(task.dest_path.c_str(), "ab+");
    if (!fp) {
        task.state = DownloadState::FAILED;
        task.error_message = "Không thể mở file đích: " + task.dest_path;
        brls::Logger::error("DownloadManager error: {}", task.error_message);
        return;
    }

    fseek(fp, 0, SEEK_END);
    curl_off_t existing_bytes = ftell(fp);

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        task.state = DownloadState::FAILED;
        task.error_message = "Khởi tạo curl thất bại";
        return;
    }

    CurlProgressContext ctx;
    ctx.task = &task;
    ctx.should_abort = &this->should_abort_current;
    ctx.existing_bytes = existing_bytes;
    ctx.last_notify_time = std::chrono::steady_clock::now();
    ctx.last_speed_time = std::chrono::steady_clock::now();
    ctx.last_dl_bytes = 0;
    ctx.event = &this->progress_event;

    curl_easy_setopt(curl, CURLOPT_URL, task.direct_url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; SwitchFS/0.1.0)");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    if (existing_bytes > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, existing_bytes);
    }

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    this->current_active_id = 0;

    if (this->should_abort_current.load()) {
        if (task.state != DownloadState::PAUSED) {
            task.state = DownloadState::PAUSED;
        }
        brls::Logger::info("DownloadManager: Task paused/aborted: {}", task.title);
    } else if (res == CURLE_OK) {
        task.state = DownloadState::COMPLETED;
        task.progress = 100.0;
        task.speed_bps = 0.0;
        brls::Logger::info("DownloadManager: Task completed successfully: {}", task.title);

        // Tự động giải nén file archive (.rar, .zip) nếu có
        if (data::ArchiveExtractor::isArchive(task.dest_path)) {
            brls::Logger::info("DownloadManager: Đang tự động giải nén: {}", task.dest_path);
            std::string extract_dir = getDownloadDir() + "extracted/" + task.title + "/";
            bool ok = data::ArchiveExtractor::extract(task.dest_path, extract_dir);
            if (ok) {
                brls::Logger::info("DownloadManager: Giải nén thành công vào: {}", extract_dir);
            } else {
                brls::Logger::warning("DownloadManager: Không thể giải nén archive: {}", task.dest_path);
            }
        }
    } else {
        task.state = DownloadState::FAILED;
        task.error_message = curl_easy_strerror(res);
        brls::Logger::error("DownloadManager: Task failed: {} - {}", task.title, task.error_message);
    }

    this->progress_event.fire(task);
}

void DownloadManager::workerLoop() {
    while (this->is_running) {
        DownloadTask* next_task = nullptr;

        {
            std::lock_guard<std::mutex> lock(this->tasks_mutex);
            for (auto& t : this->tasks) {
                if (t.state == DownloadState::PENDING) {
                    next_task = &t;
                    break;
                }
            }
        }

        if (next_task) {
            processTask(*next_task);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }
}

} // namespace switchfs::download
