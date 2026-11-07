#include "view/queue_tab.hpp"
#include <iomanip>
#include <sstream>

namespace switchfs::view {

static std::string formatBytes(int64_t bytes) {
    if (bytes <= 0) return "0 B";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double d_bytes = (double)bytes;
    while (d_bytes >= 1024.0 && unit < 4) {
        d_bytes /= 1024.0;
        unit++;
    }
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << d_bytes << " " << units[unit];
    return ss.str();
}

QueueTab::QueueTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20, 20, 20, 20);

    // Status label
    this->status_label = new brls::Label();
    this->status_label->setText("Hàng đợi tải xuống");
    this->status_label->setFontSize(22);
    this->status_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    this->status_label->setMarginBottom(15);
    this->addView(this->status_label);

    // Scrolling Frame
    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->tasks_container = new brls::Box(brls::Axis::COLUMN);
    this->tasks_container->setGrow(1.0f);
    this->scrolling_frame->setContentView(this->tasks_container);
    this->addView(this->scrolling_frame);

    // Subscribe to progress events
    this->progress_sub = download::DownloadManager::getInstance().getProgressEvent().subscribe([this](const download::DownloadTask&) {
        brls::sync([this]() {
            this->refreshList();
        });
    });
    this->has_sub = true;

    this->refreshList();
}

QueueTab::~QueueTab() {
    if (this->has_sub) {
        download::DownloadManager::getInstance().getProgressEvent().unsubscribe(this->progress_sub);
    }
}

brls::View* QueueTab::create() {
    return new QueueTab();
}

void QueueTab::refreshList() {
    auto tasks = download::DownloadManager::getInstance().getTasks();
    this->tasks_container->clearViews();

    if (tasks.empty()) {
        auto* empty_label = new brls::Label();
        empty_label->setText("Chưa có tác vụ tải xuống nào.\nHãy vào tab Trang chủ hoặc Tìm kiếm để chọn game.");
        empty_label->setFontSize(16);
        empty_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        empty_label->setMarginTop(30);
        this->tasks_container->addView(empty_label);
        return;
    }

    for (const auto& task : tasks) {
        auto* card = new brls::Box(brls::Axis::COLUMN);
        card->setPadding(15, 20, 15, 20);
        card->setMarginBottom(15);
        card->setCornerRadius(8);
        card->setShadowType(brls::ShadowType::GENERIC);

        // Header: Title & Host
        auto* header_row = new brls::Box(brls::Axis::ROW);
        header_row->setMarginBottom(8);

        auto* title = new brls::Label();
        title->setText(task.title);
        title->setFontSize(18);
        title->setTextColor(brls::Application::getTheme()["brls/text"]);
        title->setGrow(1.0f);
        header_row->addView(title);

        auto* host_badge = new brls::Label();
        host_badge->setText("[" + task.host_name + "]");
        host_badge->setFontSize(14);
        host_badge->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        header_row->addView(host_badge);
        card->addView(header_row);

        // Progress text & speed
        std::string status_str;
        switch (task.state) {
            case download::DownloadState::PENDING: status_str = "Đang chờ..."; break;
            case download::DownloadState::DOWNLOADING: status_str = "Đang tải..."; break;
            case download::DownloadState::PAUSED: status_str = "Tạm dừng"; break;
            case download::DownloadState::COMPLETED: status_str = "Đã hoàn thành!"; break;
            case download::DownloadState::FAILED: status_str = "Lỗi: " + task.error_message; break;
        }

        std::string progress_info = status_str;
        if (task.state == download::DownloadState::DOWNLOADING) {
            std::ostringstream ss;
            ss << " • " << formatBytes((int64_t)task.speed_bps) << "/s";
            ss << " • " << formatBytes(task.downloaded_bytes) << " / " << formatBytes(task.total_bytes);
            ss << " (" << std::fixed << std::setprecision(1) << task.progress << "%)";
            progress_info += ss.str();
        }

        auto* info_label = new brls::Label();
        info_label->setText(progress_info);
        info_label->setFontSize(14);
        info_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        info_label->setMarginBottom(10);
        card->addView(info_label);

        // Progress bar container (Outer bar)
        auto* bar_outer = new brls::Box(brls::Axis::ROW);
        bar_outer->setHeight(8);
        bar_outer->setCornerRadius(4);
        bar_outer->setMarginBottom(12);

        // Simple progress bar representation via width percentage
        float pct = std::max(0.0, std::min(100.0, task.progress)) / 100.0f;
        auto* bar_fill = new brls::Box(brls::Axis::ROW);
        bar_fill->setHeight(8);
        bar_fill->setGrow(pct);
        bar_outer->addView(bar_fill);

        card->addView(bar_outer);

        // Actions Row
        auto* actions_row = new brls::Box(brls::Axis::ROW);

        if (task.state == download::DownloadState::DOWNLOADING) {
            auto* pause_btn = new brls::Button();
            pause_btn->setText("Tạm dừng");
            pause_btn->setHeight(36);
            pause_btn->setMarginRight(10);
            pause_btn->registerClickAction([task](brls::View* view) {
                download::DownloadManager::getInstance().pauseTask(task.id);
                return true;
            });
            actions_row->addView(pause_btn);
        } else if (task.state == download::DownloadState::PAUSED) {
            auto* resume_btn = new brls::Button();
            resume_btn->setText("Tiếp tục tải");
            resume_btn->setHeight(36);
            resume_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
            resume_btn->setMarginRight(10);
            resume_btn->registerClickAction([task](brls::View* view) {
                download::DownloadManager::getInstance().resumeTask(task.id);
                return true;
            });
            actions_row->addView(resume_btn);
        }

        auto* cancel_btn = new brls::Button();
        cancel_btn->setText("Hủy");
        cancel_btn->setHeight(36);
        cancel_btn->registerClickAction([task](brls::View* view) {
            download::DownloadManager::getInstance().cancelTask(task.id);
            return true;
        });
        actions_row->addView(cancel_btn);

        card->addView(actions_row);
        this->tasks_container->addView(card);
    }
}

} // namespace switchfs::view
