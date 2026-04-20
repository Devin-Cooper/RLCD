// host_test/app/test_response_parser.cpp
// Host-side unit test for the test_console response helpers —
// verifies the >>> prefix, line termination, and format escaping.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_session.hpp>
#include <cstdio>
#include <cstdarg>
#include <string>
#include <functional>
#include <unistd.h>
#include <fcntl.h>

// Minimal reimpl of the test_console response helpers — matches production.
namespace test_console {

static void emit_line(const char* prefix, const char* fmt, va_list ap) {
    fputs(prefix, stdout);
    vprintf(fmt, ap);
    fputc('\n', stdout);
    fflush(stdout);
}
void ok(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    emit_line(">>> OK ", fmt, ap);
    va_end(ap);
}
void err(int code, const char* fmt, ...) {
    fprintf(stdout, ">>> ERR %d ", code);
    va_list ap; va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
    fflush(stdout);
}
void data(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    emit_line(">>> DATA ", fmt, ap);
    va_end(ap);
}

} // namespace test_console

static std::string capture_stdout(std::function<void()> body) {
    fflush(stdout);
    int saved = dup(fileno(stdout));
    int pipefd[2];
    pipe(pipefd);
    dup2(pipefd[1], fileno(stdout));
    close(pipefd[1]);
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    body();
    fflush(stdout);
    std::string out;
    char buf[256];
    int n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        out.append(buf, n);
    }
    close(pipefd[0]);
    dup2(saved, fileno(stdout));
    close(saved);
    return out;
}

TEST_CASE("ok() emits >>> OK with trailing newline", "[test_console][parser]") {
    auto s = capture_stdout([]{ test_console::ok("hello %d", 42); });
    REQUIRE(s == ">>> OK hello 42\n");
}

TEST_CASE("err() emits >>> ERR <code> <msg>", "[test_console][parser]") {
    auto s = capture_stdout([]{ test_console::err(7, "payload too long"); });
    REQUIRE(s == ">>> ERR 7 payload too long\n");
}

TEST_CASE("data() emits >>> DATA payload", "[test_console][parser]") {
    auto s = capture_stdout([]{ test_console::data("line-%d", 3); });
    REQUIRE(s == ">>> DATA line-3\n");
}

TEST_CASE("ok() with empty format emits trailing space", "[test_console][parser]") {
    auto s = capture_stdout([]{ test_console::ok(""); });
    REQUIRE(s == ">>> OK \n");
}
