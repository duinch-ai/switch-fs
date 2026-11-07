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
    this->content_box->setPadding(20, 20, 20, 20);

    // Status / Loading label
    this->status_label = new brls::Label();
    this->status_label->setText("Loading game catalog from nswgf.com...");
    this->status_label->setFontSize(18);
    this->status_label->setMarginBottom(15);
    this->content_box->addView(this->status_label);

    // Container for card rows
    this->grid_container = new brls::Box(brls::Axis::COLUMN);
    this->grid_container->setGrow(1.0f);
    this->content_box->addView(this->grid_container);

    // Load More Button
    this->load_more_btn = new brls::Button();
    this->load_more_btn->setText("Load more games (Next page)");
    this->load_more_btn->setHeight(48);
    this->load_more_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);
    this->load_more_btn->setMarginTop(15);
    this->load_more_btn->setMarginBottom(30);
    this->load_more_btn->setVisibility(brls::Visibility::GONE);
    this->load_more_btn->registerClickAction([this](brls::View* view) {
        this->loadGames(this->current_page + 1);
        return true;
    });
    this->content_box->addView(this->load_more_btn);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);

    brls::Logger::info("HomeTab created, calling loadGames(1)...");
    // Load first page
    this->loadGames(1);
}

brls::View* HomeTab::create() {
    return new HomeTab();
}

void HomeTab::loadGames(int page) {
    if (this->is_loading) return;
    this->is_loading = true;
    this->status_label->setText("Connecting to nswgf.com API...");
    brls::Logger::info("HomeTab: Loading page {} from nswgf.com...", page);

    brls::async([this, page]() {
        auto games = api::NswgfApi::getLatestGames(page, 20);
        brls::Logger::info("HomeTab: Fetched {} games from API", games.size());

        brls::sync([this, games, page]() {
            this->is_loading = false;
            if (games.empty()) {
                this->status_label->setText("Unable to connect or no data returned from nswgf.com.");
                return;
            }

            this->status_label->setText("Page " + std::to_string(page) + " • Found " + std::to_string(games.size()) + " games");
            this->current_page = page;

            // Arrange cards in 4-column rows
            GridRow* current_row = nullptr;
            for (size_t i = 0; i < games.size(); ++i) {
                if (i % 4 == 0) {
                    current_row = new GridRow();
                    current_row->setMarginBottom(15);
                    this->grid_container->addView(current_row);
                }
                auto* card = GameCard::create(games[i]);
                if (current_row) {
                    current_row->addView(card);
                }
            }
            if (this->load_more_btn) {
                this->load_more_btn->setText("Load more games (Page " + std::to_string(this->current_page + 1) + ")");
                this->load_more_btn->setVisibility(brls::Visibility::VISIBLE);
            }
            brls::Logger::info("HomeTab: Rendered {} game cards", games.size());
        });
    });
}

} // namespace switchfs::view
