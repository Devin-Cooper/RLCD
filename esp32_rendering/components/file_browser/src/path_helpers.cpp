#include "path_helpers.hpp"
#include <algorithm>

namespace fb::path {

std::string join(std::string_view a, std::string_view b) {
    if (a.empty()) return std::string(b);
    bool a_slash = a.back() == '/';
    bool b_slash = !b.empty() && b.front() == '/';
    std::string out(a);
    if (a_slash && b_slash) out.pop_back();
    else if (!a_slash && !b_slash && !b.empty()) out.push_back('/');
    out.append(b);
    return out;
}

std::string parent(std::string_view path) {
    if (path.empty()) return "/";
    // Strip trailing slash (except if path itself is "/").
    while (path.size() > 1 && path.back() == '/') path.remove_suffix(1);
    auto pos = path.rfind('/');
    if (pos == std::string_view::npos) return "/";
    if (pos == 0) return "/";
    return std::string(path.substr(0, pos));
}

std::string_view basename(std::string_view path) {
    while (path.size() > 1 && path.back() == '/') path.remove_suffix(1);
    auto pos = path.rfind('/');
    if (pos == std::string_view::npos) return path;
    return path.substr(pos + 1);
}

std::string truncateBreadcrumb(std::string_view path, std::size_t max_chars) {
    if (path.size() <= max_chars) return std::string(path);
    if (max_chars < 8) return std::string(path.substr(0, max_chars));  // pathological
    // Keep last basename + "..." prefix + as much head as fits.
    auto bn = basename(path);
    if (bn.size() + 4 >= max_chars) {
        // Even the basename doesn't fit; clip from the right.
        return "..." + std::string(path.substr(path.size() - (max_chars - 3)));
    }
    std::size_t head = max_chars - bn.size() - 4;  // ".../" between
    std::string out(path.substr(0, head));
    out.append(".../");
    out.append(bn);
    return out;
}

}  // namespace fb::path
