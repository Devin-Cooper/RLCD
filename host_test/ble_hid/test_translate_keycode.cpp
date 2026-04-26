#include <catch2/catch_test_macros.hpp>
#include "hid_translate.hpp"

#include <string>

using ble_hid::KeyEvent;
using ble_hid::translateKeycode;
using ble_hid::MOD_LSHIFT;
using ble_hid::MOD_RSHIFT;
using ble_hid::MOD_LCTRL;
using ble_hid::MOD_RCTRL;
using ble_hid::MOD_LGUI;
using ble_hid::MOD_RGUI;

static std::string bytesOf(const KeyEvent& e) {
    return std::string(reinterpret_cast<const char*>(e.bytes), e.length);
}

TEST_CASE("translateKeycode: arrow keys", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x4F, 0)) == "\x1b[C");
    REQUIRE(bytesOf(translateKeycode(0x50, 0)) == "\x1b[D");
    REQUIRE(bytesOf(translateKeycode(0x51, 0)) == "\x1b[B");
    REQUIRE(bytesOf(translateKeycode(0x52, 0)) == "\x1b[A");
}

TEST_CASE("translateKeycode: navigation keys", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x49, 0)) == "\x1b[2~");
    REQUIRE(bytesOf(translateKeycode(0x4C, 0)) == "\x1b[3~");
    REQUIRE(bytesOf(translateKeycode(0x4A, 0)) == "\x1b[H");
    REQUIRE(bytesOf(translateKeycode(0x4D, 0)) == "\x1b[F");
    REQUIRE(bytesOf(translateKeycode(0x4B, 0)) == "\x1b[5~");
    REQUIRE(bytesOf(translateKeycode(0x4E, 0)) == "\x1b[6~");
}

TEST_CASE("translateKeycode: F1..F12", "[ble_hid][translate]") {
    // F1 (0x3A) is rebound to Ctrl+K (0x0B) as of Phase 15 of the
    // keyboard-first UX work — opens the global command palette.
    REQUIRE(bytesOf(translateKeycode(0x3A, 0)) == std::string("\x0b", 1));
    REQUIRE(bytesOf(translateKeycode(0x3B, 0)) == "\x1bOQ");
    REQUIRE(bytesOf(translateKeycode(0x3C, 0)) == "\x1bOR");
    REQUIRE(bytesOf(translateKeycode(0x3D, 0)) == "\x1bOS");
    REQUIRE(bytesOf(translateKeycode(0x3E, 0)) == "\x1b[15~");
    REQUIRE(bytesOf(translateKeycode(0x3F, 0)) == "\x1b[17~");
    REQUIRE(bytesOf(translateKeycode(0x40, 0)) == "\x1b[18~");
    REQUIRE(bytesOf(translateKeycode(0x41, 0)) == "\x1b[19~");
    REQUIRE(bytesOf(translateKeycode(0x42, 0)) == "\x1b[20~");
    REQUIRE(bytesOf(translateKeycode(0x43, 0)) == "\x1b[21~");
    REQUIRE(bytesOf(translateKeycode(0x44, 0)) == "\x1b[23~");
    REQUIRE(bytesOf(translateKeycode(0x45, 0)) == "\x1b[24~");
}

TEST_CASE("translateKeycode: letters unshifted lowercase", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x04, 0)) == "a");
    REQUIRE(bytesOf(translateKeycode(0x11, 0)) == "n");
    REQUIRE(bytesOf(translateKeycode(0x1D, 0)) == "z");
}

TEST_CASE("translateKeycode: letters shifted uppercase", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x04, MOD_LSHIFT)) == "A");
    REQUIRE(bytesOf(translateKeycode(0x1D, MOD_RSHIFT)) == "Z");
    REQUIRE(bytesOf(translateKeycode(0x11, MOD_LSHIFT | MOD_RSHIFT)) == "N");
}

TEST_CASE("translateKeycode: Ctrl+letter produces control code", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x04, MOD_LCTRL)) == std::string("\x01", 1));
    REQUIRE(bytesOf(translateKeycode(0x06, MOD_LCTRL)) == std::string("\x03", 1));
    REQUIRE(bytesOf(translateKeycode(0x16, MOD_RCTRL)) == std::string("\x13", 1));
    REQUIRE(bytesOf(translateKeycode(0x1D, MOD_LCTRL)) == std::string("\x1a", 1));
}

TEST_CASE("translateKeycode: digits + shifted punctuation", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x1E, 0)) == "1");
    REQUIRE(bytesOf(translateKeycode(0x27, 0)) == "0");
    REQUIRE(bytesOf(translateKeycode(0x1E, MOD_LSHIFT)) == "!");
    REQUIRE(bytesOf(translateKeycode(0x1F, MOD_LSHIFT)) == "@");
    REQUIRE(bytesOf(translateKeycode(0x23, MOD_LSHIFT)) == "^");
    REQUIRE(bytesOf(translateKeycode(0x27, MOD_LSHIFT)) == ")");
}

TEST_CASE("translateKeycode: control/whitespace keys", "[ble_hid][translate]") {
    REQUIRE(bytesOf(translateKeycode(0x28, 0)) == std::string("\r", 1));
    REQUIRE(bytesOf(translateKeycode(0x29, 0)) == std::string("\x1b", 1));
    REQUIRE(bytesOf(translateKeycode(0x2A, 0)) == std::string("\x7f", 1));
    REQUIRE(bytesOf(translateKeycode(0x2B, 0)) == std::string("\t", 1));
    REQUIRE(bytesOf(translateKeycode(0x2C, 0)) == " ");
    REQUIRE(bytesOf(translateKeycode(0x38, MOD_LSHIFT)) == "?");
}

TEST_CASE("translateKeycode: CMD+Space emits Ctrl+K", "[ble_hid][translate]") {
    // Phase 15: CMD/Win + Space rebound from F1 (ESC O P) to Ctrl+K (0x0B),
    // matching the new global command palette accelerator.
    REQUIRE(bytesOf(translateKeycode(0x2C, MOD_LGUI)) == std::string("\x0b", 1));
    REQUIRE(bytesOf(translateKeycode(0x2C, MOD_RGUI)) == std::string("\x0b", 1));
    // Space alone still emits a plain space.
    REQUIRE(bytesOf(translateKeycode(0x2C, 0)) == " ");
}

TEST_CASE("translateKeycode: unmapped keycode yields zero-length event", "[ble_hid][translate]") {
    REQUIRE(translateKeycode(0x39, 0).length == 0);
    REQUIRE(translateKeycode(0xFF, 0).length == 0);
}

TEST_CASE("translateKeycode: Shift+Up -> ESC [ 1 ; 2 A", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x52, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[1;2A", 6));
}

TEST_CASE("translateKeycode: Shift+Down -> ESC [ 1 ; 2 B", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x51, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[1;2B", 6));
}

TEST_CASE("translateKeycode: Shift+Home -> ESC [ 1 ; 2 H", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x4A, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[1;2H", 6));
}

TEST_CASE("translateKeycode: Shift+End -> ESC [ 1 ; 2 F", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x4D, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[1;2F", 6));
}

TEST_CASE("translateKeycode: Shift+PgUp -> ESC [ 5 ; 2 ~", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x4B, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[5;2~", 6));
}

TEST_CASE("translateKeycode: Shift+PgDn -> ESC [ 6 ; 2 ~", "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x4E, ble_hid::MOD_LSHIFT);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[6;2~", 6));
}

TEST_CASE("translateKeycode: bare Up still emits ESC [ A (regression)",
          "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x52, 0);
    REQUIRE(e.length == 3);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[A", 3));
}

TEST_CASE("translateKeycode: bare PgUp still emits ESC [ 5 ~ (regression)",
          "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x4B, 0);
    REQUIRE(e.length == 4);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[5~", 4));
}

TEST_CASE("translateKeycode: Ctrl+Shift+Up -> ESC [ 1 ; 2 A (Shift wins)",
          "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(
        0x52, ble_hid::MOD_LSHIFT | ble_hid::MOD_LCTRL);
    REQUIRE(e.length == 6);
    REQUIRE(std::string(reinterpret_cast<const char*>(e.bytes), e.length)
            == std::string("\x1B[1;2A", 6));
}

TEST_CASE("translateKeycode: shifted printable letter unchanged (regression)",
          "[ble_hid][translate]") {
    auto e = ble_hid::translateKeycode(0x04, ble_hid::MOD_LSHIFT);   // 'a'
    REQUIRE(e.length == 1);
    REQUIRE(e.bytes[0] == 'A');
}
