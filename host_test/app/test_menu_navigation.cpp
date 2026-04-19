#include <catch2/catch_test_macros.hpp>
#include "menu.hpp"

using app::Menu;

TEST_CASE("Menu: initial state closed, selection 0", "[app][menu]") {
    Menu m;
    REQUIRE_FALSE(m.isOpen());
    REQUIRE(m.selected() == Menu::Item::Dashboard);
}

TEST_CASE("Menu: open sets visible and resets to index 0", "[app][menu]") {
    Menu m;
    m.moveDown(); m.moveDown(); m.moveDown();
    m.open();
    REQUIRE(m.isOpen());
    REQUIRE(m.selected() == Menu::Item::Dashboard);  // reset
}

TEST_CASE("Menu: close hides overlay; index preserved", "[app][menu]") {
    Menu m;
    m.open();
    m.moveDown();
    m.close();
    REQUIRE_FALSE(m.isOpen());
    REQUIRE(m.selected() == Menu::Item::Terminal);
}

// CHARACTERIZATION (pre-Spec-03): moveDown clamps at last item, does NOT wrap.
// When Spec 03 lands and flips moveDown to wrap, these assertions flip to
// expect Menu::Item::Dashboard after sufficient moveDown calls.
TEST_CASE("Menu: moveDown clamps at last item (pre-Spec-03)", "[app][menu][pre-spec-03]") {
    Menu m;
    m.open();
    for (int i = 0; i < 100; ++i) m.moveDown();
    REQUIRE(m.selected() == Menu::Item::About);
}

TEST_CASE("Menu: moveUp clamps at first item (pre-Spec-03)", "[app][menu][pre-spec-03]") {
    Menu m;
    m.open();
    for (int i = 0; i < 100; ++i) m.moveUp();
    REQUIRE(m.selected() == Menu::Item::Dashboard);
}

TEST_CASE("Menu: confirm returns currently-selected item", "[app][menu]") {
    Menu m;
    m.open();
    REQUIRE(m.confirm() == Menu::Item::Dashboard);
    m.moveDown();
    REQUIRE(m.confirm() == Menu::Item::Terminal);
    m.moveDown(); m.moveDown();
    REQUIRE(m.confirm() == Menu::Item::Settings);
}

TEST_CASE("Menu: moveDown to last item is monotonic", "[app][menu]") {
    Menu m;
    m.open();
    const int count = static_cast<int>(Menu::Item::Count);
    for (int i = 0; i < count - 1; ++i) m.moveDown();
    REQUIRE(m.selected() == Menu::Item::About);
}
