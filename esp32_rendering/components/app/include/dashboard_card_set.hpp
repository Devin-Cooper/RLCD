#pragma once

#include "dashboard_cards.hpp"
#include <1bit/render/pattern.hpp>
#include <cstddef>
#include <cstring>
#include <strings.h>  // strcasecmp

namespace app {

namespace detail {

struct LabelMap {
    const char*      label;
    MetricRef        ref;
    onebit::Pattern  signature;
};

// Default mapping table. New entries: append before the sentinel.
// Adding a new metric type = one row here + (if rendering differs)
// a new MetricRef value + a case in renderHeadlineCard.
inline LabelMap kDefaultLabelMap[] = {
    {"CPU",      MetricRef::Cpu,     onebit::benDay(128, 6, 2)},
    {"Load",     MetricRef::Cpu,     onebit::benDay(128, 6, 2)},
    {"Memory",   MetricRef::Memory,  onebit::halftone(128, 8)},
    {"Mem",      MetricRef::Memory,  onebit::halftone(128, 8)},
    {"Disk",     MetricRef::Disk,    onebit::lineScreen(128, 45, 6)},
    {"Uptime",   MetricRef::Uptime,  onebit::stripes(2, 4, /*vertical=*/false)},
    {"Temp",     MetricRef::Temp,    onebit::crosshatch(45, -45, 8, 1)},
    {"GPU",      MetricRef::Temp,    onebit::crosshatch(45, -45, 8, 1)},
    {"Screens",  MetricRef::Screens, onebit::checker(6)},
};

inline LabelMap lookupLabel(const char* label) {
    for (const auto& m : kDefaultLabelMap) {
        if (strcasecmp(label, m.label) == 0) return m;
    }
    return {label, MetricRef::Custom, onebit::blueNoise(96)};
}

} // namespace detail

/// Generic source interface for the card-set builder. Allows host tests to
/// pass a fake without dragging in the full Dashboard class.
///
/// Required methods on Source:
///   int  commandCount() const;
///   const char* labelAt(int i) const;
template <typename Source>
int buildDashboardCardSet(const Source& src, DashboardCard* out, int out_capacity) {
    if (out_capacity <= 0) return 0;

    int n = 0;
    int count = src.commandCount();

    for (int i = 0; i < count && n < out_capacity - 1; ++i) {
        DashboardCard& c = out[n];
        c.layout = CardLayout::Headline;
        c.command_index = static_cast<int8_t>(i);

        const char* label = src.labelAt(i);
        detail::LabelMap m = detail::lookupLabel(label);
        c.source = m.ref;
        c.signature = m.signature;
        std::strncpy(c.label, label, sizeof(c.label) - 1);
        c.label[sizeof(c.label) - 1] = '\0';
        ++n;
    }

    // Trends card always last
    if (n < out_capacity) {
        DashboardCard& t = out[n];
        t.layout = CardLayout::Trends;
        t.source = MetricRef::None;
        t.command_index = -1;
        t.signature = onebit::lineScreen(128, 35, 2);
        std::strncpy(t.label, "Trends", sizeof(t.label) - 1);
        t.label[sizeof(t.label) - 1] = '\0';
        ++n;
    }

    return n;
}

/// Convenience overload: build into a fixed-size array. Returns count.
class Dashboard;
int buildDashboardCardSet(const Dashboard& dashboard, DashboardCard* out, int out_capacity);

} // namespace app
