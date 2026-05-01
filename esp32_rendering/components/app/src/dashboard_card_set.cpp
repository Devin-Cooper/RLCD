#include "dashboard_card_set.hpp"
#include "dashboard.hpp"

namespace app {

namespace {
struct DashboardAdapter {
    const Dashboard& d;
    int commandCount() const { return d.commandCount(); }
    const char* labelAt(int i) const { return d.commandAt(i).label; }
};
} // namespace

int buildDashboardCardSet(const Dashboard& dashboard, DashboardCard* out, int out_capacity) {
    DashboardAdapter adapter{dashboard};
    return buildDashboardCardSet(adapter, out, out_capacity);
}

} // namespace app
