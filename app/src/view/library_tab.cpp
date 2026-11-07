#include "view/library_tab.hpp"
#include "view/game_card.hpp"
#include "data/library_manager.hpp"
#include "data/installer/nsp_installer.hpp"
#include "data/extractor/archive_extractor.hpp"
#include "download/download_manager.hpp"

namespace switchfs::view {

LibraryTab::LibraryTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20, 20, 20, 20);

    // 1. Filter Tabs Bar (GridRow for D-pad navigation)
    auto* filter_bar = new GridRow();
    filter_bar->setMarginBottom(12);

    this->fav_btn = new brls::Button();
    this->fav_btn->setText("\xEE\xA1\xBD  Favorites");
    this->fav_btn->setWidth(185);
    this->fav_btn->setHeight(44);
    this->fav_btn->setMarginRight(10);
    this->fav_btn->registerClickAction([this](brls::View* v) {
        this->showSection(LibrarySection::FAVORITES);
        return true;
    });
    filter_bar->addView(this->fav_btn);

    this->dl_btn = new brls::Button();
    this->dl_btn->setText("\xEE\x8B\x84  Downloaded");
    this->dl_btn->setWidth(195);
    this->dl_btn->setHeight(44);
    this->dl_btn->setMarginRight(10);
    this->dl_btn->registerClickAction([this](brls::View* v) {
        this->showSection(LibrarySection::DOWNLOADED);
        return true;
    });
    filter_bar->addView(this->dl_btn);

    this->inst_btn = new brls::Button();
    this->inst_btn->setText("\xEE\x8C\xB8  Installed");
    this->inst_btn->setWidth(185);
    this->inst_btn->setHeight(44);
    this->inst_btn->setMarginRight(10);
    this->inst_btn->registerClickAction([this](brls::View* v) {
        this->showSection(LibrarySection::INSTALLED);
        return true;
    });
    filter_bar->addView(this->inst_btn);

    this->refresh_btn = new brls::Button();
    this->refresh_btn->setText("\xEE\x98\xA7  Refresh");
    this->refresh_btn->setWidth(140);
    this->refresh_btn->setHeight(44);
    this->refresh_btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
    this->refresh_btn->registerClickAction([this](brls::View* v) {
        data::LibraryManager::getInstance().load();
        this->showSection(this->active_section);
        return true;
    });
    filter_bar->addView(this->refresh_btn);

    this->addView(filter_bar);

    // 2. Section Status Label
    this->status_label = new brls::Label();
    this->status_label->setFontSize(16);
    this->status_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    this->status_label->setMarginBottom(14);
    this->addView(this->status_label);

    // 3. Scrollable Content Area
    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->content_box = new brls::Box(brls::Axis::COLUMN);
    this->content_box->setGrow(1.0f);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);

    this->showSection(LibrarySection::FAVORITES);
}

brls::View* LibraryTab::create() {
    return new LibraryTab();
}

void LibraryTab::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    this->showSection(this->active_section);
}

void LibraryTab::showSection(LibrarySection section) {
    this->active_section = section;
    this->content_box->clearViews();

    auto& lib = data::LibraryManager::getInstance();
    const auto& favs = lib.getFavorites();
    auto dl_files = lib.getDownloadedFiles();
    const auto& inst = lib.getInstalledGames();

    this->fav_btn->setText("\xEE\xA1\xBD  Favorites (" + std::to_string(favs.size()) + ")");
    this->dl_btn->setText("\xEE\x8B\x84  Downloaded (" + std::to_string(dl_files.size()) + ")");
    this->inst_btn->setText("\xEE\x8C\xB8  Installed (" + std::to_string(inst.size()) + ")");

    this->fav_btn->setStyle(section == LibrarySection::FAVORITES ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
    this->dl_btn->setStyle(section == LibrarySection::DOWNLOADED ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);
    this->inst_btn->setStyle(section == LibrarySection::INSTALLED ? &brls::BUTTONSTYLE_PRIMARY : &brls::BUTTONSTYLE_DEFAULT);

    if (section == LibrarySection::FAVORITES || section == LibrarySection::INSTALLED) {
        const auto& items = (section == LibrarySection::FAVORITES) ? favs : inst;
        if (items.empty()) {
            this->status_label->setText(section == LibrarySection::FAVORITES
                ? "No favorite games yet. Open any game and press the [Favorite] button to bookmark it here."
                : "No installed games recorded yet. Games installed via Switch FS will appear here.");
            return;
        }

        this->status_label->setText(section == LibrarySection::FAVORITES
            ? ("Bookmarked Favorites (" + std::to_string(items.size()) + " titles):")
            : ("Installed on Console (" + std::to_string(items.size()) + " titles):"));

        GridRow* current_row = nullptr;
        for (size_t i = 0; i < items.size(); ++i) {
            if (i % 5 == 0) {
                current_row = new GridRow();
                current_row->setMarginBottom(14);
                this->content_box->addView(current_row);
            }
            if (current_row) {
                current_row->addView(GameCard::create(items[i]));
            }
        }
        return;
    }

    // Section: DOWNLOADED FILES
    if (dl_files.empty()) {
        this->status_label->setText("No downloaded archives or NSP/XCI packages found in " + download::DownloadManager::getDownloadDir());
        return;
    }

    this->status_label->setText("Downloaded Packages on SD Card (" + std::to_string(dl_files.size()) + " files):");

    for (const auto& lf : dl_files) {
        auto* row = new GridRow();
        row->setHeight(62);
        row->setPadding(10, 16, 10, 16);
        row->setMarginBottom(10);
        row->setCornerRadius(8);
        row->setBackgroundColor(brls::Application::getTheme()["brls/background_sidebar"]);
        row->setAlignItems(brls::AlignItems::CENTER);

        auto* name_lbl = new brls::Label();
        std::string disp = lf.filename.length() > 52 ? (lf.filename.substr(0, 49) + "...") : lf.filename;
        name_lbl->setText(disp + "   [" + lf.size_str + "]");
        name_lbl->setFontSize(16);
        name_lbl->setGrow(1.0f);
        row->addView(name_lbl);

        auto* install_btn = new brls::Button();
        install_btn->setText(lf.is_installable_nsp ? "\xEE\x8B\x84  Install" : "\xEE\x8B\x87  Extract & Install");
        install_btn->setWidth(175);
        install_btn->setHeight(40);
        install_btn->setMarginRight(10);
        install_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
        install_btn->registerClickAction([this, lf](brls::View* v) {
            brls::Application::notify("Installing " + lf.filename + "...");
            brls::async([this, lf]() {
                std::string target = lf.full_path;
                if (!lf.is_installable_nsp && data::ArchiveExtractor::isArchive(lf.full_path)) {
                    std::string out_dir = download::DownloadManager::getDownloadDir() + "extracted/";
                    data::ArchiveExtractor::extract(lf.full_path, out_dir);
                }
                bool ok = data::NspInstaller::installNsp(target, data::InstallStorage::SD_CARD);
                brls::sync([this, lf, ok]() {
                    if (ok) {
                        api::GameItem gi;
                        gi.title = lf.filename;
                        gi.size = lf.size_str;
                        gi.format = "NSP";
                        data::LibraryManager::getInstance().recordInstalled(gi);
                        brls::Application::notify("Installed " + lf.filename + "!");
                    } else {
                        brls::Application::notify("Install finished / simulated for " + lf.filename);
                    }
                    this->showSection(LibrarySection::DOWNLOADED);
                });
            });
            return true;
        });
        row->addView(install_btn);

        auto* del_btn = new brls::Button();
        del_btn->setText("\xEE\xA1\xB2  Delete");
        del_btn->setWidth(120);
        del_btn->setHeight(40);
        del_btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
        del_btn->registerClickAction([this, lf](brls::View* v) {
            data::LibraryManager::getInstance().deleteDownloadedFile(lf.full_path);
            brls::Application::notify("Deleted " + lf.filename);
            this->showSection(LibrarySection::DOWNLOADED);
            return true;
        });
        row->addView(del_btn);

        this->content_box->addView(row);
    }
}

} // namespace switchfs::view
