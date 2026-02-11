"""
Vector font for geometric line-based characters.

This module provides scalable numerals, letters, and punctuation for
display rendering. Each character is defined as a series of line segment
strokes in a 0-100 unit coordinate space, allowing resolution-independent scaling.

Design style:
- Angular/geometric aesthetic inspired by hexagonal shapes
- Bold, readable silhouettes (spanning roughly 5-95 in x, 10-90 in y)
- All straight line segments (no curves)
- 15-unit diagonal cuts at corners for angular/hexagonal appearance
- Optimized for 1-bit display with high contrast
"""

try:
    from .framebuffer import Framebuffer
    from .primitives import draw_line
except ImportError:
    from framebuffer import Framebuffer
    from primitives import draw_line


# Each glyph is a list of strokes. Each stroke is a list of (x, y) points.
# Coordinates are in a 0-100 unit square, scaled at render time.
# Strokes are connected polylines - each point connects to the next.
GLYPHS = {
    # 0: Hexagonal shape with angled corners
    '0': [
        [(20, 10), (80, 10), (95, 25), (95, 75), (80, 90), (20, 90), (5, 75), (5, 25), (20, 10)]
    ],

    # 1: Simple vertical with angular top and base
    '1': [
        [(30, 20), (50, 10), (50, 90)],
        [(30, 90), (70, 90)]
    ],

    # 2: Angular flowing shape
    '2': [
        [(10, 25), (25, 10), (75, 10), (90, 25), (90, 40), (10, 75), (10, 90), (90, 90)]
    ],

    # 3: Angular with two indents
    '3': [
        [(10, 10), (80, 10), (90, 20), (90, 40), (75, 50)],
        [(45, 50), (75, 50)],
        [(75, 50), (90, 60), (90, 80), (80, 90), (10, 90)]
    ],

    # 4: Angular with horizontal bar
    '4': [
        [(70, 10), (70, 90)],
        [(10, 60), (90, 60)],
        [(10, 60), (70, 10)]
    ],

    # 5: Angular S-like shape
    '5': [
        [(85, 10), (15, 10), (10, 15), (10, 45), (20, 50), (75, 50), (90, 60), (90, 80), (75, 90), (10, 90)]
    ],

    # 6: Rounded angular with loop at bottom
    '6': [
        [(80, 10), (25, 10), (10, 25), (10, 75), (25, 90), (75, 90), (90, 75), (90, 55), (75, 45), (10, 45)]
    ],

    # 7: Angular with distinctive slant
    '7': [
        [(10, 10), (90, 10), (90, 20), (45, 90)],
        [(30, 50), (70, 50)]
    ],

    # 8: Two stacked hexagonal loops
    '8': [
        [(25, 10), (75, 10), (90, 20), (90, 40), (75, 50), (25, 50), (10, 40), (10, 20), (25, 10)],
        [(25, 50), (75, 50), (90, 60), (90, 80), (75, 90), (25, 90), (10, 80), (10, 60), (25, 50)]
    ],

    # 9: Hexagonal with loop at top
    '9': [
        [(90, 55), (25, 55), (10, 45), (10, 25), (25, 10), (75, 10), (90, 25), (90, 75), (75, 90), (20, 90)]
    ],

    # Colon for clock display - two dots as small diamonds
    ':': [
        [(50, 25), (58, 33), (50, 41), (42, 33), (50, 25)],
        [(50, 59), (58, 67), (50, 75), (42, 67), (50, 59)]
    ],

    # Minus sign for negative numbers or separators
    '-': [
        [(15, 50), (85, 50)]
    ],

    # Period/decimal point
    '.': [
        [(50, 80), (58, 85), (50, 90), (42, 85), (50, 80)]
    ],

    # Forward slash
    '/': [
        [(85, 10), (15, 90)]
    ],

    # Degree symbol (small circle at top)
    '°': [
        [(30, 10), (70, 10), (85, 25), (85, 40), (70, 55), (30, 55), (15, 40), (15, 25), (30, 10)]
    ],

    # Percent sign
    '%': [
        [(85, 10), (15, 90)],  # Slash
        [(15, 10), (35, 10), (40, 15), (40, 30), (35, 35), (15, 35), (10, 30), (10, 15), (15, 10)],  # Top circle
        [(65, 65), (85, 65), (90, 70), (90, 85), (85, 90), (65, 90), (60, 85), (60, 70), (65, 65)]  # Bottom circle
    ],

    # Uppercase letters A-Z with angular/hexagonal style
    'A': [
        [(5, 90), (5, 75), (20, 10), (80, 10), (95, 75), (95, 90)],
        [(20, 60), (80, 60)]
    ],

    'B': [
        [(10, 10), (10, 90), (75, 90), (90, 75), (90, 55), (75, 45)],
        [(10, 45), (75, 45), (90, 35), (90, 20), (75, 10), (10, 10)]
    ],

    'C': [
        [(90, 25), (75, 10), (25, 10), (10, 25), (10, 75), (25, 90), (75, 90), (90, 75)]
    ],

    'D': [
        [(10, 10), (10, 90), (70, 90), (90, 70), (90, 30), (70, 10), (10, 10)]
    ],

    'E': [
        [(90, 10), (10, 10), (10, 90), (90, 90)],
        [(10, 50), (70, 50)]
    ],

    'F': [
        [(90, 10), (10, 10), (10, 90)],
        [(10, 50), (70, 50)]
    ],

    'G': [
        [(90, 25), (75, 10), (25, 10), (10, 25), (10, 75), (25, 90), (75, 90), (90, 75), (90, 50), (50, 50)]
    ],

    'H': [
        [(10, 10), (10, 90)],
        [(90, 10), (90, 90)],
        [(10, 50), (90, 50)]
    ],

    'I': [
        [(30, 10), (70, 10)],
        [(50, 10), (50, 90)],
        [(30, 90), (70, 90)]
    ],

    'J': [
        [(30, 10), (90, 10)],
        [(70, 10), (70, 75), (55, 90), (25, 90), (10, 75)]
    ],

    'K': [
        [(10, 10), (10, 90)],
        [(90, 10), (10, 50), (90, 90)]
    ],

    'L': [
        [(10, 10), (10, 90), (90, 90)]
    ],

    'M': [
        [(5, 90), (5, 20), (15, 10), (50, 45), (85, 10), (95, 20), (95, 90)]
    ],

    'N': [
        [(10, 90), (10, 20), (20, 10), (90, 80), (90, 10)]
    ],

    'O': [
        [(20, 10), (80, 10), (95, 25), (95, 75), (80, 90), (20, 90), (5, 75), (5, 25), (20, 10)]
    ],

    'P': [
        [(10, 90), (10, 10), (75, 10), (90, 25), (90, 40), (75, 55), (10, 55)]
    ],

    'Q': [
        [(20, 10), (80, 10), (95, 25), (95, 75), (80, 90), (20, 90), (5, 75), (5, 25), (20, 10)],
        [(60, 65), (95, 95)]
    ],

    'R': [
        [(10, 90), (10, 10), (75, 10), (90, 25), (90, 40), (75, 55), (10, 55)],
        [(55, 55), (90, 90)]
    ],

    'S': [
        [(90, 25), (75, 10), (25, 10), (10, 25), (10, 40), (25, 50), (75, 50), (90, 60), (90, 75), (75, 90), (25, 90), (10, 75)]
    ],

    'T': [
        [(10, 10), (90, 10)],
        [(50, 10), (50, 90)]
    ],

    'U': [
        [(10, 10), (10, 75), (25, 90), (75, 90), (90, 75), (90, 10)]
    ],

    'V': [
        [(5, 10), (50, 90), (95, 10)]
    ],

    'W': [
        [(5, 10), (20, 90), (50, 55), (80, 90), (95, 10)]
    ],

    'X': [
        [(10, 10), (90, 90)],
        [(90, 10), (10, 90)]
    ],

    'Y': [
        [(10, 10), (50, 50), (90, 10)],
        [(50, 50), (50, 90)]
    ],

    'Z': [
        [(10, 10), (90, 10), (10, 90), (90, 90)]
    ],
}

# Backward compatibility alias
NUMERALS = GLYPHS


def _get_char_width(char: str, char_width: int) -> int:
    """
    Get the actual width for a character.

    Args:
        char: The character
        char_width: The base character width

    Returns:
        The actual width for the character
    """
    if char == ':':
        return char_width // 2
    elif char == '.':
        return char_width // 3
    elif char == '-':
        return char_width * 2 // 3
    elif char == '/':
        return char_width // 2
    elif char == '°':
        return char_width // 3
    elif char == '%':
        return char_width
    elif char == ' ':
        return char_width // 2
    else:
        return char_width


def _scale_point(x: float, y: float,
                 dest_x: int, dest_y: int,
                 width: int, height: int) -> tuple[int, int]:
    """
    Scale a point from 0-100 unit space to destination bounding box.

    Args:
        x: X coordinate in 0-100 space
        y: Y coordinate in 0-100 space
        dest_x: Destination bounding box left edge
        dest_y: Destination bounding box top edge
        width: Destination width
        height: Destination height

    Returns:
        Tuple of (scaled_x, scaled_y) in pixel coordinates
    """
    scaled_x = dest_x + int(x * width / 100)
    scaled_y = dest_y + int(y * height / 100)
    return scaled_x, scaled_y


def _draw_thick_line(fb: Framebuffer,
                     x0: int, y0: int, x1: int, y1: int,
                     stroke_width: int, color: bool) -> None:
    """
    Draw a line with variable stroke width by drawing parallel offset lines.

    Uses a simple approach: draw the center line plus offset lines
    perpendicular to the line direction.

    Args:
        fb: Framebuffer to draw on
        x0: Starting X coordinate
        y0: Starting Y coordinate
        x1: Ending X coordinate
        y1: Ending Y coordinate
        stroke_width: Width of the stroke in pixels
        color: True for black, False for white
    """
    if stroke_width <= 1:
        draw_line(fb, x0, y0, x1, y1, color)
        return

    # Calculate line direction
    dx = x1 - x0
    dy = y1 - y0

    # Calculate perpendicular direction for offset
    # Normalize to unit length, then scale by offset
    length = (dx * dx + dy * dy) ** 0.5

    if length < 0.001:
        # Degenerate case: start and end are the same point
        # Draw a small filled area
        half = stroke_width // 2
        for ox in range(-half, half + 1):
            for oy in range(-half, half + 1):
                fb.set_pixel(x0 + ox, y0 + oy, color)
        return

    # Perpendicular direction (rotated 90 degrees)
    perp_x = -dy / length
    perp_y = dx / length

    # Draw multiple parallel lines
    half_width = (stroke_width - 1) / 2.0

    for i in range(stroke_width):
        offset = i - half_width
        ox = int(round(perp_x * offset))
        oy = int(round(perp_y * offset))
        draw_line(fb, x0 + ox, y0 + oy, x1 + ox, y1 + oy, color)


def render_numeral(
    fb: Framebuffer,
    char: str,
    x: int, y: int,
    width: int, height: int,
    stroke_width: int = 2,
    color: bool = True
) -> None:
    """
    Render a character scaled to fit bounding box.

    Args:
        fb: Framebuffer to draw on
        char: Character to render (0-9, A-Z, punctuation)
        x: Left edge of bounding box
        y: Top edge of bounding box
        width: Width of bounding box
        height: Height of bounding box
        stroke_width: Width of strokes in pixels (default 2)
        color: True for black, False for white (default True)
    """
    if char not in GLYPHS:
        return

    strokes = GLYPHS[char]

    for stroke in strokes:
        if len(stroke) < 2:
            continue

        # Scale all points
        scaled_points = [
            _scale_point(px, py, x, y, width, height)
            for px, py in stroke
        ]

        # Draw line segments between consecutive points
        for i in range(len(scaled_points) - 1):
            x0, y0 = scaled_points[i]
            x1, y1 = scaled_points[i + 1]
            _draw_thick_line(fb, x0, y0, x1, y1, stroke_width, color)


def render_string(
    fb: Framebuffer,
    text: str,
    x: int, y: int,
    char_width: int, char_height: int,
    spacing: int = 4,
    **kwargs
) -> None:
    """
    Render string of characters with spacing.

    Args:
        fb: Framebuffer to draw on
        text: String to render (should contain only supported characters)
        x: Left edge of first character bounding box
        y: Top edge of bounding box
        char_width: Width of each character bounding box
        char_height: Height of each character bounding box
        spacing: Horizontal spacing between characters (default 4)
        **kwargs: Additional arguments passed to render_numeral
                  (stroke_width, color)
    """
    current_x = x

    for char in text:
        actual_width = _get_char_width(char, char_width)

        if char in GLYPHS:
            render_numeral(fb, char, current_x, y, actual_width, char_height, **kwargs)
            current_x += actual_width + spacing
        elif char == ' ':
            # Space advances by half character width
            current_x += actual_width + spacing
        else:
            # Unknown character - skip but advance
            current_x += char_width + spacing


def get_string_width(
    text: str,
    char_width: int,
    spacing: int = 4
) -> int:
    """
    Calculate the total width of a rendered string.

    Args:
        text: String to measure
        char_width: Width of each character bounding box
        spacing: Horizontal spacing between characters (default 4)

    Returns:
        Total width in pixels
    """
    if not text:
        return 0

    total_width = 0

    for i, char in enumerate(text):
        total_width += _get_char_width(char, char_width)

        # Add spacing after each character except the last
        if i < len(text) - 1:
            total_width += spacing

    return total_width


def render_string_centered(
    fb: Framebuffer,
    text: str,
    center_x: int, y: int,
    char_width: int, char_height: int,
    spacing: int = 4,
    **kwargs
) -> None:
    """
    Render string of characters centered horizontally on center_x.

    Args:
        fb: Framebuffer to draw on
        text: String to render
        center_x: X coordinate for the center of the string
        y: Top edge of bounding box
        char_width: Width of each character bounding box
        char_height: Height of each character bounding box
        spacing: Horizontal spacing between characters (default 4)
        **kwargs: Additional arguments passed to render_numeral
                  (stroke_width, color)
    """
    string_width = get_string_width(text, char_width, spacing)
    x = center_x - string_width // 2
    render_string(fb, text, x, y, char_width, char_height, spacing, **kwargs)


def render_string_right(
    fb: Framebuffer,
    text: str,
    right_x: int, y: int,
    char_width: int, char_height: int,
    spacing: int = 4,
    **kwargs
) -> None:
    """
    Render string of characters right-aligned at right_x.

    Args:
        fb: Framebuffer to draw on
        text: String to render
        right_x: X coordinate for the right edge of the string
        y: Top edge of bounding box
        char_width: Width of each character bounding box
        char_height: Height of each character bounding box
        spacing: Horizontal spacing between characters (default 4)
        **kwargs: Additional arguments passed to render_numeral
                  (stroke_width, color)
    """
    string_width = get_string_width(text, char_width, spacing)
    x = right_x - string_width
    render_string(fb, text, x, y, char_width, char_height, spacing, **kwargs)


def render_multiline(
    fb: Framebuffer,
    lines: list[str],
    x: int, y: int,
    char_width: int, char_height: int,
    line_spacing: int = 8,
    align: str = "left",
    **kwargs
) -> None:
    """
    Render multiple lines of text with vertical spacing.

    Args:
        fb: Framebuffer to draw on
        lines: List of strings to render
        x: X coordinate (left edge for "left", center for "center", right edge for "right")
        y: Top edge of first line
        char_width: Width of each character bounding box
        char_height: Height of each character bounding box
        line_spacing: Vertical spacing between lines (default 8)
        align: Text alignment - "left", "center", or "right" (default "left")
        **kwargs: Additional arguments passed to render_numeral
                  (stroke_width, color, spacing)
    """
    spacing = kwargs.pop('spacing', 4)
    current_y = y

    for line in lines:
        if align == "center":
            render_string_centered(fb, line, x, current_y, char_width, char_height,
                                   spacing=spacing, **kwargs)
        elif align == "right":
            render_string_right(fb, line, x, current_y, char_width, char_height,
                                spacing=spacing, **kwargs)
        else:  # left
            render_string(fb, line, x, current_y, char_width, char_height,
                          spacing=spacing, **kwargs)

        current_y += char_height + line_spacing
