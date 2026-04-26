#include <catch2/catch_test_macros.hpp>
#include "dir_listing.hpp"

using fb::DirListing;
using fb::FileEntry;

static FileEntry mk(const char* name, bool is_dir, uint64_t size = 0, time_t mt = 0) {
    return FileEntry{name, is_dir, size, mt, ""};
}

TEST_CASE("DirListing dirs-first alpha", "[dir_listing]") {
    std::vector<FileEntry> raw = {
        mk("zzz.txt", false), mk("alpha", true),
        mk("Beta", true), mk("aaa.txt", false),
    };
    auto out = DirListing::assembleEntries(std::move(raw), "/sdcard/sub", DirListing::HiddenMode::Hide);
    REQUIRE(out.size() == 4);
    REQUIRE(out[0].name == "alpha");   // dirs first, case-insensitive alpha
    REQUIRE(out[1].name == "Beta");
    REQUIRE(out[2].name == "aaa.txt");
    REQUIRE(out[3].name == "zzz.txt");
}

TEST_CASE("DirListing hides dotfiles", "[dir_listing]") {
    std::vector<FileEntry> raw = {
        mk(".hidden", false), mk("visible", false), mk(".dotdir", true),
    };
    // Use a non-root path so bookmark rows don't pollute the dotfile-filter assertion.
    auto hide = DirListing::assembleEntries(raw, "/sdcard/sub", DirListing::HiddenMode::Hide);
    REQUIRE(hide.size() == 1);
    REQUIRE(hide[0].name == "visible");

    auto show = DirListing::assembleEntries(raw, "/sdcard/sub", DirListing::HiddenMode::Show);
    REQUIRE(show.size() == 3);
}

TEST_CASE("DirListing bookmarks at root only", "[dir_listing]") {
    std::vector<FileEntry> raw = { mk("foo", false) };

    auto root = DirListing::assembleEntries(raw, "/sdcard", DirListing::HiddenMode::Hide);
    REQUIRE(root.size() >= 3);  // 2 bookmarks + 1 file
    REQUIRE(!root[0].bookmark_target.empty());
    REQUIRE(!root[1].bookmark_target.empty());

    auto sub = DirListing::assembleEntries(raw, "/sdcard/sub", DirListing::HiddenMode::Hide);
    REQUIRE(sub.size() == 1);
    REQUIRE(sub[0].bookmark_target.empty());
}
