#include <catch2/catch_test_macros.hpp>

#include "ssh_key_export.hpp"

using namespace ssh_keys;

TEST_CASE("select_qr_scale clamps to [0, 6]", "[ssh_keys_qr]") {
    // V6 = 41 modules. 300/(41+8) = 6.12 → 6.
    REQUIRE(select_qr_scale(41) == 6);
    // V9 = 53. 300/(53+8) = 4.92 → 4.
    REQUIRE(select_qr_scale(53) == 4);
    // V13 = 69. 300/(69+8) = 3.89 → 3.
    REQUIRE(select_qr_scale(69) == 3);
    // V17 = 85. 300/(85+8) = 3.22 → 3.
    REQUIRE(select_qr_scale(85) == 3);
    // V25 = 117. 300/(117+8) = 2.4 → 2.
    REQUIRE(select_qr_scale(117) == 2);
    // V40 = 177. 300/(177+8) = 1.62 → 1.
    REQUIRE(select_qr_scale(177) == 1);
    // Degenerate: modules too large (would need scale 0)
    REQUIRE(select_qr_scale(400) == 0);
    // Small: V1 = 21 modules. 300/(21+8) = 10.3 → clamp to 6.
    REQUIRE(select_qr_scale(21) == 6);
}

TEST_CASE("select_qr_scale handles zero/negative gracefully", "[ssh_keys_qr]") {
    REQUIRE(select_qr_scale(0) == 0);  // invalid, but non-crashing
}
