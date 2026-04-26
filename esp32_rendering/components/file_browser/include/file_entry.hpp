#pragma once

#include <cstdint>
#include <ctime>
#include <string>

namespace fb {

struct FileEntry {
    std::string name;
    bool        is_dir = false;
    uint64_t    size = 0;
    time_t      mtime = 0;
    std::string bookmark_target;  // empty unless this is a bookmark row
};

}  // namespace fb
