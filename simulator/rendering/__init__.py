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
]
