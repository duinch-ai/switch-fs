#include "view/game_card.hpp"
#include "view/game_detail_activity.hpp"
#include "utils/http.hpp"

namespace switchfs::view {

GameCard::GameCard(const api::GameItem& item) : item(item) {
    this->setFocusable(true);
    this->setAxis(brls::Axis::COLUMN);
    this->setWidth(220);
    this->setHeight(320);
    this->setMarginLeft(10);
    this->setMarginRight(10);
    this->setMarginTop(10);
    this->setMarginBottom(10);
    this->setCornerRadius(8);
    this->setShadowType(brls::ShadowType::GENERIC);

    // 1. Cover Image (Poster aspect ratio)
    this->cover_image = new brls::Image();
    this->cover_image->setWidth(220);
    this->cover_image->setHeight(250);
    this->cover_image->setCornerRadius(8);
    this->cover_image->setScalingType(brls::ImageScalingType::FIT);
    this->cover_image->setImageFromRes("img/borealis_256.png");

    if (!item.cover_url.empty()) {
        std::string url = item.cover_url;
        brls::async([this, url]() {
            std::string data = utils::HttpClient::get(url);
            if (!data.empty()) {
                brls::sync([this, data]() {
                    this->cover_image->setImageFromMem((const unsigned char*)data.data(), data.size());
                });
            }
        });
    }
    this->addView(this->cover_image);

    // 2. Title Label
    this->title_label = new brls::Label();
    this->title_label->setText(item.title);
    this->title_label->setFontSize(16);
    this->title_label->setMarginTop(6);
    this->title_label->setMarginLeft(6);
    this->title_label->setMarginRight(6);
    this->title_label->setTextColor(brls::Application::getTheme()["brls/text"]);
    this->addView(this->title_label);

    // 3. Subtitle: Format & Size (e.g. NSP • 4.2 GB)
    this->subtitle_label = new brls::Label();
    std::string sub = item.format + " • " + item.size;
    this->subtitle_label->setText(sub);
    this->subtitle_label->setFontSize(12);
    this->subtitle_label->setMarginTop(2);
    this->subtitle_label->setMarginLeft(6);
    this->subtitle_label->setMarginRight(6);
    this->subtitle_label->setTextColor(brls::Application::getTheme()["brls/text_disabled"]);
    this->addView(this->subtitle_label);

    // Click action: Push GameDetailActivity
    this->registerClickAction([this](brls::View* view) {
        brls::Logger::info("Game clicked: {}, opening GameDetailActivity", this->item.title);
        brls::Application::pushActivity(new GameDetailActivity(this->item));
        return true;
    });
}

GameCard* GameCard::create(const api::GameItem& item) {
    return new GameCard(item);
}

} // namespace switchfs::view
