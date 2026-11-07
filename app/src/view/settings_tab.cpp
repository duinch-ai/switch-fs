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

    // 1. Group: Storage & Game Installation
    auto* section1_label = new brls::Label();
    section1_label->setText("STORAGE & GAME INSTALLATION");
    section1_label->setFontSize(16);
    section1_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    section1_label->setMarginBottom(10);
    this->content_box->addView(section1_label);

    // Default install location
    auto* storage_cell = new brls::SelectorCell();
    std::vector<std::string> storage_options = {
        "MicroSD Card (Recommended)",
        "Internal Memory (NAND)"
    };
    storage_cell->init("Default Install Location", storage_options, settings.default_storage, [&settings, &sm](int selected) {
        settings.default_storage = selected;
        sm.save();
    });
    this->content_box->addView(storage_cell);

    // Auto-install after download
    auto* auto_install_cell = new brls::BooleanCell();
    auto_install_cell->init("Auto-install after download", settings.auto_install, [&settings, &sm](bool on) {
        settings.auto_install = on;
        sm.save();
    });
    this->content_box->addView(auto_install_cell);

    // Auto-delete archive (.rar) after install
    auto* auto_delete_cell = new brls::BooleanCell();
    auto_delete_cell->init("Auto-delete archive (.rar) after install", settings.auto_delete_archive, [&settings, &sm](bool on) {
        settings.auto_delete_archive = on;
        sm.save();
    });
    this->content_box->addView(auto_delete_cell);

    // 2. Group: Host Accounts & Debrid
    auto* section2_label = new brls::Label();
    section2_label->setText("HOST ACCOUNTS & DEBRID API");
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
    }, "Enter Token", "Account token for VIP download speeds");
    this->content_box->addView(gofile_cell);

    // 1fichier API Key
    auto* onefichier_cell = new brls::InputCell();
    onefichier_cell->init("1fichier API Key", settings.onefichier_key, [&settings, &sm](std::string text) {
        settings.onefichier_key = text;
        sm.save();
    }, "Enter API Key", "API key from 1fichier.com for direct download");
    this->content_box->addView(onefichier_cell);

    // Debrid API Key (TorBox / Real-Debrid)
    auto* debrid_cell = new brls::InputCell();
    debrid_cell->init("Debrid API Key (TorBox / Real-Debrid)", settings.debrid_key, [&settings, &sm](std::string text) {
        settings.debrid_key = text;
        sm.save();
    }, "Enter API Key", "Unrestrictor key for high speed host links");
    this->content_box->addView(debrid_cell);

    // 3. Group: System & Application Info
    auto* section3_label = new brls::Label();
    section3_label->setText("SYSTEM INFORMATION");
    section3_label->setFontSize(16);
    section3_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    section3_label->setMarginTop(25);
    section3_label->setMarginBottom(10);
    this->content_box->addView(section3_label);

    auto* version_cell = new brls::DetailCell();
    version_cell->setText("Switch FS Version");
    version_cell->setDetailText("v0.1.0-release (Horizon Target)");
    this->content_box->addView(version_cell);

    auto* source_cell = new brls::DetailCell();
    source_cell->setText("Game Database Source");
    source_cell->setDetailText("nswgf.com (15,800+ Switch titles)");
    this->content_box->addView(source_cell);

    auto* platform_cell = new brls::DetailCell();
    platform_cell->setText("Current Platform");
#if defined(__SWITCH__)
    platform_cell->setDetailText("Nintendo Switch (Atmosphere / Hekate)");
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
