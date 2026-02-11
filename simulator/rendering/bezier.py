"""
Bezier curve rendering with Pope's texture-ball stroke technique.

This module provides cubic bezier curve evaluation, adaptive subdivision,
and texture-ball strokes for creating organic, hand-drawn looking outlines
inspired by Lucas Pope's Mars After Midnight.

The texture-ball technique stamps a small irregular bitmap along a bezier path,
creating natural-looking strokes with organic imperfections.

Algorithms used:
- De Casteljau's algorithm for bezier evaluation
- Catmull-Rom style auto-tangent calculation
- Adaptive subdivision based on flatness testing
- Texture-ball stamping with tangent-aligned rotation
"""

import math

try:
    from .framebuffer import Framebuffer
except ImportError:
    from framebuffer import Framebuffer


# Default 8x8 texture ball - an irregular scribble-like pattern
# True = black (ink), False = white (transparent)
# This creates an organic, hand-drawn looking brush tip
DEFAULT_BALL_8X8: list[list[bool]] = [
    [False, False, True,  True,  True,  True,  False, False],
    [False, True,  True,  True,  True,  True,  True,  False],
    [True,  True,  True,  False, True,  True,  True,  True ],
    [True,  True,  True,  True,  True,  False, True,  True ],
    [True,  True,  False, True,  True,  True,  True,  True ],
    [True,  True,  True,  True,  False, True,  True,  True ],
    [False, True,  True,  True,  True,  True,  True,  False],
    [False, False, True,  True,  True,  True,  False, False],
]


def auto_tangent(
    points: list[tuple[float, float]], smoothness: float = 0.5
) -> list[tuple[tuple[float, float], tuple[float, float]]]:
    """
    Generate control point handles for smooth curve through points.

    Uses Catmull-Rom style tangent calculation where the tangent at each
    point is parallel to the line from the previous point to the next point.

    Args:
        points: List of (x, y) points the curve should pass through.
        smoothness: Smoothness factor from 0.0 (sharp corners) to 1.0
                   (maximum smoothing). Controls handle length relative
                   to neighbor distances.

    Returns:
        List of (handle_in, handle_out) tuples for each point.
        Each handle is an (x, y) offset from the point.
        For a point P with handles (h_in, h_out), the control points are:
        - P + h_in for the incoming bezier segment
        - P + h_out for the outgoing bezier segment
    """
    n = len(points)
    if n == 0:
        return []
    if n == 1:
        return [((0.0, 0.0), (0.0, 0.0))]

    handles = []

    for i in range(n):
        # Get previous and next points for tangent calculation
        if i == 0:
            # First point: tangent toward next point
            prev = points[0]
            next_pt = points[1]
        elif i == n - 1:
            # Last point: tangent from previous point
            prev = points[n - 2]
            next_pt = points[n - 1]
        else:
            # Middle points: tangent parallel to prev->next
            prev = points[i - 1]
            next_pt = points[i + 1]

        curr = points[i]

        # Calculate tangent direction (prev -> next)
        dx = next_pt[0] - prev[0]
        dy = next_pt[1] - prev[1]

        # Normalize tangent
        length = math.sqrt(dx * dx + dy * dy)
        if length < 1e-10:
            # Degenerate case: prev and next are the same
            handles.append(((0.0, 0.0), (0.0, 0.0)))
            continue

        tx = dx / length
        ty = dy / length

        # Calculate handle lengths based on distance to neighbors
        if i == 0:
            # First point: use distance to next point
            dist_prev = 0.0
            dist_next = math.sqrt(
                (points[1][0] - curr[0]) ** 2 + (points[1][1] - curr[1]) ** 2
            )
        elif i == n - 1:
            # Last point: use distance to previous point
            dist_prev = math.sqrt(
                (curr[0] - points[n - 2][0]) ** 2 + (curr[1] - points[n - 2][1]) ** 2
            )
            dist_next = 0.0
        else:
            # Middle points: use distances to both neighbors
            dist_prev = math.sqrt(
                (curr[0] - points[i - 1][0]) ** 2 + (curr[1] - points[i - 1][1]) ** 2
            )
            dist_next = math.sqrt(
                (points[i + 1][0] - curr[0]) ** 2 + (points[i + 1][1] - curr[1]) ** 2
            )

        # Handle lengths are proportional to distance and smoothness
        # The factor of 0.5 prevents overshooting (handles go 1/3 of segment length)
        handle_in_len = smoothness * dist_prev * 0.5
        handle_out_len = smoothness * dist_next * 0.5

        # Handle directions: in is opposite to tangent, out is along tangent
        handle_in = (-tx * handle_in_len, -ty * handle_in_len)
        handle_out = (tx * handle_out_len, ty * handle_out_len)

        handles.append((handle_in, handle_out))

    return handles


def cubic_bezier(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    t: float,
) -> tuple[float, float]:
    """
    Evaluate cubic bezier at parameter t using De Casteljau's algorithm.

    De Casteljau's algorithm is numerically stable and geometrically intuitive:
    it repeatedly interpolates between control points.

    Args:
        p0: Start point
        p1: First control point (influences curve near p0)
        p2: Second control point (influences curve near p3)
        p3: End point
        t: Parameter from 0.0 (at p0) to 1.0 (at p3)

    Returns:
        The (x, y) point on the curve at parameter t.
    """
    # De Casteljau's algorithm: linear interpolation in stages
    # Level 1: interpolate between adjacent control points
    def lerp(a: tuple[float, float], b: tuple[float, float], t: float) -> tuple[float, float]:
        return (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))

    # Level 1
    q0 = lerp(p0, p1, t)
    q1 = lerp(p1, p2, t)
    q2 = lerp(p2, p3, t)

    # Level 2
    r0 = lerp(q0, q1, t)
    r1 = lerp(q1, q2, t)

    # Level 3 - final point
    return lerp(r0, r1, t)


def cubic_bezier_derivative(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    t: float,
) -> tuple[float, float]:
    """
    Evaluate the derivative (tangent) of cubic bezier at parameter t.

    The derivative of a cubic bezier is a quadratic bezier of its control
    point differences, multiplied by 3.

    Args:
        p0: Start point
        p1: First control point
        p2: Second control point
        p3: End point
        t: Parameter from 0.0 to 1.0

    Returns:
        The (dx, dy) tangent vector at parameter t.
    """
    # Derivative control points: 3 * (P[i+1] - P[i])
    d0 = (3 * (p1[0] - p0[0]), 3 * (p1[1] - p0[1]))
    d1 = (3 * (p2[0] - p1[0]), 3 * (p2[1] - p1[1]))
    d2 = (3 * (p3[0] - p2[0]), 3 * (p3[1] - p2[1]))

    # Evaluate quadratic bezier of derivative control points
    def lerp(a: tuple[float, float], b: tuple[float, float], t: float) -> tuple[float, float]:
        return (a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]))

    q0 = lerp(d0, d1, t)
    q1 = lerp(d1, d2, t)

    return lerp(q0, q1, t)


def _bezier_flatness(
    p0: tuple[float, float],
    c0: tuple[float, float],
    c1: tuple[float, float],
    p1: tuple[float, float],
) -> float:
    """
    Calculate the flatness of a bezier curve segment.

    Uses the maximum deviation of control points from the baseline
    as a measure of how "curved" the segment is.

    Args:
        p0: Start point
        c0: First control point
        c1: Second control point
        p1: End point

    Returns:
        Maximum perpendicular distance of control points from the p0-p1 line.
    """
    # Vector from p0 to p1
    dx = p1[0] - p0[0]
    dy = p1[1] - p0[1]

    length_sq = dx * dx + dy * dy

    if length_sq < 1e-10:
        # Degenerate case: p0 and p1 are the same point
        # Return distance to control points
        d0 = math.sqrt((c0[0] - p0[0]) ** 2 + (c0[1] - p0[1]) ** 2)
        d1 = math.sqrt((c1[0] - p0[0]) ** 2 + (c1[1] - p0[1]) ** 2)
        return max(d0, d1)

    length = math.sqrt(length_sq)

    # Perpendicular distance of c0 from line p0-p1
    # Using cross product: |((c0-p0) x (p1-p0))| / |p1-p0|
    cross0 = (c0[0] - p0[0]) * dy - (c0[1] - p0[1]) * dx
    dist0 = abs(cross0) / length

    # Perpendicular distance of c1 from line p0-p1
    cross1 = (c1[0] - p0[0]) * dy - (c1[1] - p0[1]) * dx
    dist1 = abs(cross1) / length

    return max(dist0, dist1)


def _subdivide_bezier_recursive(
    p0: tuple[float, float],
    c0: tuple[float, float],
    c1: tuple[float, float],
    p1: tuple[float, float],
    tolerance: float,
    points: list[tuple[int, int]],
) -> None:
    """
    Recursively subdivide bezier curve using De Casteljau's algorithm.

    Adds intermediate points to the points list when subdivision is needed.
    """
    flatness = _bezier_flatness(p0, c0, c1, p1)

    if flatness <= tolerance:
        # Curve is flat enough, add endpoint
        points.append((int(round(p1[0])), int(round(p1[1]))))
    else:
        # Subdivide at t=0.5 using De Casteljau's algorithm
        # Level 1
        q0 = ((p0[0] + c0[0]) / 2, (p0[1] + c0[1]) / 2)
        q1 = ((c0[0] + c1[0]) / 2, (c0[1] + c1[1]) / 2)
        q2 = ((c1[0] + p1[0]) / 2, (c1[1] + p1[1]) / 2)

        # Level 2
        r0 = ((q0[0] + q1[0]) / 2, (q0[1] + q1[1]) / 2)
        r1 = ((q1[0] + q2[0]) / 2, (q1[1] + q2[1]) / 2)

        # Level 3 - midpoint
        mid = ((r0[0] + r1[0]) / 2, (r0[1] + r1[1]) / 2)

        # Recursively subdivide left and right halves
        # Left: p0, q0, r0, mid
        _subdivide_bezier_recursive(p0, q0, r0, mid, tolerance, points)
        # Right: mid, r1, q2, p1
        _subdivide_bezier_recursive(mid, r1, q2, p1, tolerance, points)


def subdivide_bezier(
    p0: tuple[float, float],
    c0: tuple[float, float],
    c1: tuple[float, float],
    p1: tuple[float, float],
    tolerance: float = 1.0,
) -> list[tuple[int, int]]:
    """
    Adaptively subdivide bezier into line segments.

    Uses recursive subdivision based on flatness testing. The curve is
    subdivided until all segments deviate less than tolerance pixels
    from the true curve.

    Args:
        p0: Start point
        c0: First control point
        c1: Second control point
        p1: End point
        tolerance: Maximum pixel deviation allowed. 1.0 is good for 1-bit
                  displays. Smaller values = more segments, smoother curves.

    Returns:
        List of integer (x, y) points approximating the curve.
        Includes both start and end points.
    """
    points: list[tuple[int, int]] = [(int(round(p0[0])), int(round(p0[1])))]
    _subdivide_bezier_recursive(p0, c0, c1, p1, tolerance, points)
    return points


def _rotate_point(
    x: float, y: float, cos_theta: float, sin_theta: float
) -> tuple[float, float]:
    """Rotate point (x, y) around origin by angle with given cos/sin."""
    return (x * cos_theta - y * sin_theta, x * sin_theta + y * cos_theta)


def _splat_texture(
    fb: Framebuffer,
    cx: float,
    cy: float,
    texture: list[list[bool]],
    cos_theta: float,
    sin_theta: float,
) -> None:
    """
    Splat (stamp) a texture centered at (cx, cy) with rotation.

    Args:
        fb: Framebuffer to draw on
        cx: Center X coordinate
        cy: Center Y coordinate
        texture: 2D bitmap to stamp (True = draw pixel)
        cos_theta: Cosine of rotation angle
        sin_theta: Sine of rotation angle
    """
    h = len(texture)
    if h == 0:
        return
    w = len(texture[0])

    # Center of texture
    half_w = w / 2.0
    half_h = h / 2.0

    # For each pixel in the texture
    for ty in range(h):
        row = texture[ty]
        for tx in range(len(row)):
            if not row[tx]:
                continue  # Skip transparent pixels

            # Offset from texture center
            dx = tx - half_w + 0.5  # Add 0.5 to sample pixel center
            dy = ty - half_h + 0.5

            # Rotate offset
            rx, ry = _rotate_point(dx, dy, cos_theta, sin_theta)

            # Final pixel position
            px = int(round(cx + rx))
            py = int(round(cy + ry))

            fb.set_pixel(px, py, True)


def stroke_bezier_texture_ball(
    fb: Framebuffer,
    points: list[tuple[float, float]],
    smoothness: float,
    ball_texture: list[list[bool]],
    spacing: float = 2.0,
) -> None:
    """
    Pope's texture-ball stroke technique.

    Creates organic, hand-drawn looking strokes by stamping a small
    bitmap texture along a bezier curve path. The texture is rotated
    to align with the path tangent, giving a natural brush-like effect.

    Algorithm:
    1. Generate bezier path through points with auto-tangents
    2. Walk path at 'spacing' pixel intervals
    3. At each step, splat ball_texture rotated to path tangent

    Args:
        fb: Framebuffer to draw on
        points: List of (x, y) points the curve should pass through
        smoothness: Smoothness factor for auto-tangent (0.0-1.0)
        ball_texture: Small 2D bitmap to stamp (e.g. 8x8).
                     True = draw pixel, False = transparent.
        spacing: Pixel interval between texture stamps. Smaller = denser.
                2.0 is a good default for 8x8 textures.
    """
    if len(points) < 2:
        if len(points) == 1:
            # Single point: just splat the texture
            _splat_texture(fb, points[0][0], points[0][1], ball_texture, 1.0, 0.0)
        return

    # Generate auto-tangent handles
    handles = auto_tangent(points, smoothness)

    # Process each bezier segment
    for i in range(len(points) - 1):
        p0 = points[i]
        p3 = points[i + 1]

        # Control points from handles
        h_out = handles[i][1]  # Outgoing handle of p0
        h_in = handles[i + 1][0]  # Incoming handle of p3

        p1 = (p0[0] + h_out[0], p0[1] + h_out[1])
        p2 = (p3[0] + h_in[0], p3[1] + h_in[1])

        # Estimate curve length for stepping
        # Use subdivided points to get approximate length
        subdivided = subdivide_bezier(p0, p1, p2, p3, tolerance=spacing)

        # Calculate total arc length
        arc_length = 0.0
        for j in range(1, len(subdivided)):
            dx = subdivided[j][0] - subdivided[j - 1][0]
            dy = subdivided[j][1] - subdivided[j - 1][1]
            arc_length += math.sqrt(dx * dx + dy * dy)

        if arc_length < 0.1:
            continue

        # Walk along the curve at spacing intervals
        # Use parameter stepping with arc-length approximation
        num_steps = max(1, int(arc_length / spacing))

        for step in range(num_steps + 1):
            # Approximate parameter t for uniform spacing
            # This is an approximation; true arc-length parametrization
            # would require integration
            t = step / num_steps if num_steps > 0 else 0.0

            # Skip first step on non-first segment to avoid double stamping
            if i > 0 and step == 0:
                continue

            # Get position and tangent
            pos = cubic_bezier(p0, p1, p2, p3, t)
            tangent = cubic_bezier_derivative(p0, p1, p2, p3, t)

            # Calculate rotation angle from tangent
            tangent_len = math.sqrt(tangent[0] ** 2 + tangent[1] ** 2)
            if tangent_len < 1e-10:
                cos_theta = 1.0
                sin_theta = 0.0
            else:
                cos_theta = tangent[0] / tangent_len
                sin_theta = tangent[1] / tangent_len

            # Splat texture at this position
            _splat_texture(fb, pos[0], pos[1], ball_texture, cos_theta, sin_theta)


def draw_bezier_curve(
    fb: Framebuffer,
    points: list[tuple[float, float]],
    smoothness: float = 0.5,
    color: bool = True,
    tolerance: float = 1.0,
) -> None:
    """
    Draw a smooth bezier curve through points using line segments.

    This is a simpler alternative to texture-ball strokes that just
    draws the curve as connected line segments.

    Args:
        fb: Framebuffer to draw on
        points: List of (x, y) points the curve should pass through
        smoothness: Smoothness factor for auto-tangent (0.0-1.0)
        color: True for black, False for white
        tolerance: Subdivision tolerance in pixels
    """
    if len(points) < 2:
        if len(points) == 1:
            fb.set_pixel(int(round(points[0][0])), int(round(points[0][1])), color)
        return

    # Generate auto-tangent handles
    handles = auto_tangent(points, smoothness)

    # Draw each bezier segment
    for i in range(len(points) - 1):
        p0 = points[i]
        p3 = points[i + 1]

        h_out = handles[i][1]
        h_in = handles[i + 1][0]

        p1 = (p0[0] + h_out[0], p0[1] + h_out[1])
        p2 = (p3[0] + h_in[0], p3[1] + h_in[1])

        # Subdivide and draw line segments
        subdivided = subdivide_bezier(p0, p1, p2, p3, tolerance)

        for j in range(1, len(subdivided)):
            x0, y0 = subdivided[j - 1]
            x1, y1 = subdivided[j]
            # Use primitives draw_line if available, or simple pixel walk
            _draw_line_simple(fb, x0, y0, x1, y1, color)


def _draw_line_simple(
    fb: Framebuffer, x0: int, y0: int, x1: int, y1: int, color: bool
) -> None:
    """Simple Bresenham line drawing for internal use."""
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy

    x, y = x0, y0

    while True:
        fb.set_pixel(x, y, color)

        if x == x1 and y == y1:
            break

        e2 = 2 * err

        if e2 > -dy:
            err -= dy
            x += sx

        if e2 < dx:
            err += dx
            y += sy
