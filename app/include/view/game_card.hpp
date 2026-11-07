#pragma once

#include <borealis.hpp>
#include "api/nswgf.hpp"

namespace switchfs::view {

class GridRow : public brls::Box {
public:
    GridRow();

    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;
};

class GameCard : public brls::Box {
public:
    explicit GameCard(const api::GameItem& item);

    static GameCard* create(const api::GameItem& item);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    const api::GameItem& getGameItem() const { return item; }

private:
    api::GameItem item;
    brls::Image* cover_image = nullptr;
    brls::Label* title_label = nullptr;
    brls::Label* subtitle_label = nullptr;
};

} // namespace switchfs::view
