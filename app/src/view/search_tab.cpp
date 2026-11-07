#include "view/search_tab.hpp"
#include "view/game_card.hpp"

namespace switchfs::view {

SearchTab::SearchTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20, 20, 20, 20);

    // 1. Search Bar Button
    this->search_btn = new brls::Button();
    this->search_btn->setText("🔍  Press (A) to enter search keyword...");
    this->search_btn->setHeight(50);
    this->search_btn->setMarginBottom(15);
    this->search_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

    this->search_btn->registerClickAction([this](brls::View* view) {
        brls::Application::getImeManager()->openForText([this](std::string text) {
            if (!text.empty()) {
                this->performSearch(text);
            }
        }, "Search Nintendo Switch Games", "Enter game title to search", 64, this->current_keyword);
        return true;
    });

    this->addView(this->search_btn);

    // 2. Status Label
    this->status_label = new brls::Label();
    this->status_label->setText("Enter keywords to search across 15,800+ titles on nswgf.com");
    this->status_label->setFontSize(18);
    this->status_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    this->status_label->setMarginBottom(15);
    this->addView(this->status_label);

    // 3. Scrolling Results Area
    this->scrolling_frame = new brls::ScrollingFrame();
    this->scrolling_frame->setGrow(1.0f);

    this->content_box = new brls::Box(brls::Axis::COLUMN);
    this->content_box->setGrow(1.0f);

    this->grid_container = new brls::Box(brls::Axis::COLUMN);
    this->grid_container->setGrow(1.0f);
    this->content_box->addView(this->grid_container);

    this->scrolling_frame->setContentView(this->content_box);
    this->addView(this->scrolling_frame);
}

brls::View* SearchTab::create() {
    return new SearchTab();
}

void SearchTab::performSearch(const std::string& keyword) {
    if (this->is_searching) return;
    this->is_searching = true;
    this->current_keyword = keyword;

    this->search_btn->setText("🔍  " + keyword + " (Press A to change keyword)");
    this->status_label->setText("Searching for '" + keyword + "' on nswgf.com...");

    // Clear previous results
    this->grid_container->clearViews();

    brls::async([this, keyword]() {
        auto games = api::NswgfApi::searchGames(keyword, 1, 24);

        brls::sync([this, games, keyword]() {
            this->is_searching = false;
            if (games.empty()) {
                this->status_label->setText("No games found matching '" + keyword + "'.");
                return;
            }

            this->status_label->setText("Found " + std::to_string(games.size()) + " results for '" + keyword + "':");

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
        });
    });
}

} // namespace switchfs::view
