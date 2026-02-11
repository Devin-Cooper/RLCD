"""
Geometric drawing primitives for 1-bit framebuffer.

This module provides fundamental drawing functions for geometric shapes.
All functions use the Framebuffer's optimized fill_span() where possible
for efficient horizontal fills.

Algorithms used:
- Bresenham's line algorithm for draw_line
- Midpoint circle algorithm for circles
- Scanline fill algorithm for polygon filling
"""

try:
    from .framebuffer import Framebuffer
except ImportError:
    from framebuffer import Framebuffer


def draw_line(fb: Framebuffer, x0: int, y0: int, x1: int, y1: int, color: bool) -> None:
    """
    Draw a line from (x0, y0) to (x1, y1) using Bresenham's algorithm.

    The line includes both endpoints. Handles all octants correctly.

    Args:
        fb: Framebuffer to draw on
        x0: Starting X coordinate
        y0: Starting Y coordinate
        x1: Ending X coordinate
        y1: Ending Y coordinate
        color: True for black, False for white
    """
    # Calculate deltas
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)

    # Determine step directions
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1

    # Initialize error term
    err = dx - dy

    x, y = x0, y0

    while True:
        fb.set_pixel(x, y, color)

        # Check if we've reached the endpoint
        if x == x1 and y == y1:
            break

        e2 = 2 * err

        if e2 > -dy:
            err -= dy
            x += sx

        if e2 < dx:
            err += dx
            y += sy


def draw_polygon(fb: Framebuffer, points: list[tuple[int, int]], color: bool) -> None:
    """
    Draw a polygon outline as connected line segments.

    The polygon is automatically closed (last point connects to first).
    If fewer than 2 points are provided, nothing is drawn.

    Args:
        fb: Framebuffer to draw on
        points: List of (x, y) coordinate tuples defining the polygon vertices
        color: True for black, False for white
    """
    if len(points) < 2:
        return

    # Draw edges between consecutive points
    for i in range(len(points)):
        x0, y0 = points[i]
        x1, y1 = points[(i + 1) % len(points)]
        draw_line(fb, x0, y0, x1, y1, color)


def fill_polygon(fb: Framebuffer, points: list[tuple[int, int]], color: bool) -> None:
    """
    Fill a polygon using the scanline fill algorithm.

    Handles convex and simple concave polygons. For self-intersecting
    polygons, the result may be unpredictable.

    Uses the even-odd rule for determining inside/outside.

    Args:
        fb: Framebuffer to draw on
        points: List of (x, y) coordinate tuples defining the polygon vertices
        color: True for black, False for white
    """
    if len(points) < 3:
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
            # fill_span uses exclusive end
            fb.fill_span(y, x_start, x_end + 1, color)


def fill_rect(fb: Framebuffer, x: int, y: int, w: int, h: int, color: bool) -> None:
    """
    Fill an axis-aligned rectangle.

    Uses fill_span for efficient horizontal fills.

    Args:
        fb: Framebuffer to draw on
        x: Left edge X coordinate
        y: Top edge Y coordinate
        w: Width in pixels
        h: Height in pixels
        color: True for black, False for white
    """
    if w <= 0 or h <= 0:
        return

    x_end = x + w

    for row in range(y, y + h):
        fb.fill_span(row, x, x_end, color)


def draw_circle(fb: Framebuffer, cx: int, cy: int, r: int, color: bool) -> None:
    """
    Draw a circle outline using the midpoint circle algorithm.

    Args:
        fb: Framebuffer to draw on
        cx: Center X coordinate
        cy: Center Y coordinate
        r: Radius in pixels
        color: True for black, False for white
    """
    if r < 0:
        return

    if r == 0:
        fb.set_pixel(cx, cy, color)
        return

    # Midpoint circle algorithm
    x = r
    y = 0
    d = 1 - r  # Decision parameter

    while x >= y:
        # Draw 8 octant symmetric points
        fb.set_pixel(cx + x, cy + y, color)
        fb.set_pixel(cx - x, cy + y, color)
        fb.set_pixel(cx + x, cy - y, color)
        fb.set_pixel(cx - x, cy - y, color)
        fb.set_pixel(cx + y, cy + x, color)
        fb.set_pixel(cx - y, cy + x, color)
        fb.set_pixel(cx + y, cy - x, color)
        fb.set_pixel(cx - y, cy - x, color)

        y += 1

        if d <= 0:
            # Midpoint is inside the circle
            d += 2 * y + 1
        else:
            # Midpoint is outside the circle
            x -= 1
            d += 2 * y - 2 * x + 1


def fill_circle(fb: Framebuffer, cx: int, cy: int, r: int, color: bool) -> None:
    """
    Fill a circle using horizontal spans.

    Uses the midpoint circle algorithm to determine the circle boundary
    and fill_span for efficient horizontal fills.

    Args:
        fb: Framebuffer to draw on
        cx: Center X coordinate
        cy: Center Y coordinate
        r: Radius in pixels
        color: True for black, False for white
    """
    if r < 0:
        return

    if r == 0:
        fb.set_pixel(cx, cy, color)
        return

    # Midpoint circle algorithm with horizontal span filling
    x = r
    y = 0
    d = 1 - r

    while x >= y:
        # Fill horizontal spans for all 4 quadrants
        # Top and bottom spans (using y offset for x span width)
        fb.fill_span(cy + y, cx - x, cx + x + 1, color)
        fb.fill_span(cy - y, cx - x, cx + x + 1, color)

        # Middle spans (using x offset for y position)
        fb.fill_span(cy + x, cx - y, cx + y + 1, color)
        fb.fill_span(cy - x, cx - y, cx + y + 1, color)

        y += 1

        if d <= 0:
            d += 2 * y + 1
        else:
            x -= 1
            d += 2 * y - 2 * x + 1
