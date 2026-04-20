#include <catch2/catch_test_macros.hpp>
#include "text_input.hpp"
#include <cstring>

using app::TextInput;
using app::TextInputOpts;
using app::TextInputResult;

static TextInputResult feed(TextInput& ti, const char* s) {
    TextInputResult r = TextInputResult::None;
    for (size_t i = 0; s[i]; ++i) {
        uint8_t b = static_cast<uint8_t>(s[i]);
        r = ti.handleKey(&b, 1);
    }
    return r;
}

TEST_CASE("TextInput: append prints chars, length tracks", "[app][text]") {
    char buf[16];
    TextInput ti(buf, sizeof(buf));
    feed(ti, "h"); feed(ti, "i");
    REQUIRE(ti.length() == 2);
    REQUIRE(std::strcmp(ti.data(), "hi") == 0);
}

TEST_CASE("TextInput: backspace deletes last char", "[app][text]") {
    char buf[16];
    TextInput ti(buf, sizeof(buf));
    feed(ti, "abc");
    uint8_t bs = 0x08;
    ti.handleKey(&bs, 1);
    REQUIRE(std::strcmp(ti.data(), "ab") == 0);
}

TEST_CASE("TextInput: Enter returns Submit, Esc returns Cancel",
          "[app][text]") {
    char buf[4];
    TextInput ti(buf, sizeof(buf));
    REQUIRE(feed(ti, "\r") == TextInputResult::Submit);
    REQUIRE(feed(ti, "\x1B") == TextInputResult::Cancel);
}

TEST_CASE("TextInput: capacity silently rejects overflow",
          "[app][text][limits]") {
    char buf[4];
    TextInput ti(buf, sizeof(buf));
    feed(ti, "abcdef");
    REQUIRE(ti.length() == 3);
    REQUIRE(std::strcmp(ti.data(), "abc") == 0);
}

TEST_CASE("TextInput: numeric flag rejects non-digits",
          "[app][text][numeric]") {
    char buf[8];
    TextInput ti(buf, sizeof(buf), TextInputOpts{.numeric = true});
    feed(ti, "1a2b3c");
    REQUIRE(std::strcmp(ti.data(), "123") == 0);
}

TEST_CASE("TextInput: masked + tab_toggles_reveal toggles on Tab",
          "[app][text][mask]") {
    char buf[8];
    TextInput ti(buf, sizeof(buf),
                 TextInputOpts{.masked = true, .tab_toggles_reveal = true});
    feed(ti, "pw");
    uint8_t tab = '\t';
    REQUIRE(ti.handleKey(&tab, 1) == TextInputResult::None);
    REQUIRE(std::strcmp(ti.data(), "pw") == 0);
}

TEST_CASE("TextInput: tab_toggles_reveal=false passes tab through (buffer unchanged)",
          "[app][text][mask]") {
    char buf[8];
    TextInput ti(buf, sizeof(buf),
                 TextInputOpts{.masked = true, .tab_toggles_reveal = false});
    uint8_t tab = '\t';
    REQUIRE(ti.handleKey(&tab, 1) == TextInputResult::None);
    REQUIRE(ti.length() == 0);
}

TEST_CASE("TextInput: Ctrl+R on masked is a no-op to buffer",
          "[app][text][mask]") {
    char buf[8];
    TextInput ti(buf, sizeof(buf), TextInputOpts{.masked = true});
    feed(ti, "hi");
    uint8_t ctrl_r = 0x12;
    REQUIRE(ti.handleKey(&ctrl_r, 1) == TextInputResult::None);
    REQUIRE(std::strcmp(ti.data(), "hi") == 0);
}

TEST_CASE("TextInput: DEL (0x7F) is treated as backspace",
          "[app][text]") {
    char buf[16];
    TextInput ti(buf, sizeof(buf));
    feed(ti, "abc");
    uint8_t del = 0x7F;
    ti.handleKey(&del, 1);
    REQUIRE(std::strcmp(ti.data(), "ab") == 0);
    REQUIRE(ti.length() == 2);
}

TEST_CASE("TextInput: multi-byte escape sequences are silently ignored",
          "[app][text]") {
    char buf[16];
    TextInput ti(buf, sizeof(buf));
    feed(ti, "hi");
    // ESC [ A (Up arrow) — 3 bytes. Must NOT corrupt buffer (bug fixed in eade6af).
    const uint8_t up_arrow[] = {0x1B, 0x5B, 0x41};
    REQUIRE(ti.handleKey(up_arrow, 3) == TextInputResult::None);
    REQUIRE(ti.length() == 2);
    REQUIRE(std::strcmp(ti.data(), "hi") == 0);

    // ESC O P (F1) — 3 bytes.
    const uint8_t f1[] = {0x1B, 0x4F, 0x50};
    REQUIRE(ti.handleKey(f1, 3) == TextInputResult::None);
    REQUIRE(ti.length() == 2);
    REQUIRE(std::strcmp(ti.data(), "hi") == 0);
}
