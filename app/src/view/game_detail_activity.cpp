#include "view/game_detail_activity.hpp"
#include "download/download_manager.hpp"
#include "utils/http.hpp"

namespace switchfs::view {

GameDetailActivity::GameDetailActivity(const api::GameItem& item) : item(item) {
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    auto* root_box = new brls::Box(brls::Axis::COLUMN);
    root_box->setGrow(1.0f);
    root_box->setPadding(30, 40, 30, 40);

    // Top horizontal section: Cover (Left) + Metadata (Right)
    auto* top_section = new brls::Box(brls::Axis::ROW);
    top_section->setMarginBottom(30);

    // 1. Cover Image
    auto* cover = new brls::Image();
    cover->setWidth(240);
    cover->setHeight(320);
    cover->setCornerRadius(10);
    cover->setScalingType(brls::ImageScalingType::FIT);
    cover->setImageFromRes("img/borealis_256.png");

    if (!item.cover_url.empty()) {
        std::string url = item.cover_url;
        brls::async([cover, url]() {
            std::string data = utils::HttpClient::get(url);
            if (!data.empty()) {
                brls::sync([cover, data]() {
                    cover->setImageFromMem((const unsigned char*)data.data(), data.size());
                });
            }
        });
    }
    top_section->addView(cover);

    // 2. Metadata column
    auto* meta_col = new brls::Box(brls::Axis::COLUMN);
    meta_col->setMarginLeft(30);
    meta_col->setGrow(1.0f);

    // Title
    auto* title_label = new brls::Label();
    title_label->setText(item.title);
    title_label->setFontSize(24);
    title_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    title_label->setMarginBottom(15);
    meta_col->addView(title_label);

    // Badge Row: Format, Size, Date
    auto* badges_row = new brls::Box(brls::Axis::ROW);
    badges_row->setMarginBottom(12);

    auto* format_label = new brls::Label();
    format_label->setText("Định dạng: " + item.format);
    format_label->setFontSize(16);
    format_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    format_label->setMarginRight(20);
    badges_row->addView(format_label);

    auto* size_label = new brls::Label();
    size_label->setText("Dung lượng: " + item.size);
    size_label->setFontSize(16);
    size_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    size_label->setMarginRight(20);
    badges_row->addView(size_label);

    auto* date_label = new brls::Label();
    date_label->setText("Cập nhật: " + item.date);
    date_label->setFontSize(16);
    date_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    badges_row->addView(date_label);

    meta_col->addView(badges_row);

    // Language
    if (!item.language.empty()) {
        auto* lang_label = new brls::Label();
        lang_label->setText("Ngôn ngữ: " + item.language);
        lang_label->setFontSize(15);
        lang_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        lang_label->setMarginBottom(10);
        meta_col->addView(lang_label);
    }

    // Hint note
    auto* hint_label = new brls::Label();
    hint_label->setText("File sau khi tải sẽ tự động kiểm tra định dạng và giải nén cài đặt.");
    hint_label->setFontSize(14);
    hint_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    meta_col->addView(hint_label);

    top_section->addView(meta_col);
    root_box->addView(top_section);

    // Section 2: Download Options
    auto* section_label = new brls::Label();
    section_label->setText("Chọn nguồn tải xuống (Download Hosts):");
    section_label->setFontSize(20);
    section_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    section_label->setMarginBottom(15);
    root_box->addView(section_label);

    if (item.download_links.empty()) {
        auto* empty_label = new brls::Label();
        empty_label->setText("Không tìm thấy link tải tự động cho game này.");
        empty_label->setFontSize(16);
        empty_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        root_box->addView(empty_label);
    } else {
        auto* buttons_box = new brls::Box(brls::Axis::COLUMN);
        for (const auto& dl : item.download_links) {
            auto* btn = new brls::Button();
            std::string btn_text = "[" + dl.host_name + "] " + dl.url.substr(0, std::min<size_t>(dl.url.length(), 60)) + "...";
            btn->setText(btn_text);
            btn->setHeight(50);
            btn->setMarginBottom(10);

            if (dl.host_name.find("Gofile") != std::string::npos || dl.host_name.find("1fichier") != std::string::npos) {
                btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
            } else {
                btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
            }

            btn->registerClickAction([this, dl](brls::View* view) {
                download::DownloadManager::getInstance().enqueue(this->item, dl.url, dl.host_name);
                brls::Application::notify("Đã thêm [" + this->item.title + "] vào Hàng đợi tải!");
                return true;
            });

            buttons_box->addView(btn);
        }
        root_box->addView(buttons_box);
    }

    scroll->setContentView(root_box);

    auto* applet = new brls::AppletFrame(scroll);
    applet->setTitle("Chi tiết tựa game");
    applet->setIcon("@res/icon.png");

    // Register B button to pop activity
    applet->registerAction("Quay lại", brls::ControllerButton::BUTTON_B, [](brls::View* view) {
        brls::Application::popActivity();
        return true;
    });

    this->setContentView(applet);
}

} // namespace switchfs::view
