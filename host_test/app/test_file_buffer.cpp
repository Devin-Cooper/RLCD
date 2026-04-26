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

TEST_CASE("FileBuffer search case-insensitive", "[file_buffer]") {
    fb::FileBuffer b;
    auto p = tmpFile("hello\nWorld\nHELLO again\n");
    REQUIRE(b.load(p));
    auto m = b.findAll("hello");
    REQUIRE(m.size() == 2);
    REQUIRE(m[0] == 0);
    REQUIRE(m[1] == 2);

    REQUIRE(b.findAll("").empty());
}

TEST_CASE("FileBuffer hex row format", "[file_buffer]") {
    char data[20] = "ABC\x00\x01\x02";
    char out[80];
    fb::FileBuffer::formatHexRow(data, 6, 0, out);
    // First field "0000  41 42 43 00 01 02 ..."
    REQUIRE(std::string_view(out, 4) == "0000");
    REQUIRE(out[6] == '4'); REQUIRE(out[7] == '1');  // 'A' = 0x41
    REQUIRE(out[55] == '|');
    REQUIRE(out[56] == 'A');
    REQUIRE(out[57] == 'B');
    REQUIRE(out[58] == 'C');
    REQUIRE(out[59] == '.');  // 0x00 -> '.'
    REQUIRE(out[72] == '|');
}

// [REVIEW 2026-04-26] Spec testing item 3: random binary -> Hex mode detect.
TEST_CASE("FileBuffer random binary -> Hex", "[file_buffer]") {
    // Build a 1 KB buffer with high-bit-set bytes that violate UTF-8 lead-byte rules.
    std::string buf;
    buf.reserve(1024);
    // Use 0xF5+ (illegal UTF-8 start) to trip the bad-byte heuristic.
    for (int i = 0; i < 1024; ++i) {
        buf.push_back((char)(0xF5 + (i & 0x07)));
    }
    fb::FileBuffer b;
    auto p = tmpFile(buf);
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Hex);
}

// [REVIEW 2026-04-26] Spec testing item 4: 10 K random lines stress.
TEST_CASE("FileBuffer 10K-line index stress", "[file_buffer]") {
    std::string huge;
    huge.reserve(10000 * 8);
    for (int i = 0; i < 10000; ++i) {
        huge += "abcdefg\n";
    }
    fb::FileBuffer b;
    auto p = tmpFile(huge);
    REQUIRE(b.load(p));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::Text);
    REQUIRE(b.lineCount() == 10000);
    REQUIRE(b.line(0) == "abcdefg");
    REQUIRE(b.line(9999) == "abcdefg");
}

// [REVIEW 2026-04-26] Spec testing item 8: TooLarge boundary 4 MiB + 1 byte.
TEST_CASE("FileBuffer TooLarge boundary", "[file_buffer]") {
    // Write a >4 MiB file using sparse seek + a tail byte to avoid actually
    // allocating 4 MiB in RAM during the test.
    char path[L_tmpnam];
    std::tmpnam(path);
    {
        std::FILE* f = std::fopen(path, "wb");
        REQUIRE(f != nullptr);
        // Head marker.
        const char head_marker = 'H';
        std::fwrite(&head_marker, 1, 1, f);
        // Sparse seek to just past 4 MiB (4*1024*1024 + 1 byte total).
        std::fseek(f, 4 * 1024 * 1024, SEEK_SET);
        const char tail_marker = 'T';
        std::fwrite(&tail_marker, 1, 1, f);
        std::fclose(f);
    }
    fb::FileBuffer b;
    REQUIRE(b.load(path));
    REQUIRE(b.mode() == fb::FileBuffer::Mode::TooLarge);
    REQUIRE(b.size() == 64 * 1024);                       // head + tail = 32 KB + 32 KB
    REQUIRE(b.fileSize() == 4u * 1024 * 1024 + 1);
    // Head byte 0 should be 'H' (the marker we wrote at file offset 0).
    REQUIRE(b.data()[0] == 'H');
    // Last loaded byte should be 'T' (the marker at file_size - 1).
    REQUIRE(b.data()[64 * 1024 - 1] == 'T');
    std::remove(path);
}
