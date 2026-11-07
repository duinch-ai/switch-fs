#pragma once

#include <borealis.hpp>

namespace switchfs::view {

enum class LibrarySection {
    FAVORITES = 0,
    DOWNLOADED = 1,
    INSTALLED = 2
};

class LibraryTab : public brls::Box {
public:
    LibraryTab();

    static brls::View* create();

    void showSection(LibrarySection section);
    void willAppear(bool resetState = false) override;

private:
    LibrarySection active_section = LibrarySection::FAVORITES;

    brls::Button* fav_btn = nullptr;
    brls::Button* dl_btn = nullptr;
    brls::Button* inst_btn = nullptr;
    brls::Button* refresh_btn = nullptr;
    brls::Label* status_label = nullptr;
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* content_box = nullptr;
};

} // namespace switchfs::view
