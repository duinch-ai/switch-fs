#pragma once

#include "api/romm.hpp"
#include <borealis.hpp>
#include <string>

namespace switchfs::view {

class RommTab : public brls::Box {
public:
    RommTab();

    static brls::View* create();

    void loadRoms(int page = 1, const std::string& keyword = "");

private:
    int current_page = 1;
    std::string current_keyword;
    bool has_more = true;
    bool is_loading = false;

    brls::Button* url_btn = nullptr;
    brls::Button* auth_btn = nullptr;
    brls::Button* search_btn = nullptr;
    brls::Button* sync_btn = nullptr;
    brls::Label* status_label = nullptr;
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* content_box = nullptr;
    brls::Box* grid_container = nullptr;
};

} // namespace switchfs::view
