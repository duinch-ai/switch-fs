#include "view/game_detail_activity.hpp"
#include "download/download_manager.hpp"
#include "utils/http.hpp"
#include <borealis.hpp>

namespace switchfs::view {

GameDetailActivity::GameDetailActivity(const api::GameItem& item) : item(item) {
}

brls::View* GameDetailActivity::createContentView() {
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    auto* root_box = new brls::Box(brls::Axis::COLUMN);
    root_box->setGrow(1.0f);
    root_box->setPadding(30, 40, 30, 40);

    // Section 1: Header (Cover Image + Metadata)
    auto* top_section = new brls::Box(brls::Axis::ROW);
    top_section->setMarginBottom(30);

    // Large Cover Image
    auto* cover_img = new brls::Image();
    cover_img->setWidth(220);
    cover_img->setHeight(220);
    cover_img->setCornerRadius(10);
    cover_img->setScalingType(brls::ImageScalingType::FIT);
    cover_img->setMarginRight(30);
    cover_img->setImageFromRes("img/borealis_256.png");

    if (!item.cover_url.empty()) {
        std::string url = item.cover_url;
        brls::async([cover_img, url]() {
            std::string data = utils::HttpClient::get(url);
            if (!data.empty()) {
                brls::sync([cover_img, data]() {
                    cover_img->setImageFromMem((const unsigned char*)data.data(), data.size());
                });
            }
        });
    }
    top_section->addView(cover_img);

    // Metadata Column
    auto* meta_col = new brls::Box(brls::Axis::COLUMN);
    meta_col->setGrow(1.0f);

    auto* title_label = new brls::Label();
    title_label->setText(item.title);
    title_label->setFontSize(26);
    title_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    title_label->setMarginBottom(15);
    meta_col->addView(title_label);

    // Badge Row: Format, Size, Date
    auto* badges_row = new brls::Box(brls::Axis::ROW);
    badges_row->setMarginBottom(12);

    auto* format_label = new brls::Label();
    format_label->setText("Format: " + item.format);
    format_label->setFontSize(16);
    format_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    format_label->setMarginRight(20);
    badges_row->addView(format_label);

    auto* size_label = new brls::Label();
    size_label->setText("Size: " + item.size);
    size_label->setFontSize(16);
    size_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    size_label->setMarginRight(20);
    badges_row->addView(size_label);

    auto* date_label = new brls::Label();
    date_label->setText("Updated: " + item.date);
    date_label->setFontSize(16);
    date_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    badges_row->addView(date_label);

    meta_col->addView(badges_row);

    // Language
    if (!item.language.empty()) {
        auto* lang_label = new brls::Label();
        lang_label->setText("Language: " + item.language);
        lang_label->setFontSize(15);
        lang_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        lang_label->setMarginBottom(10);
        meta_col->addView(lang_label);
    }

    // Hint note
    auto* hint_label = new brls::Label();
    hint_label->setText("Downloaded archives are automatically extracted and installed to Horizon OS.");
    hint_label->setFontSize(14);
    hint_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    meta_col->addView(hint_label);

    top_section->addView(meta_col);
    root_box->addView(top_section);

    // Section 2: Download Options
    auto* section_label = new brls::Label();
    section_label->setText("Select Download Host:");
    section_label->setFontSize(20);
    section_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    section_label->setMarginBottom(15);
    root_box->addView(section_label);

    if (item.download_links.empty()) {
        auto* empty_label = new brls::Label();
        empty_label->setText("No download links found for this title.");
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
                brls::Application::notify("Added [" + this->item.title + "] to Download Queue!");
                return true;
            });

            buttons_box->addView(btn);
        }
        root_box->addView(buttons_box);
    }

    scroll->setContentView(root_box);

    auto* applet = new brls::AppletFrame(scroll);
    applet->setTitle("Game Details");
    applet->setIcon(BRLS_ASSET("icon.png"));

    return applet;
}

} // namespace switchfs::view
