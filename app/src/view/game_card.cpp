#include "view/game_card.hpp"
#include "view/game_detail_activity.hpp"
#include "utils/http.hpp"
#include <borealis/core/touch/tap_gesture.hpp>
#include <algorithm>

namespace switchfs::view {

GridRow::GridRow() : brls::Box(brls::Axis::ROW) {
}

brls::View* GridRow::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    size_t col = 0;
    bool found = false;
    auto& children = this->getChildren();
    for (size_t i = 0; i < children.size(); ++i) {
        if (children[i] == currentView) {
            col = i;
            found = true;
            break;
        }
    }
    if (!found) {
        return brls::Box::getNextFocus(direction, currentView);
    }

    if (direction == brls::FocusDirection::DOWN || direction == brls::FocusDirection::UP) {
        if (this->hasParent()) {
            auto& siblings = this->getParent()->getChildren();
            for (size_t r = 0; r < siblings.size(); ++r) {
                if (siblings[r] == this) {
                    if (direction == brls::FocusDirection::DOWN && r + 1 < siblings.size()) {
                        auto* next_box = dynamic_cast<brls::Box*>(siblings[r + 1]);
                        if (next_box && !next_box->getChildren().empty()) {
                            size_t target_col = std::min(col, next_box->getChildren().size() - 1);
                            if (auto* target = next_box->getChildren()[target_col]->getDefaultFocus()) {
                                return target;
                            }
                        }
                    } else if (direction == brls::FocusDirection::UP && r > 0) {
                        auto* prev_box = dynamic_cast<brls::Box*>(siblings[r - 1]);
                        if (prev_box && !prev_box->getChildren().empty()) {
                            size_t target_col = std::min(col, prev_box->getChildren().size() - 1);
                            if (auto* target = prev_box->getChildren()[target_col]->getDefaultFocus()) {
                                return target;
                            }
                        }
                    }
                    break;
                }
            }
            return this->getParent()->getNextFocus(direction, this);
        }
    } else if (direction == brls::FocusDirection::RIGHT) {
        if (col + 1 < children.size()) {
            return children[col + 1]->getDefaultFocus();
        } else if (this->hasParent()) {
            auto& siblings = this->getParent()->getChildren();
            for (size_t r = 0; r + 1 < siblings.size(); ++r) {
                if (siblings[r] == this) {
                    auto* next_box = dynamic_cast<brls::Box*>(siblings[r + 1]);
                    if (next_box && !next_box->getChildren().empty()) {
                        return next_box->getChildren()[0]->getDefaultFocus();
                    }
                    break;
                }
            }
        }
    } else if (direction == brls::FocusDirection::LEFT) {
        if (col > 0) {
            return children[col - 1]->getDefaultFocus();
        }
    }

    return brls::Box::getNextFocus(direction, currentView);
}

GameCard::GameCard(const api::GameItem& item) : item(item) {
    this->setFocusable(true);
    this->setAxis(brls::Axis::COLUMN);
    this->setWidth(196);
    this->setHeight(254);
    this->setMarginLeft(8);
    this->setMarginRight(8);
    this->setMarginTop(8);
    this->setMarginBottom(8);
    this->setCornerRadius(8);
    this->setShadowType(brls::ShadowType::GENERIC);

    // 1. Cover Image (Compact thumbnail)
    this->cover_image = new brls::Image();
    this->cover_image->setWidth(196);
    this->cover_image->setHeight(196);
    this->cover_image->setCornerRadius(8);
    this->cover_image->setScalingType(brls::ImageScalingType::FIT);
    this->cover_image->setImageFromRes("img/borealis_256.png");

    if (!item.cover_url.empty()) {
        std::string url = item.cover_url;
        ASYNC_RETAIN
        brls::async([ASYNC_TOKEN, url]() {
            if (utils::HttpClient::isAborting()) {
                brls::sync([ASYNC_TOKEN]() { ASYNC_RELEASE });
                return;
            }
            std::string data = utils::HttpClient::get(url, 6);
            brls::sync([ASYNC_TOKEN, data]() {
                ASYNC_RELEASE
                if (!data.empty() && !utils::HttpClient::isAborting()) {
                    this->cover_image->clear();
                    this->cover_image->setFreeTexture(true);
                    this->cover_image->setImageFromMem((const unsigned char*)data.data(), data.size());
                }
            });
        });
    }
    this->addView(this->cover_image);

    // 2. Title Label (Single-line compact)
    this->title_label = new brls::Label();
    std::string short_title = item.title.length() > 22 ? (item.title.substr(0, 20) + "..") : item.title;
    this->title_label->setText(short_title);
    this->title_label->setFontSize(14);
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

    // Click & Tap action: Push GameDetailActivity
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
    this->registerClickAction([this](brls::View* view) {
        brls::Logger::info("Game clicked: {}, opening GameDetailActivity", this->item.title);
        brls::Application::pushActivity(new GameDetailActivity(this->item));
        return true;
    });
}

void GameCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    brls::Box::draw(vg, x, y, width, height, style, ctx);
    if (this->isFocused()) {
        nvgBeginPath(vg);
        nvgRoundedRect(vg, x - 3.0f, y - 3.0f, width + 6.0f, height + 6.0f, 10.0f);
        nvgStrokeColor(vg, ctx->theme["brls/highlight/color1"]);
        nvgStrokeWidth(vg, 4.0f);
        nvgStroke(vg);
    }
}

GameCard* GameCard::create(const api::GameItem& item) {
    return new GameCard(item);
}

} // namespace switchfs::view
