// esp32_rendering/components/app/src/breadcrumb.cpp
#include "screen.hpp"
#include "screen_stack.hpp"
#include <cstring>

namespace app {

namespace {
constexpr const char kSep[] = " > ";
constexpr std::size_t kSepLen = sizeof(kSep) - 1;
constexpr const char kEllipsis[] = "... ";
constexpr std::size_t kEllipsisLen = sizeof(kEllipsis) - 1;

std::size_t labelLen(const Screen* s) {
    if (!s) return 1;
    return std::strlen(s->breadcrumbLabel());
}
} // namespace

void buildBreadcrumb(const ScreenStack& stack,
                     char* out, std::size_t out_capacity) {
    if (out_capacity == 0) return;
    out[0] = '\0';
    std::size_t depth = stack.depth();
    if (depth == 0) return;

    std::size_t needed = 0;
    for (std::size_t i = 0; i < depth; ++i) {
        needed += labelLen(stack.at(i));
        if (i + 1 < depth) needed += kSepLen;
    }

    auto formatRange = [&](std::size_t start, bool prefix_ellipsis) {
        std::size_t off = 0;
        if (prefix_ellipsis) {
            std::memcpy(out + off, kEllipsis, kEllipsisLen);
            off += kEllipsisLen;
        }
        for (std::size_t i = start; i < depth; ++i) {
            const Screen* s = stack.at(i);
            const char* l = s ? s->breadcrumbLabel() : "?";
            std::size_t n = std::strlen(l);
            std::memcpy(out + off, l, n);
            off += n;
            if (i + 1 < depth) {
                std::memcpy(out + off, kSep, kSepLen);
                off += kSepLen;
            }
        }
        out[off] = '\0';
    };

    if (needed + 1 <= out_capacity) {
        formatRange(0, /*prefix_ellipsis=*/false);
        return;
    }

    if (out_capacity <= kEllipsisLen + 1) {
        out[0] = '\0';
        return;
    }
    std::size_t budget = out_capacity - 1 - kEllipsisLen;

    // Find smallest `start` such that the tail [start..depth) fits in `budget`.
    std::size_t start = depth;
    std::size_t running = 0;
    for (std::size_t i = depth; i-- > 0; ) {
        std::size_t n = labelLen(stack.at(i));
        std::size_t step = (running == 0) ? n : (n + kSepLen);
        if (running + step > budget) break;
        running += step;
        start = i;
    }

    if (start >= depth) {
        // Nothing fits even with ellipsis — give up cleanly.
        out[0] = '\0';
        return;
    }
    formatRange(start, /*prefix_ellipsis=*/true);
}

} // namespace app
