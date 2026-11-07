#include "view/search_tab.hpp"
#include "view/game_card.hpp"

namespace switchfs::view {

SearchTab::SearchTab() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(1.0f);
    this->setPadding(20, 20, 20, 20);

    // 1. Search Bar Button
    this->search_btn = new brls::Button();
    this->search_btn->setText("🔍  Nhấn A để nhập từ khóa tìm kiếm...");
    this->search_btn->setHeight(50);
    this->search_btn->setMarginBottom(15);
    this->search_btn->setStyle(&brls::BUTTONSTYLE_PRIMARY);

    this->search_btn->registerClickAction([this](brls::View* view) {
        brls::Application::getImeManager()->openForText([this](std::string text) {
            if (!text.empty()) {
                this->performSearch(text);
            }
        }, "Tìm kiếm game Switch", "Nhập tên tựa game cần tìm", 64, this->current_keyword);
        return true;
    });

    this->addView(this->search_btn);

    // 2. Status Label
    this->status_label = new brls::Label();
    this->status_label->setText("Nhập từ khóa để tra cứu hơn 15.800+ tựa game trên nswgf.com");
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

    this->search_btn->setText("🔍  " + keyword + " (Bấm A để đổi từ khóa)");
    this->status_label->setText("Đang tìm kiếm '" + keyword + "' trên nswgf.com...");

    // Clear previous results
    this->grid_container->clearViews();

    brls::async([this, keyword]() {
        auto games = api::NswgfApi::searchGames(keyword, 1, 24);

        brls::sync([this, games, keyword]() {
            this->is_searching = false;
            if (games.empty()) {
                this->status_label->setText("Không tìm thấy game nào với từ khóa '" + keyword + "'.");
                return;
            }

            this->status_label->setText("Tìm thấy " + std::to_string(games.size()) + " kết quả cho '" + keyword + "':");

            // Arrange cards in 4-column rows
            brls::Box* current_row = nullptr;
            for (size_t i = 0; i < games.size(); ++i) {
                if (i % 4 == 0) {
                    current_row = new brls::Box(brls::Axis::ROW);
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
