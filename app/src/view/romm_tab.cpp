#include "view/romm_tab.hpp"
#include "view/game_card.hpp"
#include "data/settings_manager.hpp"

namespace switchfs::view {

RommTab::RommTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20, 20, 20, 20);

    // 1. Top Control Bar (GridRow for D-pad horizontal navigation)
    auto* top_bar = new GridRow();
    top_bar->setMarginBottom(12);

    this->url_btn = new brls::Button();
    this->url_btn->setText("\xEE\x87\x9B  Server URL");
    this->url_btn->setWidth(190);
    this->url_btn->setHeight(44);
    this->url_btn->setMarginRight(10);
    this->url_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    this->url_btn->registerClickAction([this](brls::View* v) {
        auto& s = data::SettingsManager::getInstance().getSettings();
        brls::Application::getImeManager()->openForText([this](std::string text) {
            if (!text.empty()) {
                auto& s = data::SettingsManager::getInstance().getSettings();
                s.romm_url = api::RommApi::normalizeBaseUrl(text);
                s.romm_token.clear();
                data::SettingsManager::getInstance().save();
                this->loadRoms(1, "");
            }
        }, "RomM Server URL", "e.g. http://192.168.1.100:8080", 128,
           s.romm_url.empty() ? "http://192.168.1." : s.romm_url);
        return true;
    });
    top_bar->addView(this->url_btn);

    this->auth_btn = new brls::Button();
    this->auth_btn->setText("\xEE\xA2\xB8  Credentials");
    this->auth_btn->setWidth(190);
    this->auth_btn->setHeight(44);
    this->auth_btn->setMarginRight(10);
    this->auth_btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
    this->auth_btn->registerClickAction([this](brls::View* v) {
        auto& s = data::SettingsManager::getInstance().getSettings();
        std::string current = s.romm_username.empty() ? "" : (s.romm_username + ":" + s.romm_password);
        brls::Application::getImeManager()->openForText([this](std::string text) {
            auto& s = data::SettingsManager::getInstance().getSettings();
            size_t colon = text.find(':');
            if (colon != std::string::npos) {
                s.romm_username = text.substr(0, colon);
                s.romm_password = text.substr(colon + 1);
            } else {
                s.romm_token = text;
            }
            data::SettingsManager::getInstance().save();
            this->loadRoms(1, this->current_keyword);
        }, "RomM Auth (user:password or token)", "Format: username:password", 128, current);
        return true;
    });
    top_bar->addView(this->auth_btn);

    this->search_btn = new brls::Button();
    this->search_btn->setText("\xEE\xA2\xB6  Search RomM");
    this->search_btn->setWidth(190);
    this->search_btn->setHeight(44);
    this->search_btn->setMarginRight(10);
    this->search_btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
    this->search_btn->registerClickAction([this](brls::View* v) {
        brls::Application::getImeManager()->openForText([this](std::string text) {
            this->loadRoms(1, text);
        }, "Search RomM Library", "Enter game title...", 64, this->current_keyword);
        return true;
    });
    top_bar->addView(this->search_btn);

    this->sync_btn = new brls::Button();
    this->sync_btn->setText("\xEE\x98\xA7  Sync");
    this->sync_btn->setWidth(130);
    this->sync_btn->setHeight(44);
    this->sync_btn->setStyle(&brls::BUTTONSTYLE_DEFAULT);
    this->sync_btn->registerClickAction([this](brls::View* v) {
        this->loadRoms(1, "");
        return true;
    });
    top_bar->addView(this->sync_btn);

    this->addView(top_bar);

    // 2. Status Label
    this->status_label = new brls::Label();
    this->status_label->setFontSize(16);
    this->status_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    this->status_label->setMarginBottom(12);
    this->addView(this->status_label);

    // 3. Scrolling 5-column Grid
    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->content_box = new brls::Box(brls::Axis::COLUMN);
    this->content_box->setGrow(1.0f);

    this->grid_container = new brls::Box(brls::Axis::COLUMN);
    this->grid_container->setGrow(1.0f);
    this->content_box->addView(this->grid_container);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);

    const auto& s = data::SettingsManager::getInstance().getSettings();
    if (s.romm_url.empty()) {
        this->status_label->setText("RomM Local Server: Not configured. Click [Server URL] above to connect to your self-hosted RomM library.");
    } else {
        this->loadRoms(1, "");
    }
}

brls::View* RommTab::create() {
    return new RommTab();
}

void RommTab::loadRoms(int page, const std::string& keyword) {
    if (this->is_loading) return;
    if (page > 1 && !this->has_more) return;

    const auto& s = data::SettingsManager::getInstance().getSettings();
    if (s.romm_url.empty()) {
        this->status_label->setText("Please configure your RomM Server URL first.");
        return;
    }

    this->is_loading = true;
    this->current_page = page;
    this->current_keyword = keyword;

    if (page == 1) {
        this->has_more = true;
        this->grid_container->clearViews();
    }

    this->status_label->setText("Connecting to RomM (" + s.romm_url + ") - Page " + std::to_string(page) + "...");

    brls::async([this, page, keyword]() {
        auto games = api::RommApi::getRoms(page, 20, keyword);

        brls::sync([this, games, page, keyword]() {
            this->is_loading = false;
            const auto& s = data::SettingsManager::getInstance().getSettings();

            if (games.empty()) {
                this->has_more = false;
                if (page == 1) {
                    this->status_label->setText("No ROMs returned from " + s.romm_url + ". Check server URL or credentials.");
                }
                return;
            }

            if (games.size() < 20) {
                this->has_more = false;
            }

            this->status_label->setText("RomM Library (" + s.romm_url + ") - Page " + std::to_string(page));

            GridRow* current_row = nullptr;
            for (size_t i = 0; i < games.size(); ++i) {
                if (i % 5 == 0) {
                    current_row = new GridRow();
                    current_row->setMarginBottom(14);
                    this->grid_container->addView(current_row);
                }
                auto* card = GameCard::create(games[i]);
                if (current_row) {
                    current_row->addView(card);
                    GridRow* row_ptr = current_row;
                    card->getFocusEvent()->subscribe([this, row_ptr](brls::View* v) {
                        if (!this->has_more || this->is_loading) return;
                        auto& children = this->grid_container->getChildren();
                        if (children.size() >= 2) {
                            if (row_ptr == children.back() || row_ptr == children[children.size() - 2]) {
                                this->loadRoms(this->current_page + 1, this->current_keyword);
                            }
                        } else if (!children.empty() && row_ptr == children.back()) {
                            this->loadRoms(this->current_page + 1, this->current_keyword);
                        }
                    });
                }
            }
        });
    });
}

} // namespace switchfs::view
