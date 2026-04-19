#include <catch2/catch_test_macros.hpp>
#include "buttons.hpp"

#include <vector>

using buttons::Button;
using buttons::Event;
using buttons::Config;
using buttons::ButtonHandler;

namespace {

class FakeHandler : public ButtonHandler {
public:
    explicit FakeHandler(const Config& cfg = Config{}) : ButtonHandler(cfg) {}

    void setLevel(size_t idx, uint8_t level) { levels_[idx] = level; }

    void tick() {
        for (size_t i = 0; i < static_cast<size_t>(Button::Count); ++i) {
            processButton(i);
        }
    }

    const std::vector<Event>& events(size_t idx) const { return events_[idx]; }

    void installRecorder() {
        for (size_t i = 0; i < static_cast<size_t>(Button::Count); ++i) {
            for (size_t e = 0; e < static_cast<size_t>(Event::Count); ++e) {
                onEvent(static_cast<Button>(i), static_cast<Event>(e),
                        &FakeHandler::record, this);
            }
        }
    }

protected:
    uint8_t readGpioLevel(size_t idx) const override { return levels_[idx]; }

private:
    static void record(Button b, Event e, void* ctx) {
        auto* self = static_cast<FakeHandler*>(ctx);
        self->events_[static_cast<size_t>(b)].push_back(e);
    }

    uint8_t levels_[static_cast<size_t>(Button::Count)] = {1, 1};
    std::vector<Event> events_[static_cast<size_t>(Button::Count)];
};

static constexpr size_t A = static_cast<size_t>(Button::A);

static bool contains(const std::vector<Event>& v, Event e) {
    for (auto x : v) if (x == e) return true;
    return false;
}

} // namespace

TEST_CASE("ButtonHandler: short tap emits PressDown, PressUp, SingleClick", "[buttons]") {
    Config cfg;
    FakeHandler h(cfg);
    h.installRecorder();

    // Press
    h.setLevel(A, 0);
    for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
    // Release
    h.setLevel(A, 1);
    for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
    // Wait past short-press timeout for SingleClick to emit
    for (uint16_t i = 0; i < cfg.shortPressTicks + 5; ++i) h.tick();

    const auto& ev = h.events(A);
    REQUIRE(contains(ev, Event::PressDown));
    REQUIRE(contains(ev, Event::PressUp));
    REQUIRE(contains(ev, Event::SingleClick));
}

TEST_CASE("ButtonHandler: long press emits PressDown + LongPressStart", "[buttons]") {
    Config cfg;
    FakeHandler h(cfg);
    h.installRecorder();

    h.setLevel(A, 0);
    for (uint16_t i = 0; i < cfg.longPressTicks + cfg.debounceTicks + 10; ++i) h.tick();

    const auto& ev = h.events(A);
    REQUIRE(contains(ev, Event::PressDown));
    REQUIRE(contains(ev, Event::LongPressStart));
}

TEST_CASE("ButtonHandler: bounce shorter than debounceTicks is filtered", "[buttons]") {
    Config cfg;
    FakeHandler h(cfg);
    h.installRecorder();

    // Oscillate faster than debounce can stabilize
    for (int i = 0; i < 2; ++i) {
        h.setLevel(A, 0); h.tick();
        h.setLevel(A, 1); h.tick();
    }
    REQUIRE(h.events(A).empty());

    // Actually press and hold through debounce
    h.setLevel(A, 0);
    for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
    REQUIRE(contains(h.events(A), Event::PressDown));
}

TEST_CASE("ButtonHandler: two taps within shortPressTicks emit DoubleClick", "[buttons]") {
    Config cfg;
    FakeHandler h(cfg);
    h.installRecorder();

    auto press_release = [&]() {
        h.setLevel(A, 0);
        for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
        h.setLevel(A, 1);
        for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
    };

    press_release();
    for (int i = 0; i < 5; ++i) h.tick();  // brief gap
    press_release();
    for (uint16_t i = 0; i < cfg.shortPressTicks + 5; ++i) h.tick();

    REQUIRE(contains(h.events(A), Event::DoubleClick));
}

TEST_CASE("ButtonHandler: two full press cycles produce two PressDown events", "[buttons]") {
    Config cfg;
    FakeHandler h(cfg);
    h.installRecorder();

    for (int cycle = 0; cycle < 2; ++cycle) {
        h.setLevel(A, 0);
        for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
        h.setLevel(A, 1);
        for (int i = 0; i < cfg.debounceTicks + 3; ++i) h.tick();
    }
    for (uint16_t i = 0; i < cfg.shortPressTicks + 5; ++i) h.tick();

    int down_count = 0;
    for (auto e : h.events(A)) if (e == Event::PressDown) ++down_count;
    REQUIRE(down_count == 2);
}
