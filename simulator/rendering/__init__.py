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
]
