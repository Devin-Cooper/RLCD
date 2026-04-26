#include "dir_listing.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>

namespace fb {

namespace {
struct Bookmark { const char* label; const char* target; bool is_dir; };
constexpr Bookmark kBookmarks[] = {
    { "/servers/  (bookmark)", "/sdcard/servers",   true  },
    { "wifi.json  (bookmark)", "/sdcard/wifi.json", false },
};

int caseCmp(const std::string& a, const std::string& b) {
    auto ai = a.begin(); auto bi = b.begin();
    while (ai != a.end() && bi != b.end()) {
        int ca = std::tolower((unsigned char)*ai);
        int cb = std::tolower((unsigned char)*bi);
        if (ca != cb) return ca - cb;
        ++ai; ++bi;
    }
    return (a.size() < b.size()) ? -1 : (a.size() > b.size() ? 1 : 0);
}
}  // namespace

std::vector<FileEntry> DirListing::assembleEntries(
    std::vector<FileEntry> raw, const std::string& path, HiddenMode hidden) {

    if (hidden == HiddenMode::Hide) {
        raw.erase(std::remove_if(raw.begin(), raw.end(),
            [](const FileEntry& e) { return !e.name.empty() && e.name.front() == '.'; }),
            raw.end());
    }
    std::sort(raw.begin(), raw.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir;
        return caseCmp(a.name, b.name) < 0;
    });
    if (path == "/sdcard") {
        std::vector<FileEntry> out;
        out.reserve(raw.size() + sizeof(kBookmarks)/sizeof(kBookmarks[0]));
        for (const auto& bm : kBookmarks) {
            FileEntry e;
            e.name = bm.label;
            e.is_dir = bm.is_dir;
            e.bookmark_target = bm.target;
            out.push_back(std::move(e));
        }
        for (auto& e : raw) out.push_back(std::move(e));
        return out;
    }
    return raw;
}

bool DirListing::load(const std::string& path, HiddenMode hidden) {
    entries_.clear();
    path_ = path;
    error_ = false;
    errno_ = 0;

    DIR* d = opendir(path.c_str());
    if (!d) {
        error_ = true;
        errno_ = errno;
        return false;
    }
    std::vector<FileEntry> raw;
    struct dirent* de;
    while ((de = readdir(d)) != nullptr) {
        if (std::strcmp(de->d_name, ".") == 0 || std::strcmp(de->d_name, "..") == 0) continue;
        FileEntry e;
        e.name = de->d_name;
        std::string full = path;
        if (full.empty() || full.back() != '/') full.push_back('/');
        full.append(de->d_name);
        struct stat st{};
        if (::stat(full.c_str(), &st) == 0) {
            e.is_dir = S_ISDIR(st.st_mode);
            e.size = e.is_dir ? 0 : (uint64_t)st.st_size;
            e.mtime = st.st_mtime;
        } else {
            // Skip stat-failing entries per spec.
            continue;
        }
        raw.push_back(std::move(e));
    }
    closedir(d);
    entries_ = assembleEntries(std::move(raw), path, hidden);
    return true;
}

}  // namespace fb
