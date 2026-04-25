#include <catch2/catch_test_macros.hpp>
#include "screen_stack.hpp"

using app::Screen;
using app::ScreenStack;

namespace {

struct StubScreen : Screen {
    int* enter_count;
    int* exit_count;
    int* render_count;
    bool transparent;

    StubScreen(int* e, int* x, int* r, bool t = false)
        : enter_count(e), exit_count(x), render_count(r), transparent(t) {}

    void onEnter() override { if (enter_count) ++*enter_count; }
    void onExit()  override { if (exit_count)  ++*exit_count; }
    void handleInput(const input::InputEvent&, ScreenStack&) override {}
    void render(onebit::IFramebuffer&, const onebit::BitmapFont&) override {
        if (render_count) ++*render_count;
    }
    bool isTransparent() const override { return transparent; }
};

} // namespace

TEST_CASE("ScreenStack: push runs onEnter immediately", "[app][stack]") {
    ScreenStack s;
    int e = 0, x = 0, r = 0;
    s.push(std::make_unique<StubScreen>(&e, &x, &r));
    REQUIRE(e == 1);
    REQUIRE(x == 0);
    REQUIRE(s.depth() == 1);
}

TEST_CASE("ScreenStack: pop is deferred, applyPending runs onExit",
          "[app][stack]") {
    ScreenStack s;
    int e1=0, x1=0, r1=0, e2=0, x2=0, r2=0;
    s.push(std::make_unique<StubScreen>(&e1,&x1,&r1));
    s.push(std::make_unique<StubScreen>(&e2,&x2,&r2));
    s.pop();
    REQUIRE(x2 == 0);
    REQUIRE(s.depth() == 2);
    s.applyPending();
    REQUIRE(x2 == 1);
    REQUIRE(s.depth() == 1);
}

TEST_CASE("ScreenStack: pop refuses when depth == 1", "[app][stack]") {
    ScreenStack s;
    int e=0, x=0, r=0;
    s.push(std::make_unique<StubScreen>(&e,&x,&r));
    s.pop();
    s.applyPending();
    REQUIRE(s.depth() == 1);
    REQUIRE(x == 0);
}

TEST_CASE("ScreenStack: replace pops-then-pushes atomically",
          "[app][stack]") {
    ScreenStack s;
    int e1=0, x1=0, e2=0, x2=0;
    s.push(std::make_unique<StubScreen>(&e1,&x1,nullptr));
    s.replace(std::make_unique<StubScreen>(&e2,&x2,nullptr));
    REQUIRE(x1 == 0); REQUIRE(e2 == 0);
    s.applyPending();
    REQUIRE(x1 == 1); REQUIRE(e2 == 1);
    REQUIRE(s.depth() == 1);
}

TEST_CASE("ScreenStack: renderAll starts at lowest opaque",
          "[app][stack][render]") {
    ScreenStack s;
    int r_base=0, r_mid=0, r_top=0;
    s.push(std::make_unique<StubScreen>(nullptr,nullptr,&r_base,false));
    s.push(std::make_unique<StubScreen>(nullptr,nullptr,&r_mid,false));
    s.push(std::make_unique<StubScreen>(nullptr,nullptr,&r_top,true));

    onebit::Framebuffer<400,300> fb;
    onebit::BitmapFont font{};

    s.renderAll(fb, font);
    REQUIRE(r_base == 0);
    REQUIRE(r_mid == 1);
    REQUIRE(r_top == 1);
}

TEST_CASE("ScreenStack: clearToBaseAndPush pops to depth 1 then pushes",
          "[app][stack]") {
    ScreenStack s;
    int x1=0, x2=0, x3=0, e4=0;
    s.push(std::make_unique<StubScreen>(nullptr,&x1,nullptr));
    s.push(std::make_unique<StubScreen>(nullptr,&x2,nullptr));
    s.push(std::make_unique<StubScreen>(nullptr,&x3,nullptr));
    s.clearToBaseAndPush(std::make_unique<StubScreen>(&e4,nullptr,nullptr));
    s.applyPending();
    REQUIRE(s.depth() == 2);
    REQUIRE(x2 == 1);
    REQUIRE(x3 == 1);
    REQUIRE(x1 == 0);
    REQUIRE(e4 == 1);
}

TEST_CASE("ScreenStack: pushBypassingGate skips policy", "[app][stack][gate]") {
    ScreenStack s;
    int e = 0, x = 0, r = 0;
    bool policy_called = false;
    s.setGatePolicy([&](Screen&, std::unique_ptr<Screen>&){
        policy_called = true;
        return true;
    });
    s.pushBypassingGate(ScreenStack::BypassToken::forTest(),
                        std::make_unique<StubScreen>(&e,&x,&r));
    REQUIRE_FALSE(policy_called);
    REQUIRE(s.depth() == 1);
}

TEST_CASE("ScreenStack: push consults gate policy", "[app][stack][gate]") {
    ScreenStack s;
    int e = 0, x = 0, r = 0;
    s.push(std::make_unique<StubScreen>(&e,&x,&r));  // base, no policy yet
    int policy_calls = 0;
    s.setGatePolicy([&](Screen&, std::unique_ptr<Screen>&){
        ++policy_calls;
        return false;  // decline
    });
    int e2=0, x2=0, r2=0;
    s.push(std::make_unique<StubScreen>(&e2,&x2,&r2));
    REQUIRE(policy_calls == 1);
    REQUIRE(s.depth() == 2);
    REQUIRE(e2 == 1);
}

TEST_CASE("ScreenStack: gate intercept consumes candidate", "[app][stack][gate]") {
    ScreenStack s;
    int e = 0;
    s.push(std::make_unique<StubScreen>(&e,nullptr,nullptr));
    s.setGatePolicy([&](Screen&, std::unique_ptr<Screen>& deferred){
        deferred.reset();   // pretend KeyboardGateModal swallows it
        return true;
    });
    int e2 = 0;
    s.push(std::make_unique<StubScreen>(&e2,nullptr,nullptr));
    REQUIRE(s.depth() == 1);
    REQUIRE(e2 == 0);   // candidate was reset before onEnter
}

TEST_CASE("ScreenStack: replaceBypassingGate skips policy", "[app][stack][gate]") {
    ScreenStack s;
    int e1=0, x1=0, e2=0, x2=0;
    s.push(std::make_unique<StubScreen>(&e1,&x1,nullptr));
    bool policy_called = false;
    s.setGatePolicy([&](Screen&, std::unique_ptr<Screen>&){
        policy_called = true;
        return true;
    });
    s.replaceBypassingGate(ScreenStack::BypassToken::forTest(),
                           std::make_unique<StubScreen>(&e2,&x2,nullptr));
    REQUIRE_FALSE(policy_called);
    s.applyPending();
    REQUIRE(x1 == 1); REQUIRE(e2 == 1);
    REQUIRE(s.depth() == 1);
}
