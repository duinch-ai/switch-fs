#pragma once

#include <borealis.hpp>
#include "api/nswgf.hpp"

namespace switchfs::view {

class HomeTab : public brls::Box {
public:
    HomeTab();

    static brls::View* create();

    void loadGames(int page = 1);

private:
    int current_page = 1;
    bool is_loading = false;
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* content_box = nullptr;
    brls::Box* grid_container = nullptr;
    brls::Label* status_label = nullptr;
};

} // namespace switchfs::view
