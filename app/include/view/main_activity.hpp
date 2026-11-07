#pragma once

#include <borealis.hpp>

namespace switchfs::view {

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");
};

} // namespace switchfs::view
