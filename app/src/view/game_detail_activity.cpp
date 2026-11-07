#include "view/game_detail_activity.hpp"
#include "view/game_card.hpp"
#include "download/download_manager.hpp"
#include "utils/http.hpp"
#include <borealis.hpp>

namespace switchfs::view {

GameDetailActivity::GameDetailActivity(const api::GameItem& item) : item(item) {
}

static std::string getHostCompactIconLabel(const std::string& host_name) {
    if (host_name == "Send.now") return "\xEE\x85\xA3  Send";
    if (host_name == "1fichier") return "\xEE\x8B\x80  1F";
    if (host_name == "Gofile")   return "\xEE\x8B\x87  GF";
    if (host_name == "MegaUp")   return "\xEE\x8B\x84  MU";
    if (host_name == "Datanodes") return "\xEE\x85\x97  DN";
    return "\xEE\x8B\x84  " + host_name.substr(0, 4);
}

brls::View* GameDetailActivity::createContentView() {
    auto* scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);

    auto* root_box = new brls::Box(brls::Axis::COLUMN);
    root_box->setGrow(1.0f);
    root_box->setPadding(28, 36, 28, 36);

    // Section 1: Header (Cover Image + Metadata)
    auto* top_section = new brls::Box(brls::Axis::ROW);
    top_section->setMarginBottom(24);

    auto* cover_img = new brls::Image();
    cover_img->setWidth(180);
    cover_img->setHeight(180);
    cover_img->setCornerRadius(10);
    cover_img->setScalingType(brls::ImageScalingType::FIT);
    cover_img->setMarginRight(26);
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

    auto* meta_col = new brls::Box(brls::Axis::COLUMN);
    meta_col->setGrow(1.0f);

    auto* title_label = new brls::Label();
    title_label->setText(item.title);
    title_label->setFontSize(24);
    title_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    title_label->setMarginBottom(12);
    meta_col->addView(title_label);

    auto* badges_row = new brls::Box(brls::Axis::ROW);
    badges_row->setMarginBottom(10);

    auto* format_label = new brls::Label();
    format_label->setText("Format: " + item.format);
    format_label->setFontSize(15);
    format_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    format_label->setMarginRight(20);
    badges_row->addView(format_label);

    auto* size_label = new brls::Label();
    size_label->setText("Size: " + item.size);
    size_label->setFontSize(15);
    size_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    size_label->setMarginRight(20);
    badges_row->addView(size_label);

    auto* date_label = new brls::Label();
    date_label->setText("Updated: " + item.date);
    date_label->setFontSize(15);
    date_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    badges_row->addView(date_label);

    meta_col->addView(badges_row);

    if (!item.language.empty()) {
        auto* lang_label = new brls::Label();
        lang_label->setText("Language: " + item.language);
        lang_label->setFontSize(14);
        lang_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        lang_label->setMarginBottom(8);
        meta_col->addView(lang_label);
    }

    auto* hint_label = new brls::Label();
    hint_label->setText("Direct download & auto-install supported (Send.now, 1fichier, Gofile + mirror fallback).");
    hint_label->setFontSize(13);
    hint_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    meta_col->addView(hint_label);

    top_section->addView(meta_col);
    root_box->addView(top_section);

    // Section 2: Compact Icon Download Links
    auto* section_label = new brls::Label();
    section_label->setText("Download Mirrors:");
    section_label->setFontSize(18);
    section_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    section_label->setMarginBottom(6);
    root_box->addView(section_label);

    auto* legend_label = new brls::Label();
    legend_label->setText("Auto = Fastest Direct Mirror   •   Send = Send.now   •   1F = 1fichier   •   GF = Gofile   •   MU = MegaUp   •   DN = Datanodes");
    legend_label->setFontSize(13);
    legend_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    legend_label->setMarginBottom(16);
    root_box->addView(legend_label);

    if (item.download_links.empty()) {
        auto* empty_label = new brls::Label();
        empty_label->setText("No download links found for this title.");
        empty_label->setFontSize(16);
        empty_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        root_box->addView(empty_label);
    } else {
        auto* groups_box = new brls::Box(brls::Axis::COLUMN);

        // Group links into packages (each package on nswgf has up to 5 mirrors, starting when host repeats)
        std::vector<std::vector<api::DownloadLink>> groups;
        std::vector<api::DownloadLink> current_group;
        for (const auto& dl : item.download_links) {
            bool host_already_in_group = false;
            for (const auto& existing : current_group) {
                if (existing.host_name == dl.host_name) {
                    host_already_in_group = true;
                    break;
                }
            }
            if (host_already_in_group && !current_group.empty()) {
                groups.push_back(current_group);
                current_group.clear();
            }
            current_group.push_back(dl);
        }
        if (!current_group.empty()) {
            groups.push_back(current_group);
        }

        for (size_t g_idx = 0; g_idx < groups.size(); ++g_idx) {
            const auto& group = groups[g_idx];

            // Determine package label (e.g. Base Game vs Patch / DLC)
            std::string pkg_title = (g_idx == 0) ? "Base Game Package" : ("Patch / DLC #" + std::to_string(g_idx));
            for (const auto& dl : group) {
                if (dl.url.find("[UPD") != std::string::npos || dl.url.find("UPD]") != std::string::npos || dl.url.find("DDUDLC") != std::string::npos) {
                    pkg_title = "Patch / DLC Package";
                    break;
                } else if (dl.url.find("[DLC") != std::string::npos) {
                    pkg_title = "DLC Package";
                    break;
                } else if (dl.url.find("[BASE]") != std::string::npos) {
                    pkg_title = "Base Game Package";
                    break;
                }
            }

            auto* pkg_label = new brls::Label();
            pkg_label->setText(pkg_title);
            pkg_label->setFontSize(15);
            pkg_label->setTextColor(brls::Application::getTheme()["brls/text"]);
            pkg_label->setMarginBottom(8);
            groups_box->addView(pkg_label);

            auto* row = new GridRow();
            row->setMarginBottom(16);

            // Pick best link for "Auto" button (Send.now > 1fichier > Gofile > first)
            api::DownloadLink best_dl = group.front();
            for (const std::string& pref : {"Send.now", "1fichier", "Gofile"}) {
                bool found = false;
                for (const auto& dl : group) {
                    if (dl.host_name == pref) {
                        best_dl = dl;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }

            api::GameItem pkg_item = this->item;
            pkg_item.download_links = group;
            if (g_idx > 0) {
                pkg_item.title = this->item.title + " (" + pkg_title + ")";
            }

            // 1. Compact "Auto" Icon Button
            auto* auto_btn = new brls::Button();
            auto_btn->setText("\xEE\x8F\xA7  Auto");
            auto_btn->setWidth(116);
            auto_btn->setHeight(44);
            auto_btn->setMarginRight(10);
            auto_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
            auto_btn->registerClickAction([pkg_item, best_dl](brls::View* view) {
                download::DownloadManager::getInstance().enqueue(pkg_item, best_dl.url, best_dl.host_name);
                brls::Application::notify("Downloading [" + pkg_item.title + "] via " + best_dl.host_name + "...");
                return true;
            });
            row->addView(auto_btn);

            // 2. Compact Host Icon Buttons
            for (const auto& dl : group) {
                auto* btn = new brls::Button();
                btn->setText(getHostCompactIconLabel(dl.host_name));
                btn->setWidth(106);
                btn->setHeight(44);
                btn->setMarginRight(10);
                btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);

                btn->registerClickAction([pkg_item, dl](brls::View* view) {
                    download::DownloadManager::getInstance().enqueue(pkg_item, dl.url, dl.host_name);
                    brls::Application::notify("Queued [" + pkg_item.title + "] (" + dl.host_name + ")");
                    return true;
                });

                row->addView(btn);
            }

            groups_box->addView(row);
        }

        root_box->addView(groups_box);
    }

    scroll->setContentView(root_box);

    auto* applet = new brls::AppletFrame(scroll);
    applet->setTitle("Game Details");
    applet->setIcon(BRLS_ASSET("icon.png"));

    return applet;
}

} // namespace switchfs::view
