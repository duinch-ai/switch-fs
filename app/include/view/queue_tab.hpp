#pragma once

#include "download/download_manager.hpp"
#include <borealis.hpp>

namespace switchfs::view {

class QueueTab : public brls::Box {
public:
    QueueTab();
    ~QueueTab() override;

    static brls::View* create();

    void refreshList();

private:
    brls::Label* status_label = nullptr;
    brls::ScrollingFrame* scrolling_frame = nullptr;
    brls::Box* tasks_container = nullptr;
    brls::Event<download::DownloadTask>::Subscription progress_sub;
    bool has_sub = false;
};

} // namespace switchfs::view
