#include "command_palette_filter.hpp"
#include <cctype>
#include <cstring>

namespace app {

bool icontains(const char* haystack, const char* needle) {
    if (!needle || !*needle) return true;
    if (!haystack) return false;
    size_t n = std::strlen(needle);
    for (const char* p = haystack; *p; ++p) {
        bool match = true;
        for (size_t i = 0; i < n; ++i) {
            if (!p[i]) { match = false; break; }
            char a = static_cast<char>(std::tolower(static_cast<unsigned char>(p[i])));
            char b = static_cast<char>(std::tolower(static_cast<unsigned char>(needle[i])));
            if (a != b) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

} // namespace app
