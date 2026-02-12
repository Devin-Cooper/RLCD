#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

namespace rendering {

/// A contiguous range of dirty rows
struct DirtyRegion {
    int16_t y_start;    // First dirty row (inclusive)
    int16_t y_end;      // Last dirty row (exclusive)
};

/// Compares current vs previous framebuffer to find dirty row ranges.
/// Used to skip display transfers when nothing has changed, and to
/// identify which regions need updating.
class DirtyTracker {
public:
    DirtyTracker(int16_t width, int16_t height);

    /// Compare current vs previous framebuffer, return dirty row ranges.
    /// Merges nearby regions closer than min_gap rows to reduce overhead.
    std::vector<DirtyRegion> computeDirtyRegions(
        const uint8_t* current,
        const uint8_t* previous,
        int16_t min_gap = 8
    );

    /// Quick check: are the two buffers identical?
    bool isClean(const uint8_t* current, const uint8_t* previous) const;

private:
    int16_t width_;
    int16_t height_;
    int16_t row_bytes_;
};

} // namespace rendering
