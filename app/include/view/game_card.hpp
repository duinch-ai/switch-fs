#pragma once

#include <borealis.hpp>
#include "api/nswgf.hpp"

namespace switchfs::view {

class GameCard : public brls::Box {
public:
    explicit GameCard(const api::GameItem& item);

    static GameCard* create(const api::GameItem& item);

    const api::GameItem& getGameItem() const { return item; }

private:
    api::GameItem item;
    brls::Image* cover_image = nullptr;
    brls::Label* title_label = nullptr;
    brls::Label* subtitle_label = nullptr;
};

} // namespace switchfs::view
