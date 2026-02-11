"""Rendering toolkit for 1-bit display simulator."""

from .framebuffer import Framebuffer
from .primitives import (
    draw_line,
    draw_polygon,
    fill_polygon,
    fill_rect,
    draw_circle,
    fill_circle,
)
from .patterns import (
    BAYER_4X4,
    Pattern,
    pattern_test,
    fill_polygon_pattern,
)
from .bezier import (
    auto_tangent,
    cubic_bezier,
    cubic_bezier_derivative,
    subdivide_bezier,
    stroke_bezier_texture_ball,
    draw_bezier_curve,
    DEFAULT_BALL_8X8,
)
from .vector_font import (
    GLYPHS,
    NUMERALS,
    render_numeral,
    render_string,
    render_string_centered,
    render_string_right,
    render_multiline,
    get_string_width,
)
from .animation import (
    lerp,
    ease_in_out,
    ease_in_out_sine,
    breathing_scale,
    breathing_offset,
    wiggle_points,
    wiggle_int_points,
    transition_points,
    AnimationState,
)

# Display is optional - only imported if pygame is available
try:
    from .display import Display
    _HAS_PYGAME = True
except ImportError:
    Display = None  # type: ignore
    _HAS_PYGAME = False

__all__ = [
    "Framebuffer",
    "draw_line",
    "draw_polygon",
    "fill_polygon",
    "fill_rect",
    "draw_circle",
    "fill_circle",
    "BAYER_4X4",
    "Pattern",
    "pattern_test",
    "fill_polygon_pattern",
    "auto_tangent",
    "cubic_bezier",
    "cubic_bezier_derivative",
    "subdivide_bezier",
    "stroke_bezier_texture_ball",
    "draw_bezier_curve",
    "DEFAULT_BALL_8X8",
    "GLYPHS",
    "NUMERALS",
    "render_numeral",
    "render_string",
    "render_string_centered",
    "render_string_right",
    "render_multiline",
    "get_string_width",
    "Display",
    # Animation functions
    "lerp",
    "ease_in_out",
    "ease_in_out_sine",
    "breathing_scale",
    "breathing_offset",
    "wiggle_points",
    "wiggle_int_points",
    "transition_points",
    "AnimationState",
]
