#include "rendering/dirty_tracker.hpp"

namespace rendering {

DirtyTracker::DirtyTracker(int16_t width, int16_t height)
    : width_(width)
    , height_(height)
    , row_bytes_((width + 7) / 8)
{}

bool DirtyTracker::isClean(const uint8_t* current, const uint8_t* previous) const {
    size_t total = static_cast<size_t>(row_bytes_) * height_;
    return memcmp(current, previous, total) == 0;
}

std::vector<DirtyRegion> DirtyTracker::computeDirtyRegions(
    const uint8_t* current,
    const uint8_t* previous,
    int16_t min_gap
) {
    std::vector<DirtyRegion> regions;
    int16_t dirty_start = -1;

    for (int16_t y = 0; y < height_; y++) {
        const uint8_t* curr_row = current + y * row_bytes_;
        const uint8_t* prev_row = previous + y * row_bytes_;

        bool row_dirty = (memcmp(curr_row, prev_row, row_bytes_) != 0);

        if (row_dirty && dirty_start < 0) {
            dirty_start = y;
        } else if (!row_dirty && dirty_start >= 0) {
            // Look ahead to merge nearby regions
            bool merge = false;
            for (int16_t ahead = y; ahead < y + min_gap && ahead < height_; ahead++) {
                if (memcmp(current + ahead * row_bytes_,
                           previous + ahead * row_bytes_, row_bytes_) != 0) {
                    merge = true;
                    break;
                }
            }
            if (!merge) {
                regions.push_back({dirty_start, y});
                dirty_start = -1;
            }
        }
    }

    if (dirty_start >= 0) {
        regions.push_back({dirty_start, height_});
    }

    return regions;
}

} // namespace rendering
