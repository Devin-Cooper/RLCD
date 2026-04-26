#pragma once

#include "file_entry.hpp"
#include <string>
#include <vector>

namespace fb {

class DirListing {
public:
    enum class HiddenMode { Hide, Show };

    bool load(const std::string& path, HiddenMode hidden);

    const std::vector<FileEntry>& entries() const { return entries_; }
    const std::string& path() const { return path_; }
    bool error() const { return error_; }
    int  errnoCode() const { return errno_; }

    /// Pure helper: applies hidden-filter + sort + bookmark prefix. Used by
    /// load() after readdir; exposed for host tests.
    static std::vector<FileEntry> assembleEntries(
        std::vector<FileEntry> raw,
        const std::string& path,
        HiddenMode hidden);

private:
    std::vector<FileEntry> entries_;
    std::string path_;
    bool error_ = false;
    int  errno_ = 0;
};

}  // namespace fb
