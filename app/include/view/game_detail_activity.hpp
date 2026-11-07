#pragma once

#include "api/nswgf.hpp"
#include <borealis.hpp>

namespace switchfs::view {

class GameDetailActivity : public brls::Activity {
public:
    explicit GameDetailActivity(const api::GameItem& item);

private:
    api::GameItem item;
};

} // namespace switchfs::view
