#pragma once

#include "api/nswgf.hpp"
#include <borealis.hpp>
#include <string>
#include <vector>

namespace switchfs::view {

class SearchTab : public brls::Box {
public:
    SearchTab();

    static brls::View* create();

    void performSearch(const std::string& keyword, int page = 1);

private:
    std::string current_keyword;
    int current_page = 1;
    bool has_more = true;
    bool is_searching = false;

    brls::Button* search_btn = nullptr;
    brls::Label* status_label = nullptr;
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* content_box = nullptr;
    brls::Box* grid_container = nullptr;
};

} // namespace switchfs::view
