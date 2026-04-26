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
}

TEST_CASE("FileBuffer ASCII text mode", "[file_buffer]") {
    fb::FileBuffer b;
    auto p = tmpFile("hello\nworld\n");
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Text);
    REQUIRE(b.lineCount() == 2);
    REQUIRE(b.line(0) == "hello");
    REQUIRE(b.line(1) == "world");
}

TEST_CASE("FileBuffer no trailing newline", "[file_buffer]") {
    fb::FileBuffer b;
    auto p = tmpFile("abc\ndef");
    REQUIRE(b.load(p));
    REQUIRE(b.lineCount() == 2);
    REQUIRE(b.line(1) == "def");
}

TEST_CASE("FileBuffer empty file", "[file_buffer]") {
    fb::FileBuffer b;
    auto p = tmpFile("");
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Text);
    REQUIRE(b.lineCount() == 0);
}

TEST_CASE("FileBuffer NUL bytes -> hex", "[file_buffer]") {
    std::string buf(100, 'a');
    for (int i = 0; i < 10; ++i) buf[i * 10] = '\0';
    fb::FileBuffer b;
    auto p = tmpFile(buf);
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Hex);
}

TEST_CASE("FileBuffer tab expansion + 4096 clip", "[file_buffer]") {
    fb::FileBuffer b;
    auto p = tmpFile("a\tb\n");
    REQUIRE(b.load(p));
    REQUIRE(b.line(0) == "a   b");  // 4-space tab

    std::string huge(5000, 'x');
    auto p2 = tmpFile(huge);
    REQUIRE(b.load(p2));
    REQUIRE(b.line(0).size() == 4096);
}

TEST_CASE("FileBuffer UTF-8 multibyte stays Text", "[file_buffer]") {
    // U+00E9 (e-acute) = 0xC3 0xA9; 16 of them = 32 bytes, well-formed UTF-8.
    std::string u8;
    for (int i = 0; i < 16; ++i) { u8.push_back((char)0xC3); u8.push_back((char)0xA9); }
    fb::FileBuffer b;
    auto p = tmpFile(u8);
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Text);
}
