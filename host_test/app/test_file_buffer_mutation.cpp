#include <catch2/catch_test_macros.hpp>
#include "file_buffer.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>   // unlink for cleanup
#include <sys/stat.h>

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

TEST_CASE("FileBuffer snapshot/restore roundtrip", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("orig\n")));
    REQUIRE(b.snapshot());
    REQUIRE(b.hasSnapshot());
    REQUIRE(b.insert(0, "X"));
    REQUIRE(b.size() == 6);
    REQUIRE(b.restoreFromSnapshot());
    REQUIRE(b.size() == 5);
    REQUIRE(b.line(0) == "orig");
    REQUIRE_FALSE(b.hasSnapshot());
    REQUIRE(b.dirty());   // restored != on-disk pre-state, still dirty
}

TEST_CASE("FileBuffer snapshot is idempotent", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("a\n")));
    REQUIRE(b.snapshot());
    REQUIRE(b.snapshot());   // no-op when already have one
    REQUIRE(b.hasSnapshot());
}

TEST_CASE("FileBuffer snapshot of empty buffer", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("")));
    REQUIRE(b.snapshot());
    REQUIRE(b.insert(0, "abc"));
    REQUIRE(b.restoreFromSnapshot());
    REQUIRE(b.size() == 0);
}

TEST_CASE("FileBuffer::saveAtomic writes verbatim", "[mutation]") {
    fb::FileBuffer b;
    auto src = tmpFile("hello\nworld\n");
    REQUIRE(b.load(src));
    REQUIRE(b.insert(5, "!"));
    auto dst = tmpFile("");
    REQUIRE(b.saveAtomic(dst));

    std::ifstream f(dst, std::ios::binary);
    std::string out((std::istreambuf_iterator<char>(f)), {});
    REQUIRE(out == "hello!\nworld\n");

    // No leftover .tmp
    std::string tmp = dst + ".tmp";
    struct stat st;
    REQUIRE(::stat(tmp.c_str(), &st) != 0);

    ::unlink(dst.c_str());
}

TEST_CASE("FileBuffer::saveAtomic preserves CRLF bytes", "[mutation]") {
    fb::FileBuffer b;
    auto src = tmpFile("a\r\nb\r\n");
    REQUIRE(b.load(src));
    REQUIRE(b.crlf());
    auto dst = tmpFile("");
    REQUIRE(b.saveAtomic(dst));

    std::ifstream f(dst, std::ios::binary);
    std::string out((std::istreambuf_iterator<char>(f)), {});
    REQUIRE(out == "a\r\nb\r\n");

    ::unlink(dst.c_str());
}

TEST_CASE("FileBuffer::swapWithSnapshot toggles two-state", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("orig\n")));
    REQUIRE(b.snapshot());
    REQUIRE(b.insert(0, "X"));
    REQUIRE(b.line(0) == "Xorig");
    REQUIRE(b.swapWithSnapshot());
    REQUIRE(b.line(0) == "orig");
    REQUIRE(b.swapWithSnapshot());
    REQUIRE(b.line(0) == "Xorig");   // back to edited state
    REQUIRE(b.hasSnapshot());        // snapshot still present after swap
}

TEST_CASE("FileBuffer::saveAtomic rename failure leaves tmp", "[mutation]") {
    fb::FileBuffer b;
    REQUIRE(b.load(tmpFile("x")));
    // Pre-create dst as a directory to make rename fail (POSIX: rename
    // file-onto-directory is EISDIR).
    char dirpath[L_tmpnam];
    std::tmpnam(dirpath);
    REQUIRE(::mkdir(dirpath, 0755) == 0);
    REQUIRE_FALSE(b.saveAtomic(dirpath));
    REQUIRE(b.errnoCode() != 0);
    std::string tmp = std::string(dirpath) + ".tmp";
    struct stat st;
    REQUIRE(::stat(tmp.c_str(), &st) == 0);
    ::unlink(tmp.c_str());
    ::rmdir(dirpath);
}
