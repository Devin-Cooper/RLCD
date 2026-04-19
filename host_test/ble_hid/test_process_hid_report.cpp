#include <catch2/catch_test_macros.hpp>
#include "hid_report_process.hpp"
#include "hid_translate.hpp"

#include <cstring>
#include <string>
#include <vector>

using ble_hid::KeyEvent;
using ble_hid::processHidReport;

namespace {

struct Collector {
    std::vector<std::string> events;
    static void emit(const KeyEvent& ev, void* ctx) {
        auto* self = static_cast<Collector*>(ctx);
        self->events.emplace_back(
            reinterpret_cast<const char*>(ev.bytes), ev.length);
    }
};

static void feed(uint8_t prev[6], Collector& c,
                 uint8_t mod, uint8_t k1 = 0, uint8_t k2 = 0, uint8_t k3 = 0,
                 uint8_t k4 = 0, uint8_t k5 = 0, uint8_t k6 = 0) {
    uint8_t r[8] = {mod, 0, k1, k2, k3, k4, k5, k6};
    processHidReport(r, sizeof(r), prev, &Collector::emit, &c);
}

} // namespace

TEST_CASE("processHidReport: empty report emits nothing", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0);
    REQUIRE(c.events.empty());
}

TEST_CASE("processHidReport: single key press emits once", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0, 0x04);
    REQUIRE(c.events.size() == 1);
    REQUIRE(c.events[0] == "a");
    feed(prev, c, 0, 0x04);
    REQUIRE(c.events.size() == 1);
}

TEST_CASE("processHidReport: release then re-press emits twice", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0, 0x04);
    feed(prev, c, 0);
    feed(prev, c, 0, 0x04);
    REQUIRE(c.events.size() == 2);
    REQUIRE(c.events[0] == "a");
    REQUIRE(c.events[1] == "a");
}

TEST_CASE("processHidReport: rollover adds one key at a time", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0, 0x04);
    feed(prev, c, 0, 0x04, 0x05);
    feed(prev, c, 0, 0x04, 0x05, 0x06);
    REQUIRE(c.events.size() == 3);
    REQUIRE(c.events[0] == "a");
    REQUIRE(c.events[1] == "b");
    REQUIRE(c.events[2] == "c");
}

TEST_CASE("processHidReport: modifier shift applies to new presses", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, ble_hid::MOD_LSHIFT, 0x04);
    REQUIRE(c.events.size() == 1);
    REQUIRE(c.events[0] == "A");
}

TEST_CASE("processHidReport: unmapped keys do not emit", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0, 0x39);
    REQUIRE(c.events.empty());
}

TEST_CASE("processHidReport: len < 8 is ignored", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    uint8_t short_report[4] = {0, 0, 0x04, 0};
    processHidReport(short_report, 4, prev, &Collector::emit, &c);
    REQUIRE(c.events.empty());
}

TEST_CASE("processHidReport: arrow key produces escape sequence", "[ble_hid][report]") {
    uint8_t prev[6] = {};
    Collector c;
    feed(prev, c, 0, 0x52);
    REQUIRE(c.events.size() == 1);
    REQUIRE(c.events[0] == "\x1b[A");
}
