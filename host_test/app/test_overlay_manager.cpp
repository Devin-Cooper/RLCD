#include <catch2/catch_test_macros.hpp>
#include "overlay.hpp"
#include "animator.hpp"
#include "screen.hpp"      // for app::KeybindHint
#include "span_view.hpp"
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

// ----------------------------------------------------------
// Phase 7: footer suppression smoke
// ----------------------------------------------------------
namespace {
struct NoFooterScreen : app::Screen {
    void handleInput(const input::InputEvent&, app::ScreenStack&) override {}
    void render(onebit::IFramebuffer&, const onebit::BitmapFont&) override {}
    bool wantsKeybindFooter() const override { return false; }
};

struct FooterScreen : app::Screen {
    static constexpr app::KeybindHint kHints[] = {
        {"Esc", "Back"},
        {"Tab", "Next"},
    };
    void handleInput(const input::InputEvent&, app::ScreenStack&) override {}
    void render(onebit::IFramebuffer&, const onebit::BitmapFont&) override {}
    app::SpanView<const app::KeybindHint> keybindHints() const override {
        return {kHints, sizeof(kHints) / sizeof(kHints[0])};
    }
};
constexpr app::KeybindHint FooterScreen::kHints[];
} // namespace

TEST_CASE("OverlayManager: renderFooter null-screen and opt-out are no-ops",
          "[app][overlay][footer]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);

    onebit::Framebuffer<400, 300> fb;
    onebit::BitmapFont font{};

    // null top — must early-return cleanly
    om.renderFooter(fb, font, nullptr, 1000);

    // Screen that opts out — must early-return cleanly
    NoFooterScreen no_footer;
    REQUIRE_FALSE(no_footer.wantsKeybindFooter());
    om.renderFooter(fb, font, &no_footer, 1000);
}

TEST_CASE("OverlayManager: renderFooter suppressed by modal/toast",
          "[app][overlay][footer]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);

    onebit::Framebuffer<400, 300> fb;
    onebit::BitmapFont font{};
    FooterScreen scr;

    // Baseline: no overlays, footer-wanting screen — no crash.
    om.renderFooter(fb, font, &scr, 1000);

    // Modal active — suppression path.
    om.showInfo("T", "B");
    REQUIRE(om.hasModal());
    om.renderFooter(fb, font, &scr, 1000);

    // Drain the modal so the toast suppression path is exercised in isolation.
    om.handleInput(kbd("x"));
    om.tick(static_cast<int64_t>(app::kModalScaleUs) + 1);
    REQUIRE_FALSE(om.hasModal());

    // Toast in flight — suppression path.
    REQUIRE(om.showToast("hi", 2500));
    REQUIRE(om.activeToastCount() == 1);
    om.renderFooter(fb, font, &scr, 1000);
}

// --- Help modal (Phase 9) ---

TEST_CASE("Overlay: showHelp drives ModalScale tween 0->100",
          "[app][overlay][help]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);

    app::KeybindHint hints[] = {
        {"Esc", "back"},
        {"Ctrl+/", "help"},
    };
    REQUIRE_FALSE(om.isHelpVisible());
    om.showHelpForTest("Dashboard",
                       app::SpanView<const app::KeybindHint>(hints, 2));
    REQUIRE(om.isHelpVisible());

    auto tag = makeTag(TweenKind::ModalScale, 0);
    REQUIRE(anim.value(tag, 0) == 0);
    REQUIRE(anim.value(tag, app::kModalScaleUs) == 100);
}

TEST_CASE("Overlay: showHelp transitions ScalingIn -> Visible -> Hidden",
          "[app][overlay][help]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);
    om.showHelpForTest("X", {});
    REQUIRE(om.isHelpVisible());

    // Past scale-in completion: still visible.
    om.tick(static_cast<int64_t>(app::kModalScaleUs) + 1);
    REQUIRE(om.isHelpVisible());

    om.hideHelp();
    REQUIRE(om.isHelpVisible());  // scale-out window — still drawing

    om.tick(2 * static_cast<int64_t>(app::kModalScaleUs) + 2);
    REQUIRE_FALSE(om.isHelpVisible());
}

TEST_CASE("Overlay: showHelp is no-op when a modal is active",
          "[app][overlay][help]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);
    om.showInfo("Title", "Body");
    REQUIRE(om.hasModal());
    om.showHelpForTest("X", {});
    REQUIRE_FALSE(om.isHelpVisible());
}

TEST_CASE("Overlay: any keypress dismisses the help modal",
          "[app][overlay][help]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);
    om.showHelpForTest("X", {});
    REQUIRE(om.isHelpVisible());
    REQUIRE(om.handleInput(kbd("x")));   // consumed by help layer
    om.tick(static_cast<int64_t>(app::kModalScaleUs) + 1);
    REQUIRE_FALSE(om.isHelpVisible());
}

TEST_CASE("Overlay: hideHelp before tick is idempotent",
          "[app][overlay][help]") {
    Animator anim;
    OverlayManager om(anim);
    om.tick(0);
    om.hideHelp();  // not visible — no-op, no crash
    REQUIRE_FALSE(om.isHelpVisible());

    om.showHelpForTest("X", {});
    om.hideHelp();
    om.hideHelp();  // double-call during scale-out — no-op
    om.tick(2 * static_cast<int64_t>(app::kModalScaleUs) + 2);
    REQUIRE_FALSE(om.isHelpVisible());
}
