#include <catch2/catch_test_macros.hpp>
#include "dashboard_card_set.hpp"
#include <array>
#include <vector>

using namespace app;

namespace {

struct FakeCommandSource {
    struct Cmd { const char* label; const char* output; };
    std::vector<Cmd> cmds;
    int commandCount() const { return (int)cmds.size(); }
    const char* labelAt(int i) const { return cmds[i].label; }
};

} // namespace

TEST_CASE("CardSet maps known labels to MetricRef (case-insensitive)", "[app][dashboard]") {
    FakeCommandSource src;
    src.cmds = {
        {"CPU",      "0.10 0.16 0.11"},
        {"memory",   "3.8Gi/30Gi"},
        {"Disk",     "956G/3.6T (28%)"},
        {"uptime",   "up 1 day"},
        {"Temp",     "+58C"},
        {"screens",  "2"},
    };
    std::array<DashboardCard, 9> cards;
    int n = buildDashboardCardSet(src, cards.data(), (int)cards.size());

    REQUIRE(n == 7);  // 6 headline + 1 Trends
    CHECK(cards[0].source == MetricRef::Cpu);
    CHECK(cards[1].source == MetricRef::Memory);
    CHECK(cards[2].source == MetricRef::Disk);
    CHECK(cards[3].source == MetricRef::Uptime);
    CHECK(cards[4].source == MetricRef::Temp);
    CHECK(cards[5].source == MetricRef::Screens);
    CHECK(cards[6].layout == CardLayout::Trends);
    CHECK(cards[6].source == MetricRef::None);
    CHECK(cards[6].command_index == -1);
}

TEST_CASE("CardSet maps unknown labels to Custom + BlueNoise", "[app][dashboard]") {
    FakeCommandSource src;
    src.cmds = { {"FooBar", "x"} };
    std::array<DashboardCard, 9> cards;
    int n = buildDashboardCardSet(src, cards.data(), (int)cards.size());

    REQUIRE(n == 2);
    CHECK(cards[0].source == MetricRef::Custom);
    CHECK(cards[0].command_index == 0);
    // BlueNoise is the documented signature for Custom.
    CHECK(cards[0].signature.kind == onebit::PatternKind::BlueNoise);
}

TEST_CASE("CardSet truncates to capacity (8 commands + Trends = max 9)", "[app][dashboard]") {
    FakeCommandSource src;
    for (int i = 0; i < 12; ++i) src.cmds.push_back({"X", "y"});
    std::array<DashboardCard, 9> cards;
    int n = buildDashboardCardSet(src, cards.data(), (int)cards.size());

    REQUIRE(n == 9);   // truncated
    CHECK(cards[8].layout == CardLayout::Trends);
}

TEST_CASE("CardSet with zero commands still appends a Trends card", "[app][dashboard]") {
    FakeCommandSource src;
    std::array<DashboardCard, 9> cards;
    int n = buildDashboardCardSet(src, cards.data(), (int)cards.size());
    REQUIRE(n == 1);
    CHECK(cards[0].layout == CardLayout::Trends);
}
