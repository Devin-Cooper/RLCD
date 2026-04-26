#include "time_service.hpp"

namespace time_service {

static const TzEntry kCatalog[] = {
    { "UTC",                  "UTC0" },
    { "America/Los_Angeles",  "PST8PDT,M3.2.0,M11.1.0" },
    { "America/Denver",       "MST7MDT,M3.2.0,M11.1.0" },
    { "America/Chicago",      "CST6CDT,M3.2.0,M11.1.0" },
    { "America/New_York",     "EST5EDT,M3.2.0,M11.1.0" },
    { "Europe/London",        "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Europe/Berlin",        "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Asia/Tokyo",           "JST-9" },
    { "Australia/Sydney",     "AEST-10AEDT,M10.1.0,M4.1.0/3" },
};

const TzEntry* TimeService::catalog(size_t* count) {
    if (count) *count = sizeof(kCatalog) / sizeof(kCatalog[0]);
    return kCatalog;
}

}  // namespace time_service
