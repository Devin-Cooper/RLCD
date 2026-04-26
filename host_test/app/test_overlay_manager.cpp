#include <catch2/catch_test_macros.hpp>
#include "overlay.hpp"
#include "animator.hpp"
#include <cstring>

using app::OverlayManager;
using app::Animator;
using app::makeTag;
using app::TweenKind;

TEST_CASE("Overlay: toast enqueue up to 3; 4th is dropped",
          "[app][overlay][toast]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    REQUIRE(o.showToast("a", 1000));
    REQUIRE(o.showToast("b", 1000));
    REQUIRE(o.showToast("c", 1000));
    REQUIRE_FALSE(o.showToast("d", 1000));
    REQUIRE(o.activeToastCount() == 3);
    REQUIRE(o.droppedToastCount() == 1);
}

TEST_CASE("Overlay: toast dedup suppresses identical msg in 500ms",
          "[app][overlay][toast]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    REQUIRE(o.showToast("same"));
    REQUIRE_FALSE(o.showToast("same"));
    REQUIRE(o.droppedToastCount() == 1);
    o.tick(600 * 1000);
    REQUIRE(o.showToast("same"));
}

TEST_CASE("Overlay: tick expires toasts", "[app][overlay][toast]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    o.showToast("short", 1000);  // 1s lifetime (>= 2 * kToastSlideUs)
    REQUIRE(o.activeToastCount() == 1);
    o.tick(500 * 1000);          // mid-life: still visible
    REQUIRE(o.activeToastCount() == 1);
    // Past expiry + slide-out duration: toast finishes sliding out and is dropped.
    o.tick(1000 * 1000 + static_cast<int64_t>(app::kToastSlideUs) + 1000);
    REQUIRE(o.activeToastCount() == 0);
}

static input::InputEvent kbd(const char* bytes) {
    input::InputEvent e{};
    e.source = input::Source::Keyboard;
    e.type   = input::EventType::Keypress;
    e.data_length = static_cast<uint8_t>(std::strlen(bytes));
    for (int i = 0; i < e.data_length; ++i) e.data[i] = bytes[i];
    return e;
}

TEST_CASE("Overlay: Info modal consumes input, dismisses on any key",
          "[app][overlay][modal]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    o.showInfo("Title", "Body");
    REQUIRE(o.hasModal());
    REQUIRE(o.handleInput(kbd("x")));   // consumed
    // Modal stays active during scale-out tween; advance past it then tick.
    o.tick(static_cast<int64_t>(app::kModalScaleUs) + 1);
    REQUIRE_FALSE(o.hasModal());
}

TEST_CASE("Overlay: Confirm fires callback with Yes on Enter",
          "[app][overlay][modal]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    bool got = false, got_result = false;
    o.showConfirm("Delete?", "Sure?", [&](bool yes){ got = true; got_result = yes; });
    REQUIRE(o.handleInput(kbd("\r")));
    REQUIRE(got); REQUIRE(got_result);
}

TEST_CASE("Overlay: Confirm Esc = No", "[app][overlay][modal]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    bool got_result = true;
    o.showConfirm("Delete?", "Sure?", [&](bool yes){ got_result = yes; });
    REQUIRE(o.handleInput(kbd("\x1B")));
    REQUIRE_FALSE(got_result);
}

TEST_CASE("Overlay: Confirm arrow toggles selection", "[app][overlay][modal]") {
    Animator anim;
    OverlayManager o(anim);
    o.tick(0);
    bool got_result = true;
    o.showConfirm("Delete?", "Sure?", [&](bool yes){ got_result = yes; });
    input::InputEvent right{}; right.source = input::Source::Keyboard;
    right.type = input::EventType::Keypress; right.data_length = 3;
    right.data[0] = 0x1B; right.data[1] = '['; right.data[2] = 'C';
    o.handleInput(right);
    o.handleInput(kbd("\r"));
    REQUIRE_FALSE(got_result);
}

TEST_CASE("OverlayManager: modal triggers ModalScale tween",
          "[app][overlay][anim]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);
    om.showError("Oops", "It broke");
    auto tag = makeTag(TweenKind::ModalScale, 0);
    REQUIRE(anim.inProgress(tag, 1));
    REQUIRE(anim.value(tag, 0) == 0);
    REQUIRE(anim.value(tag, 140'000) == 100);
}

TEST_CASE("OverlayManager: toast triggers ToastSlide tween",
          "[app][overlay][anim]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(1000);
    REQUIRE(om.showToast("hello", 2500));
    auto tag = makeTag(TweenKind::ToastSlide, 0);
    REQUIRE(anim.inProgress(tag, 1000));
    int16_t mid = anim.value(tag, 91'000);
    REQUIRE(mid > 0);
    REQUIRE(mid < 16);
    REQUIRE(anim.value(tag, 181'000 + 1000) == 0);
}
