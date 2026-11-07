#include "download/download_manager.hpp"
#include "api/romm.hpp"
#include "data/library_manager.hpp"
#include "data/extractor/archive_extractor.hpp"
#include "data/installer/nsp_installer.hpp"
#include "data/remote/host_resolver.hpp"
#include <curl/curl.h>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>
#include <filesystem>

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
    task.direct_url = host_url; // Will be resolved in worker thread
    task.state = DownloadState::PENDING;
    task.fallback_links = game.download_links;

    // Sanitize title for filename
    std::string safe_title = game.title;
    for (char& c : safe_title) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    std::string ext = ".rar";
    if (host_name == "RomM") {
        if (host_url.find(".nsz") != std::string::npos || game.format == "NSZ") ext = ".nsz";
        else if (host_url.find(".xci") != std::string::npos || game.format == "XCI") ext = ".xci";
        else if (host_url.find(".zip") != std::string::npos) ext = ".zip";
        else ext = ".nsp";
    }
    task.dest_path = getDownloadDir() + safe_title + ext;

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
    this->progress_event.fire(task);

    // 1. Resolve file hoster URL into direct binary stream (with automatic mirror fallback)
    data::ResolvedStream stream;
    if (task.host_name == "RomM") {
        stream.direct_url = task.original_url;
    } else {
        stream = data::HostResolver::resolve(task.original_url, task.host_name, &this->should_abort_current);
    }
    if (!stream.ok() && !this->should_abort_current.load()) {
        brls::Logger::warning("DownloadManager: Primary host {} failed ({}), trying mirror fallbacks...",
                              task.host_name, stream.error);
        // Priority order for mirrors: Send.now -> 1fichier -> Gofile -> others
        static const std::vector<std::string> priority_hosts = {"Send.now", "1fichier", "Gofile", "MediaFire"};
        for (const auto& ph : priority_hosts) {
            if (this->should_abort_current.load()) break;
            for (const auto& link : task.fallback_links) {
                if (link.host_name == ph && link.url != task.original_url) {
                    auto fb = data::HostResolver::resolve(link.url, link.host_name, &this->should_abort_current);
                    if (fb.ok()) {
                        stream = fb;
                        task.host_name = link.host_name;
                        brls::Logger::info("DownloadManager: Switched to working mirror [{}]", link.host_name);
                        break;
                    }
                }
            }
            if (stream.ok()) break;
        }
    }

    if (!stream.ok()) {
        this->current_active_id = 0;
        task.state = DownloadState::FAILED;
        task.error_message = stream.error.empty() ? "Could not resolve direct download link" : stream.error;
        this->progress_event.fire(task);
        return;
    }

    task.direct_url = stream.direct_url;

    FILE* fp = fopen(task.dest_path.c_str(), "ab+");
    if (!fp) {
        this->current_active_id = 0;
        task.state = DownloadState::FAILED;
        task.error_message = "Cannot open destination file: " + task.dest_path;
        brls::Logger::error("DownloadManager error: {}", task.error_message);
        return;
    }

    fseek(fp, 0, SEEK_END);
    curl_off_t existing_bytes = ftell(fp);

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        this->current_active_id = 0;
        task.state = DownloadState::FAILED;
        task.error_message = "Failed to initialize curl";
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
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeDataCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, xferInfoCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &ctx);

    struct curl_slist* headers = nullptr;
    if (task.host_name == "RomM") {
        std::string auth = api::RommApi::getAuthHeader();
        if (!auth.empty()) {
            headers = curl_slist_append(headers, auth.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        }
    }
    if (!stream.cookie_header.empty()) {
        curl_easy_setopt(curl, CURLOPT_COOKIE, stream.cookie_header.c_str());
    }
    if (!stream.referer.empty()) {
        curl_easy_setopt(curl, CURLOPT_REFERER, stream.referer.c_str());
    }

    if (existing_bytes > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, existing_bytes);
    }

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    fclose(fp);
    if (headers) curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    this->current_active_id = 0;

    if (this->should_abort_current.load()) {
        if (task.state != DownloadState::PAUSED) {
            task.state = DownloadState::PAUSED;
        }
        brls::Logger::info("DownloadManager: Task paused/aborted: {}", task.title);
    } else if (res == CURLE_OK && (http_code == 200 || http_code == 206)) {
        task.state = DownloadState::COMPLETED;
        task.progress = 100.0;
        task.speed_bps = 0.0;
        brls::Logger::info("DownloadManager: Task completed successfully: {}", task.title);

        // Auto-extract archive (.rar, .zip) if applicable
        std::string nsp_target = task.dest_path;
        if (data::ArchiveExtractor::isArchive(task.dest_path)) {
            brls::Logger::info("DownloadManager: Auto-extracting: {}", task.dest_path);
            std::string extract_dir = getDownloadDir() + "extracted/" + task.title + "/";
            bool ok = data::ArchiveExtractor::extract(task.dest_path, extract_dir);
            if (ok) {
                brls::Logger::info("DownloadManager: Extracted successfully to: {}", extract_dir);
                for (const auto& entry : std::filesystem::directory_iterator(extract_dir)) {
                    std::string path_str = entry.path().string();
                    if (path_str.rfind(".nsp") != std::string::npos || path_str.rfind(".nsz") != std::string::npos) {
                        nsp_target = path_str;
                        break;
                    }
                }
            } else {
                brls::Logger::warning("DownloadManager: Failed to extract archive: {}", task.dest_path);
            }
        }

        // Auto-install game to Switch if auto_install is enabled
        if (task.auto_install && !nsp_target.empty()) {
            brls::Logger::info("DownloadManager: Starting auto-install: {}", nsp_target);
            bool installed = data::NspInstaller::installNsp(nsp_target, data::InstallStorage::SD_CARD);
            if (installed) {
                api::GameItem inst_game;
                inst_game.id = task.id;
                inst_game.title = task.title;
                inst_game.cover_url = task.cover_url;
                inst_game.format = "INSTALLED";
                data::LibraryManager::getInstance().recordInstalled(inst_game);
                brls::sync([task]() {
                    brls::Application::notify("Successfully installed [" + task.title + "] to Switch Home Menu!");
                });
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
