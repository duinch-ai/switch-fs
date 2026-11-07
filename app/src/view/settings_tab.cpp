#include "view/settings_tab.hpp"
#include <borealis/views/cells/cell_selector.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <borealis/views/cells/cell_detail.hpp>

namespace switchfs::view {

SettingsTab::SettingsTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->content_box = new brls::Box(brls::Axis::COLUMN);
    this->content_box->setGrow(1.0f);
    this->content_box->setPadding(20, 30, 20, 30);

    auto& sm = data::SettingsManager::getInstance();
    auto& settings = sm.getSettings();

    // 1. Nhóm: Cài đặt lưu trữ & Động cơ cài đặt
    auto* section1_label = new brls::Label();
    section1_label->setText("CÀI ĐẶT LƯU TRỮ & CÀI ĐẶT GAME");
    section1_label->setFontSize(16);
    section1_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    section1_label->setMarginBottom(10);
    this->content_box->addView(section1_label);

    // Vị trí cài đặt mặc định
    auto* storage_cell = new brls::SelectorCell();
    std::vector<std::string> storage_options = {
        "Thẻ nhớ MicroSD (Khuyên dùng)",
        "Bộ nhớ trong máy (NAND)"
    };
    storage_cell->init("Vị trí cài đặt mặc định", storage_options, settings.default_storage, [&settings, &sm](int selected) {
        settings.default_storage = selected;
        sm.save();
    });
    this->content_box->addView(storage_cell);

    // Tự động cài đặt sau khi tải
    auto* auto_install_cell = new brls::BooleanCell();
    auto_install_cell->init("Tự động cài đặt sau khi tải", settings.auto_install, [&settings, &sm](bool on) {
        settings.auto_install = on;
        sm.save();
    });
    this->content_box->addView(auto_install_cell);

    // Tự động xóa file nén (.rar) sau khi cài đặt thành công
    auto* auto_delete_cell = new brls::BooleanCell();
    auto_delete_cell->init("Tự động xóa file nén (.rar) sau khi cài", settings.auto_delete_archive, [&settings, &sm](bool on) {
        settings.auto_delete_archive = on;
        sm.save();
    });
    this->content_box->addView(auto_delete_cell);

    // 2. Nhóm: Tài khoản Host & Debrid
    auto* section2_label = new brls::Label();
    section2_label->setText("TÀI KHOẢN HOST & DEBRID CAO CẤP");
    section2_label->setFontSize(16);
    section2_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    section2_label->setMarginTop(25);
    section2_label->setMarginBottom(10);
    this->content_box->addView(section2_label);

    // Gofile API Token
    auto* gofile_cell = new brls::InputCell();
    gofile_cell->init("Gofile API Token", settings.gofile_token, [&settings, &sm](std::string text) {
        settings.gofile_token = text;
        sm.save();
    }, "Nhập Token", "Nhập token cá nhân nếu có tài khoản VIP");
    this->content_box->addView(gofile_cell);

    // 1fichier API Key
    auto* onefichier_cell = new brls::InputCell();
    onefichier_cell->init("1fichier API Key", settings.onefichier_key, [&settings, &sm](std::string text) {
        settings.onefichier_key = text;
        sm.save();
    }, "Nhập API Key", "API key từ 1fichier.com để tải tốc độ cao");
    this->content_box->addView(onefichier_cell);

    // Debrid API Key (TorBox / Real-Debrid)
    auto* debrid_cell = new brls::InputCell();
    debrid_cell->init("Debrid API Key (TorBox / Real-Debrid)", settings.debrid_key, [&settings, &sm](std::string text) {
        settings.debrid_key = text;
        sm.save();
    }, "Nhập API Key", "Mở khóa link tải tốc độ cao cho mọi host");
    this->content_box->addView(debrid_cell);

    // 3. Nhóm: Thông tin Ứng dụng & Hệ thống
    auto* section3_label = new brls::Label();
    section3_label->setText("THÔNG TIN HỆ THỐNG");
    section3_label->setFontSize(16);
    section3_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    section3_label->setMarginTop(25);
    section3_label->setMarginBottom(10);
    this->content_box->addView(section3_label);

    auto* version_cell = new brls::DetailCell();
    version_cell->setText("Phiên bản Switch FS");
    version_cell->setDetailText("v0.1.0-release (Horizon Target)");
    this->content_box->addView(version_cell);

    auto* source_cell = new brls::DetailCell();
    source_cell->setText("Nguồn dữ liệu game");
    source_cell->setDetailText("nswgf.com (15.800+ Switch titles)");
    this->content_box->addView(source_cell);

    auto* platform_cell = new brls::DetailCell();
    platform_cell->setText("Nền tảng đang chạy");
#if defined(__SWITCH__)
    platform_cell->setDetailText("Nintendo Switch (Atmosphère / Hekate)");
#else
    platform_cell->setDetailText("macOS Simulator (SDL2 / OpenGL)");
#endif
    this->content_box->addView(platform_cell);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);
}

brls::View* SettingsTab::create() {
    return new SettingsTab();
}

} // namespace switchfs::view
