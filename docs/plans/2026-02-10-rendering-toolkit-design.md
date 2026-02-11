# 1-Bit Rendering Toolkit Design

A portable rendering toolkit inspired by Lucas Pope's Mars After Midnight techniques, designed for a 400×300 1-bit reflective LCD clock on ESP32-S3.

## Goals

1. Build reusable rendering primitives that work in Python (simulator) and port cleanly to C++ (ESP32)
2. Implement MVP versions of Pope's visual techniques: vector typography, bezier curves with texture-ball strokes, and dither patterns
3. Replace the existing simulator display system with a richer, more portable architecture

## Architecture

```
simulator/
├── main.py                    # Demo runner / entry point
├── rendering/
│   ├── __init__.py           # Public API exports
│   ├── framebuffer.py        # Byte-buffer core (ESP32-portable)
│   ├── primitives.py         # Lines, polygons, circles
│   ├── patterns.py           # 5 Bayer dither patterns + pattern-filled polygons
│   ├── bezier.py             # Cubic beziers, auto-tangent, texture-ball stroke
│   ├── vector_font.py        # Geometric numeral definitions + renderer
│   └── display.py            # Pygame visualization wrapper (not portable)
├── demo.py                    # Interactive toolkit showcase
├── data_provider.py           # Time/sensor data (kept from existing)
└── requirements.txt
```

### Portability Separation

**Portable core** (pure Python, no pygame, operates on `bytearray`):
- `framebuffer.py`
- `primitives.py`
- `patterns.py`
- `bezier.py`
- `vector_font.py`

**Visualization layer** (pygame-specific):
- `display.py`

### Files to Remove

- `simulator/clock.py`
- `simulator/hex_segments.py`
- `simulator/display.py` (replaced by `rendering/display.py`)

## Module Specifications

### framebuffer.py

Byte-buffer foundation with ESP32-compatible memory layout.

```python
class Framebuffer:
    WIDTH = 400
    HEIGHT = 300
    BYTES_PER_ROW = 50  # 400 / 8

    def __init__(self):
        self.buffer = bytearray(self.BYTES_PER_ROW * self.HEIGHT)

    def clear(self, color: bool = False) -> None:
        """Fill entire buffer. color=True for black, False for white."""

    def set_pixel(self, x: int, y: int, color: bool) -> None:
        """Set single pixel. Bounds-checked."""

    def get_pixel(self, x: int, y: int) -> bool:
        """Read single pixel. Returns False if out of bounds."""

    def fill_span(self, y: int, x_start: int, x_end: int, color: bool) -> None:
        """Optimized horizontal span fill using byte operations."""
```

**Framebuffer format:**
- 400×300 pixels, 1-bit packed
- 50 bytes per row, 15,000 bytes total
- Bit 7 = leftmost pixel in each byte (matches ESP32 convention)
- `True`/`1` = black, `False`/`0` = white

**Key optimization:** `fill_span()` writes full bytes where possible (8× faster than pixel-by-pixel), handling partial bytes at span edges.

### primitives.py

Geometric drawing functions operating on a `Framebuffer`.

```python
def draw_line(fb: Framebuffer, x0: int, y0: int, x1: int, y1: int, color: bool) -> None:
    """Bresenham line algorithm."""

def draw_polygon(fb: Framebuffer, points: list[tuple[int, int]], color: bool) -> None:
    """Draw polygon outline as connected line segments."""

def fill_polygon(fb: Framebuffer, points: list[tuple[int, int]], color: bool) -> None:
    """Scanline fill algorithm using optimized fill_span."""

def fill_rect(fb: Framebuffer, x: int, y: int, w: int, h: int, color: bool) -> None:
    """Axis-aligned rectangle fill using fill_span."""

def draw_circle(fb: Framebuffer, cx: int, cy: int, r: int, color: bool) -> None:
    """Midpoint circle algorithm for outline."""

def fill_circle(fb: Framebuffer, cx: int, cy: int, r: int, color: bool) -> None:
    """Filled circle using horizontal spans."""
```

### patterns.py

5 Bayer-based ordered dither patterns with pattern-filled polygon rendering.

```python
# 4x4 Bayer matrix (values 0-15, threshold levels)
BAYER_4X4 = [
    [ 0,  8,  2, 10],
    [12,  4, 14,  6],
    [ 3, 11,  1,  9],
    [15,  7, 13,  5],
]

class Pattern:
    SOLID_BLACK = 0   # 100% fill
    DENSE = 1         # ~75% fill (threshold 4)
    MEDIUM = 2        # ~50% fill (threshold 8)
    SPARSE = 3        # ~25% fill (threshold 12)
    SOLID_WHITE = 4   # 0% fill

def pattern_test(pattern: int, x: int, y: int) -> bool:
    """Returns True (black) if pixel should be filled for given pattern."""

def fill_polygon_pattern(fb: Framebuffer, points: list[tuple[int, int]], pattern: int) -> None:
    """Scanline fill with pattern mask. Pattern tiles from global (0,0)."""
```

**Pattern behavior:** Patterns tile infinitely from global origin (0,0). Polygons act as clip masks.

### bezier.py

Cubic bezier curves with Pope's auto-smooth tangent technique and texture-ball strokes.

```python
def auto_tangent(points: list[tuple[float, float]], smoothness: float = 0.5) -> list[tuple[tuple, tuple]]:
    """Generate control point handles for smooth curve through points.

    Returns list of (handle_in, handle_out) for each point.
    Smoothness 0.0 = sharp corners, 1.0 = maximum smoothing.

    For each point, tangent direction = normalized(next - prev).
    Handle length = smoothness * distance to neighbors.
    """

def cubic_bezier(p0, p1, p2, p3, t: float) -> tuple[float, float]:
    """Evaluate cubic bezier at parameter t (0-1)."""

def subdivide_bezier(p0, c0, c1, p1, tolerance: float = 1.0) -> list[tuple[int, int]]:
    """Adaptively subdivide bezier into line segments.

    tolerance = max pixel deviation allowed. 1.0 is good for 1-bit.
    Returns list of integer (x, y) points.
    """

def stroke_bezier_texture_ball(
    fb: Framebuffer,
    points: list[tuple[float, float]],
    smoothness: float,
    ball_texture: list[list[bool]],  # Small 2D bitmap, e.g. 8x8
    spacing: float = 2.0
) -> None:
    """Pope's texture-ball stroke technique.

    1. Generate bezier path through points with auto-tangents
    2. Walk path at 'spacing' pixel intervals
    3. At each step, splat ball_texture rotated to path tangent
    """
```

**Texture ball format:** A small square bitmap (8×8 suggested) with irregular, hand-drawn edges. Rotation aligns the texture to the curve tangent at each splat point.

### vector_font.py

Geometric numerals defined as line segment paths.

```python
# Each numeral is a list of strokes. Each stroke is a list of (x, y) points.
# Coordinates in a 0-100 unit square, scaled at render time.
NUMERALS = {
    '0': [[(10, 0), (90, 0), (90, 100), (10, 100), (10, 0)]],
    '1': [[(50, 0), (50, 100)]],
    '2': [[(10, 0), (90, 0), (90, 50), (10, 50), (10, 100), (90, 100)]],
    # ... 3-9
    ':': [[(50, 30), (50, 30)], [(50, 70), (50, 70)]],  # Two dots
}

def render_numeral(
    fb: Framebuffer,
    char: str,
    x: int, y: int,
    width: int, height: int,
    stroke_width: int = 2,
    color: bool = True
) -> None:
    """Render a numeral scaled to fit bounding box."""

def render_string(
    fb: Framebuffer,
    text: str,
    x: int, y: int,
    char_width: int, char_height: int,
    spacing: int = 4,
    **kwargs
) -> None:
    """Render string of numerals with spacing."""
```

**Design notes:**
- Coordinates in 0-100 unit space for resolution independence
- Stroke width via parallel offset lines or thin polygon fill
- Numeral designs: angular/geometric (straight segments only)

### rendering/display.py

Pygame visualization wrapper.

```python
class Display:
    def __init__(self, fb: Framebuffer, scale: int = 2):
        """Initialize pygame window at framebuffer size × scale."""

    def render(self) -> None:
        """Blit framebuffer to pygame screen."""

    def save_screenshot(self, path: str) -> None:
        """Save current screen to image file."""

    def handle_events(self) -> bool:
        """Process events. Returns False if quit requested."""
```

### demo.py

Interactive showcase with 4 modes (cycle with spacebar):

1. **Patterns** — Show all 5 dither patterns in hexagonal shapes
2. **Bezier** — Organic shapes with texture-ball stroke outlines
3. **Numerals** — Full 0-9 character set display
4. **Clock sketch** — Composition preview combining all techniques

```python
class ToolkitDemo:
    def __init__(self, fb: Framebuffer):
        self.fb = fb
        self.mode = 0

    def next_mode(self):
        self.mode = (self.mode + 1) % 4

    def draw(self):
        self.fb.clear()
        if self.mode == 0:
            self._demo_patterns()
        elif self.mode == 1:
            self._demo_bezier()
        elif self.mode == 2:
            self._demo_numerals()
        elif self.mode == 3:
            self._demo_clock_sketch()
```

## Public API

```python
# rendering/__init__.py
from .framebuffer import Framebuffer
from .primitives import draw_line, draw_polygon, fill_polygon, fill_rect, fill_circle, draw_circle
from .patterns import Pattern, pattern_test, fill_polygon_pattern
from .bezier import auto_tangent, subdivide_bezier, stroke_bezier_texture_ball
from .vector_font import render_numeral, render_string, NUMERALS
from .display import Display
```

## Estimated Scope

| Module | Lines (est.) |
|--------|--------------|
| `framebuffer.py` | ~80 |
| `primitives.py` | ~150 |
| `patterns.py` | ~80 |
| `bezier.py` | ~120 |
| `vector_font.py` | ~100 |
| `display.py` | ~60 |
| `demo.py` | ~120 |
| `main.py` | ~40 |
| **Total** | **~750** |

## ESP32 Portability Notes

The portable core modules use only Python builtins (`bytearray`, `list`, `int`, `float`, basic math). Porting to C++:

- `Framebuffer` → `uint8_t buffer[15000]` with same bit-packing
- `fill_span()` → byte-aligned memset with edge masking
- Polygon scanline fill → same algorithm
- Bezier subdivision → same De Casteljau algorithm
- Pattern test → same Bayer matrix lookup

The ESP32's existing `RLCD_SetPixel` and `RLCD_FillRect` map to `set_pixel` and `fill_rect`. Higher-level primitives (polygon fill, bezier stroke) would be new additions to the firmware.

## Design Influences

From Lucas Pope's Mars After Midnight:
- **Vector typography** with line-based glyphs, not bitmaps
- **Texture-ball strokes** for organic, hand-drawn outlines
- **Minimal dithering** — patterns as materials, not grayscale simulation
- **Auto-smooth bezier tangents** for procedural organic shapes
- **High contrast** — solid black/white regions dominate

Applied constraints:
- Geometric/angular numerals (straight segments only)
- 5 fixed Bayer patterns (no hand-designed artwork required for MVP)
- Portable byte-buffer design targeting ESP32 from the start
