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

TEST_CASE("Menu: moveDown wraps from last to first", "[app][menu][wrap]") {
    Menu m;
    m.open();
    for (int i = 0; i < 5; ++i) m.moveDown();
    REQUIRE(m.selected() == Menu::Item::About);
    m.moveDown();
    REQUIRE(m.selected() == Menu::Item::Dashboard);
}

TEST_CASE("Menu: moveUp wraps from first to last", "[app][menu][wrap]") {
    Menu m;
    m.open();
    REQUIRE(m.selected() == Menu::Item::Dashboard);
    m.moveUp();
    REQUIRE(m.selected() == Menu::Item::About);
}

TEST_CASE("Menu: many moveDown cycles return to origin", "[app][menu][wrap]") {
    Menu m;
    m.open();
    const int count = static_cast<int>(Menu::Item::Count);
    for (int i = 0; i < count * 3; ++i) m.moveDown();
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
