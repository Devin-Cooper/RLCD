#include <catch2/catch_test_macros.hpp>
#include "file_buffer.hpp"
#include <cstdio>
#include <fstream>
#include <string>

namespace {
std::string tmpFile(const std::string& contents) {
    char path[L_tmpnam];
    std::tmpnam(path);
    std::ofstream f(path, std::ios::binary);
    f.write(contents.data(), (std::streamsize)contents.size());
    return path;
}
}  // namespace

TEST_CASE("FileBuffer::insert at start of single-line buffer", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("hello\n")));
    REQUIRE(b.insert(0, "X"));
    REQUIRE(b.size() == 7);
    REQUIRE(b.lineCount() == 1);   // triggers lazy rebuild
    REQUIRE(b.line(0) == "Xhello");
    REQUIRE(b.dirty());
}

TEST_CASE("FileBuffer::insert multi-line text grows lineCount", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("a\nb\n")));
    REQUIRE(b.lineCount() == 2);
    REQUIRE(b.insert(2, "MID\nLINE\n"));   // between 'b' and the newline before it
    REQUIRE(b.lineCount() == 4);
}

TEST_CASE("FileBuffer::erase across line boundary", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("aaa\nbbb\nccc\n")));
    REQUIRE(b.lineCount() == 3);
    REQUIRE(b.erase(3, 1));   // delete first '\n'
    REQUIRE(b.lineCount() == 2);
    REQUIRE(b.line(0) == "aaabbb");
}

TEST_CASE("FileBuffer::erase len=0 is noop", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("abc")));
    REQUIRE(b.erase(0, 0));
    REQUIRE(b.size() == 3);
    REQUIRE_FALSE(b.dirty());
}

TEST_CASE("FileBuffer::insert rejects > 256 KB cap", "[mutation]") {
    fb::FileBuffer b;
    std::string base(200u * 1024u, 'a');
    REQUIRE(b.load(tmpFile(base)));
    std::string blob(64u * 1024u, 'b');
    REQUIRE_FALSE(b.insert(0, blob));   // 200K + 64K = 264K > 256K cap
    REQUIRE(b.size() == base.size());
    REQUIRE_FALSE(b.dirty());
}

TEST_CASE("FileBuffer::insert on Hex mode returns false", "[mutation]") {
    std::string buf(100, '\0');   // forces Hex
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile(buf)));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Hex);
    REQUIRE_FALSE(b.insert(0, "x"));
    REQUIRE_FALSE(b.dirty());
}

TEST_CASE("FileBuffer CRLF sniffed on load", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("hello\r\nworld\r\n")));
    REQUIRE(b.crlf());

    fb::FileBuffer b2;
    REQUIRE(b2.load(tmpFile("hello\nworld\n")));
    REQUIRE_FALSE(b2.crlf());
}
