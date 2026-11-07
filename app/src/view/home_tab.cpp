#include "view/home_tab.hpp"
#include "view/game_card.hpp"

namespace switchfs::view {

HomeTab::HomeTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);

    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->content_box = new brls::Box(brls::Axis::COLUMN);
    this->content_box->setGrow(1.0f);
    this->content_box->setPadding(16, 20, 16, 20);

    // Status / Loading label
    this->status_label = new brls::Label();
    this->status_label->setText("Loading game catalog from nswgf.com...");
    this->status_label->setFontSize(16);
    this->status_label->setMarginBottom(12);
    this->content_box->addView(this->status_label);

    // Container for card rows
    this->grid_container = new brls::Box(brls::Axis::COLUMN);
    this->grid_container->setGrow(1.0f);
    this->content_box->addView(this->grid_container);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);

    brls::Logger::info("HomeTab created, calling loadGames(1)...");
    this->loadGames(1);
}

brls::View* HomeTab::create() {
    return new HomeTab();
}

void HomeTab::loadGames(int page) {
    if (this->is_loading) return;
    this->is_loading = true;
    this->status_label->setText("Loading Page " + std::to_string(page) + " from nswgf.com...");
    brls::Logger::info("HomeTab: Loading page {} from nswgf.com...", page);

    brls::async([this, page]() {
        auto games = api::NswgfApi::getLatestGames(page, 20);
        brls::Logger::info("HomeTab: Fetched {} games from API", games.size());

        brls::sync([this, games, page]() {
            this->is_loading = false;
            if (games.empty()) {
                if (page == 1) {
                    this->status_label->setText("Unable to connect or no data returned from nswgf.com.");
                }
                return;
            }

            this->current_page = page;
            size_t total_loaded = (this->grid_container->getChildren().size() * 5) + games.size();
            this->status_label->setText("Loaded " + std::to_string(total_loaded) + " titles (Page " + std::to_string(page) + ") • Scroll down to auto-load more");

            // Arrange cards in 5-column rows with Infinite Scroll trigger
            GridRow* current_row = nullptr;
            for (size_t i = 0; i < games.size(); ++i) {
                if (i % 5 == 0) {
                    current_row = new GridRow();
                    current_row->setMarginBottom(8);
                    this->grid_container->addView(current_row);
                }
                auto* card = GameCard::create(games[i]);
                if (current_row) {
                    current_row->addView(card);
                }

                // Infinite scroll: when focusing a card in the last 2 rows, auto-load next page
                card->getFocusEvent()->subscribe([this](brls::View* view) {
                    if (this->is_loading) return;
                    auto& rows = this->grid_container->getChildren();
                    if (!rows.empty()) {
                        brls::View* parent_row = view->getParent();
                        if (parent_row == rows.back() || (rows.size() >= 2 && parent_row == rows[rows.size() - 2])) {
                            this->loadGames(this->current_page + 1);
                        }
                    }
                });
            }

            brls::Logger::info("HomeTab: Rendered {} game cards for page {}", games.size(), page);
        });
    });
}

} // namespace switchfs::view
