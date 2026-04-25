#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <memory>
#include "screen.hpp"
#include "screen_stack.hpp"

using app::Screen;
using app::ScreenStack;
using app::buildBreadcrumb;

namespace {
struct LabeledScreen : Screen {
    const char* label_;
    explicit LabeledScreen(const char* l) : label_(l) {}
    void handleInput(const input::InputEvent&, ScreenStack&) override {}
    void render(onebit::IFramebuffer&, const onebit::BitmapFont&) override {}
    const char* breadcrumbLabel() const override { return label_; }
};
} // namespace

TEST_CASE("breadcrumb: bottom-up walk joined with ' > '", "[app][breadcrumb]") {
    ScreenStack s;
    s.push(std::make_unique<LabeledScreen>("Dashboard"));
    s.push(std::make_unique<LabeledScreen>("Menu"));
    s.push(std::make_unique<LabeledScreen>("Servers"));
    s.push(std::make_unique<LabeledScreen>("Edit"));
    char buf[128] = {};
    buildBreadcrumb(s, buf, sizeof(buf));
    REQUIRE(std::string(buf) == "Dashboard > Menu > Servers > Edit");
}

TEST_CASE("breadcrumb: ellipsis truncate when overflowing", "[app][breadcrumb]") {
    ScreenStack s;
    s.push(std::make_unique<LabeledScreen>("Dashboard"));
    s.push(std::make_unique<LabeledScreen>("Menu"));
    s.push(std::make_unique<LabeledScreen>("Servers"));
    s.push(std::make_unique<LabeledScreen>("Edit"));
    char buf[20] = {};
    buildBreadcrumb(s, buf, sizeof(buf));
    REQUIRE(std::string(buf).find("...") == 0);
    REQUIRE(std::string(buf).find("Edit") != std::string::npos);
}

TEST_CASE("breadcrumb: single screen renders just its label", "[app][breadcrumb]") {
    ScreenStack s;
    s.push(std::make_unique<LabeledScreen>("Terminal"));
    char buf[64] = {};
    buildBreadcrumb(s, buf, sizeof(buf));
    REQUIRE(std::string(buf) == "Terminal");
}

TEST_CASE("breadcrumb: empty stack produces empty string", "[app][breadcrumb]") {
    ScreenStack s;
    char buf[64] = {};
    buildBreadcrumb(s, buf, sizeof(buf));
    REQUIRE(std::string(buf).empty());
}
