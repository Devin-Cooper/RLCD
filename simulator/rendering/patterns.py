"""
Bayer dither patterns for 1-bit framebuffer rendering.

This module provides ordered dithering patterns using a 4x4 Bayer matrix.
Patterns are used to create grayscale-like visual density on a 1-bit display.

Pattern levels:
- SOLID_BLACK: 100% fill (all pixels black)
- DENSE: ~75% fill (threshold 4)
- MEDIUM: ~50% fill (threshold 8)
- SPARSE: ~25% fill (threshold 12)
- SOLID_WHITE: 0% fill (all pixels white)

Patterns tile from global (0,0) origin for consistent rendering.
"""

try:
    from .framebuffer import Framebuffer
except ImportError:
    from framebuffer import Framebuffer


# 4x4 Bayer matrix for ordered dithering
# Values range from 0-15, representing thresholds for dithering
BAYER_4X4 = [
    [0, 8, 2, 10],
    [12, 4, 14, 6],
    [3, 11, 1, 9],
    [15, 7, 13, 5],
]


class Pattern:
    """Pattern constants for dithering levels."""
    SOLID_BLACK = 0   # 100% fill
    DENSE = 1         # ~75% fill (threshold 4)
    MEDIUM = 2        # ~50% fill (threshold 8)
    SPARSE = 3        # ~25% fill (threshold 12)
    SOLID_WHITE = 4   # 0% fill


# Threshold values for each pattern level
# A pixel is filled (black) if the Bayer matrix value is < threshold
_PATTERN_THRESHOLDS = {
    Pattern.SOLID_BLACK: 16,  # All values < 16, so 100% fill
    Pattern.DENSE: 12,        # Values 0-11 are filled, ~75% fill (12/16)
    Pattern.MEDIUM: 8,        # Values 0-7 are filled, ~50% fill (8/16)
    Pattern.SPARSE: 4,        # Values 0-3 are filled, ~25% fill (4/16)
    Pattern.SOLID_WHITE: 0,   # No values < 0, so 0% fill
}


def pattern_test(pattern: int, x: int, y: int) -> bool:
    """
    Test if a pixel should be filled (black) for a given pattern.

    Uses the Bayer 4x4 matrix to determine if the pixel at global
    coordinates (x, y) should be filled based on the pattern threshold.

    Args:
        pattern: Pattern level (Pattern.SOLID_BLACK to Pattern.SOLID_WHITE)
        x: X coordinate (global, patterns tile from 0,0)
        y: Y coordinate (global, patterns tile from 0,0)

    Returns:
        True if the pixel should be filled (black), False otherwise (white)
    """
    threshold = _PATTERN_THRESHOLDS.get(pattern, 0)

    # Handle edge cases
    if threshold <= 0:
        return False
    if threshold >= 16:
        return True

    # Get the Bayer matrix value for this position
    # Use modulo to tile the pattern from global (0, 0)
    bayer_value = BAYER_4X4[y & 3][x & 3]

    return bayer_value < threshold


def fill_polygon_pattern(
    fb: Framebuffer, points: list[tuple[int, int]], pattern: int
) -> None:
    """
    Fill a polygon using the scanline fill algorithm with pattern mask.

    This function is similar to fill_polygon from primitives, but applies
    a dither pattern instead of a solid fill. Each pixel is tested against
    the pattern to determine if it should be filled.

    Patterns tile from global (0,0) origin for consistent alignment
    across multiple shapes.

    Uses the even-odd rule for determining inside/outside.

    Args:
        fb: Framebuffer to draw on
        points: List of (x, y) coordinate tuples defining the polygon vertices
        pattern: Pattern level (Pattern.SOLID_BLACK to Pattern.SOLID_WHITE)
    """
    if len(points) < 3:
        return

    # Special case: SOLID_WHITE fills nothing
    if pattern == Pattern.SOLID_WHITE:
        return

    # Find bounding box
    min_y = min(p[1] for p in points)
    max_y = max(p[1] for p in points)

    # Clamp to framebuffer bounds
    min_y = max(0, min_y)
    max_y = min(fb.HEIGHT - 1, max_y)

    n = len(points)

    # Scanline fill
    for y in range(min_y, max_y + 1):
        # Find all intersections with this scanline
        intersections = []

        for i in range(n):
            x0, y0 = points[i]
            x1, y1 = points[(i + 1) % n]

            # Skip horizontal edges
            if y0 == y1:
                continue

            # Ensure y0 < y1 for consistent processing
            if y0 > y1:
                x0, y0, x1, y1 = x1, y1, x0, y0

            # Check if scanline intersects this edge
            # Use y0 <= y < y1 to handle vertices correctly (avoid double counting)
            if y0 <= y < y1:
                # Calculate x intersection using integer math
                # x = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                x_intersect = x0 + (y - y0) * (x1 - x0) // (y1 - y0)
                intersections.append(x_intersect)

        # Sort intersections
        intersections.sort()

        # Fill between pairs of intersections (even-odd rule)
        for i in range(0, len(intersections) - 1, 2):
            x_start = intersections[i]
            x_end = intersections[i + 1]

            # Apply pattern to each pixel in the span
            for x in range(x_start, x_end + 1):
                if pattern_test(pattern, x, y):
                    fb.set_pixel(x, y, True)
