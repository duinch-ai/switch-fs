#pragma once

#include <borealis.hpp>
#include "data/settings_manager.hpp"

namespace switchfs::view {

class SettingsTab : public brls::Box {
public:
    SettingsTab();

    static brls::View* create();

private:
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* content_box = nullptr;
};

} // namespace switchfs::view
