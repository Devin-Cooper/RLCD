#include <catch2/catch_test_macros.hpp>
#include "command_registry.hpp"
#include "command_ids.hpp"

using app::CommandRegistry;
using app::kAllDispatchableIds;

TEST_CASE("CommandRegistry: registerGlobals populates expected count",
          "[app][cmd]") {
    auto& reg = CommandRegistry::instance();
    reg.registerGlobals();
    REQUIRE(reg.globals().size() == 13);
}

TEST_CASE("kAllDispatchableIds covers every global cmd id", "[app][cmd]") {
    auto& reg = CommandRegistry::instance();
    reg.registerGlobals();
    for (const auto& c : reg.globals()) {
        bool found = false;
        for (auto id : kAllDispatchableIds) {
            if (id == c.id) { found = true; break; }
        }
        REQUIRE(found);
    }
}

TEST_CASE("CommandRegistry: registerGlobals is idempotent", "[app][cmd]") {
    auto& reg = CommandRegistry::instance();
    reg.registerGlobals();
    auto first_count = reg.globals().size();
    reg.registerGlobals();
    REQUIRE(reg.globals().size() == first_count);
}
