#!/usr/bin/env python3
"""
Interactive toolkit demo for RLCD rendering engine.

Provides a 5-mode showcase of the rendering toolkit capabilities:
1. Patterns - Dither pattern showcase in hexagonal shapes
2. Bezier - Organic curves with texture-ball strokes
3. Numerals - Full digit set at various sizes
4. Clock sketch - Composition preview combining all features
5. Typography - Full alphabet and text samples

Usage:
    python demo.py [--scale N]

Controls:
    SPACE  - Cycle to next demo mode
    1-5    - Jump to specific mode
    A      - Toggle animation
    Q/ESC  - Quit
"""

import math
import time
from datetime import datetime

from rendering import (
    Framebuffer,
    Pattern,
    fill_polygon_pattern,
    stroke_bezier_texture_ball,
    DEFAULT_BALL_8X8,
    render_string,
    get_string_width,
    draw_polygon,
    fill_polygon,
    draw_line,
)


def generate_hexagon(cx: int, cy: int, radius: int) -> list[tuple[int, int]]:
    """Generate hexagon vertices centered at (cx, cy) with given radius."""
    points = []
    for i in range(6):
        angle = math.pi / 6 + i * math.pi / 3  # Start at 30 degrees for flat-top
        x = int(cx + radius * math.cos(angle))
        y = int(cy + radius * math.sin(angle))
        points.append((x, y))
    return points


def generate_rounded_rect_points(
    x: int, y: int, w: int, h: int, corner: int
) -> list[tuple[float, float]]:
    """Generate points for a rounded rectangle path for bezier curves."""
    points = []
    # Start from top-left corner, go clockwise
    points.append((float(x + corner), float(y)))  # Top edge start
    points.append((float(x + w - corner), float(y)))  # Top edge end
    points.append((float(x + w), float(y + corner)))  # Right edge start
    points.append((float(x + w), float(y + h - corner)))  # Right edge end
    points.append((float(x + w - corner), float(y + h)))  # Bottom edge end
    points.append((float(x + corner), float(y + h)))  # Bottom edge start
    points.append((float(x), float(y + h - corner)))  # Left edge end
    points.append((float(x), float(y + corner)))  # Left edge start
    points.append((float(x + corner), float(y)))  # Close loop
    return points


class ToolkitDemo:
    """Interactive 5-mode toolkit demonstration."""

    MODE_PATTERNS = 0
    MODE_BEZIER = 1
    MODE_NUMERALS = 2
    MODE_CLOCK = 3
    MODE_TYPOGRAPHY = 4

    MODE_NAMES = ["Patterns", "Bezier Curves", "Numerals", "Clock Sketch", "Typography"]

    def __init__(self, fb: Framebuffer):
        """
        Initialize the demo.

        Args:
            fb: Framebuffer to render to
        """
        self.fb = fb
        self.mode = 0
        self.animation_enabled = True
        self.frame = 0

    def next_mode(self) -> None:
        """Cycle to the next demo mode."""
        self.mode = (self.mode + 1) % 5

    def set_mode(self, mode: int) -> None:
        """Set a specific demo mode (0-4)."""
        if 0 <= mode < 5:
            self.mode = mode

    def toggle_animation(self) -> bool:
        """Toggle animation on/off and return new state."""
        self.animation_enabled = not self.animation_enabled
        return self.animation_enabled

    def _breathing_scale(self, speed=1.0, min_val=0.95, max_val=1.05) -> float:
        """Calculate breathing scale based on frame count."""
        t = self.frame * speed * 0.05  # Convert frames to time-like value
        factor = (math.sin(t) + 1) / 2  # 0 to 1
        return min_val + factor * (max_val - min_val)

    def _wiggle_offset(self, seed=0, amplitude=2.0, frequency=1.0) -> tuple[float, float]:
        """Calculate wiggle offset based on frame count and seed."""
        t = self.frame * 0.1 * frequency
        phase_x = seed * 2.39996  # Golden angle for variety
        phase_y = seed * 1.61803
        dx = amplitude * math.sin(t + phase_x)
        dy = amplitude * math.cos(t + phase_y)
        return (dx, dy)

    def get_mode_name(self) -> str:
        """Get the name of the current mode."""
        return self.MODE_NAMES[self.mode]

    def draw(self) -> None:
        """Draw the current demo mode."""
        self.fb.clear()
        self.frame += 1  # Increment each frame

        if self.mode == 0:
            self._demo_patterns()
        elif self.mode == 1:
            self._demo_bezier()
        elif self.mode == 2:
            self._demo_numerals()
        elif self.mode == 3:
            self._demo_clock_sketch()
        elif self.mode == 4:
            self._demo_typography()

        # Draw mode indicator at bottom
        self._draw_mode_label()

    def _draw_mode_label(self) -> None:
        """Draw current mode name at bottom of screen."""
        # Draw a small label showing current mode
        label = f"Mode {self.mode + 1}: {self.MODE_NAMES[self.mode]}"
        # Use simple pixel text representation (crude but visible)
        # Position at bottom-left
        y = self.fb.HEIGHT - 12
        x = 10

        # Draw a small underline to indicate active mode
        draw_line(self.fb, x, y + 10, x + len(label) * 6, y + 10, True)

    def _demo_patterns(self) -> None:
        """
        Demo mode 1: Dither patterns showcase.

        Displays all 5 pattern levels (SOLID_BLACK, DENSE, MEDIUM, SPARSE, SOLID_WHITE)
        in hexagonal shapes arranged across the screen. Each hexagon is labeled
        with its pattern type.
        """
        patterns = [
            (Pattern.SOLID_BLACK, "BLACK"),
            (Pattern.DENSE, "DENSE"),
            (Pattern.MEDIUM, "MEDIUM"),
            (Pattern.SPARSE, "SPARSE"),
            (Pattern.SOLID_WHITE, "WHITE"),
        ]

        # Calculate hexagon layout
        base_hex_radius = 45
        if self.animation_enabled:
            breathing = self._breathing_scale(speed=0.8, min_val=0.95, max_val=1.05)
            hex_radius = int(base_hex_radius * breathing)
        else:
            hex_radius = base_hex_radius
        spacing = base_hex_radius * 2 + 20
        start_x = 70
        center_y = self.fb.HEIGHT // 2 - 20

        for i, (pattern, label) in enumerate(patterns):
            cx = start_x + i * spacing // len(patterns) * len(patterns)
            # Recalculate for even spacing
            cx = start_x + i * (self.fb.WIDTH - 2 * start_x) // (len(patterns) - 1)

            # Generate and fill hexagon with pattern
            hex_points = generate_hexagon(cx, center_y, hex_radius)

            # Draw outline first (will be visible for SOLID_WHITE)
            draw_polygon(self.fb, hex_points, True)

            # Fill with pattern
            fill_polygon_pattern(self.fb, hex_points, pattern)

            # Draw label below - crude text using lines
            label_y = center_y + hex_radius + 20
            # Draw pattern level number
            render_string(
                self.fb,
                str(i),
                cx - 8,
                label_y,
                char_width=16,
                char_height=20,
                stroke_width=2,
            )

        # Draw title at top
        title_text = "PATTERN LEVELS 0-4"
        title_width = get_string_width(title_text, char_width=18)
        title_x = (self.fb.WIDTH - title_width) // 2
        render_string(
            self.fb,
            "0-4",
            title_x + 150,
            20,
            char_width=18,
            char_height=24,
            stroke_width=2,
        )

    def _demo_bezier(self) -> None:
        """
        Demo mode 2: Bezier curves with texture-ball strokes.

        Draws organic shapes and curves using the texture-ball stroke technique.
        Shows flowing curves, decorative borders, and organic forms.
        """
        # Draw several bezier curves showcasing the organic stroke style

        # 1. Flowing S-curve across the top
        base_s_curve_points = [
            (50.0, 80.0),
            (130.0, 40.0),
            (200.0, 120.0),
            (280.0, 60.0),
            (350.0, 100.0),
        ]
        if self.animation_enabled:
            s_curve_points = []
            for idx, (x, y) in enumerate(base_s_curve_points):
                dx, dy = self._wiggle_offset(seed=idx, amplitude=3.0)
                s_curve_points.append((x + dx, y + dy))
        else:
            s_curve_points = base_s_curve_points
        stroke_bezier_texture_ball(
            self.fb,
            s_curve_points,
            smoothness=0.6,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=3.0,
        )

        # 2. Organic rounded frame in the center
        frame_points = generate_rounded_rect_points(80, 130, 240, 100, 25)
        stroke_bezier_texture_ball(
            self.fb,
            frame_points,
            smoothness=0.7,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.5,
        )

        # 3. Decorative wave pattern at bottom
        wave_points = []
        for i in range(8):
            x = 30.0 + i * 50
            y = 260.0 + math.sin(i * 0.8) * 20
            wave_points.append((x, y))

        stroke_bezier_texture_ball(
            self.fb,
            wave_points,
            smoothness=0.8,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.0,
        )

        # 4. Spiral curve on the left
        spiral_points = []
        for i in range(20):
            angle = i * 0.4
            radius = 10 + i * 2.5
            x = 50.0 + radius * math.cos(angle)
            y = 200.0 + radius * math.sin(angle)
            spiral_points.append((x, y))

        stroke_bezier_texture_ball(
            self.fb,
            spiral_points,
            smoothness=0.5,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=3.0,
        )

        # 5. Organic blob on the right
        blob_points = []
        blob_cx, blob_cy = 340.0, 200.0
        for i in range(12):
            angle = i * math.pi * 2 / 12
            # Vary radius for organic shape
            radius = 35 + 15 * math.sin(angle * 3) + 10 * math.cos(angle * 2)
            x = blob_cx + radius * math.cos(angle)
            y = blob_cy + radius * math.sin(angle)
            blob_points.append((x, y))
        blob_points.append(blob_points[0])  # Close the loop

        stroke_bezier_texture_ball(
            self.fb,
            blob_points,
            smoothness=0.6,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.0,
        )

    def _demo_numerals(self) -> None:
        """
        Demo mode 3: Vector font numerals showcase.

        Displays the full 0-9 character set at various sizes to demonstrate
        the scalable vector font rendering.
        """
        # Row 1: Large numerals (0-4)
        render_string(
            self.fb,
            "0123456789",
            10,
            20,
            char_width=35,
            char_height=50,
            spacing=4,
            stroke_width=3,
        )

        # Row 2: Medium numerals (full set)
        render_string(
            self.fb,
            "0123456789",
            20,
            85,
            char_width=28,
            char_height=40,
            spacing=4,
            stroke_width=2,
        )

        # Row 3: Clock time display format
        now = datetime.now()
        time_str = now.strftime("%H:%M")
        time_width = get_string_width(time_str, char_width=40, spacing=6)
        time_x = (self.fb.WIDTH - time_width) // 2
        render_string(
            self.fb,
            time_str,
            time_x,
            140,
            char_width=40,
            char_height=55,
            spacing=6,
            stroke_width=3,
        )

        # Row 4: Smaller numerals in different arrangements
        render_string(
            self.fb,
            "12:34:56",
            30,
            210,
            char_width=24,
            char_height=32,
            spacing=3,
            stroke_width=2,
        )

        render_string(
            self.fb,
            "98.76",
            230,
            210,
            char_width=24,
            char_height=32,
            spacing=3,
            stroke_width=2,
        )

        # Row 5: Tiny numerals
        render_string(
            self.fb,
            "0123456789",
            50,
            260,
            char_width=18,
            char_height=24,
            spacing=2,
            stroke_width=1,
        )

    def _demo_clock_sketch(self) -> None:
        """
        Demo mode 4: Clock composition preview.

        Combines all toolkit features into a clock face composition:
        - Time display with vector numerals
        - Pattern-filled decorative containers
        - Bezier curve organic borders
        """
        # Current time
        now = datetime.now()
        time_str = now.strftime("%H:%M")

        # Draw main time display area with organic bezier border
        main_frame = generate_rounded_rect_points(60, 70, 280, 100, 20)
        stroke_bezier_texture_ball(
            self.fb,
            main_frame,
            smoothness=0.65,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.5,
        )

        # Render time centered in the frame (with optional wiggle)
        time_width = get_string_width(time_str, char_width=48, spacing=8)
        time_x = 60 + (280 - time_width) // 2
        time_y = 90
        if self.animation_enabled:
            dx, dy = self._wiggle_offset(seed=0, amplitude=2.0, frequency=0.5)
            time_x = int(time_x + dx)
            time_y = int(time_y + dy)
        render_string(
            self.fb,
            time_str,
            time_x,
            time_y,
            char_width=48,
            char_height=65,
            spacing=8,
            stroke_width=3,
        )

        # Draw decorative hexagonal containers with patterns on sides
        # Calculate breathing radius for decorative hexagons
        base_side_radius = 28
        if self.animation_enabled:
            breathing = self._breathing_scale(speed=0.6, min_val=0.92, max_val=1.08)
            side_radius = int(base_side_radius * breathing)
        else:
            side_radius = base_side_radius

        # Left hexagon with DENSE pattern
        left_hex = generate_hexagon(35, 120, side_radius)
        draw_polygon(self.fb, left_hex, True)
        fill_polygon_pattern(self.fb, left_hex, Pattern.DENSE)

        # Right hexagon with MEDIUM pattern
        right_hex = generate_hexagon(365, 120, side_radius)
        draw_polygon(self.fb, right_hex, True)
        fill_polygon_pattern(self.fb, right_hex, Pattern.MEDIUM)

        # Bottom info panel with bezier frame
        info_frame = generate_rounded_rect_points(100, 195, 200, 55, 12)
        stroke_bezier_texture_ball(
            self.fb,
            info_frame,
            smoothness=0.6,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=3.0,
        )

        # Draw seconds and date info
        seconds_str = now.strftime(":%S")
        render_string(
            self.fb,
            seconds_str,
            140,
            205,
            char_width=24,
            char_height=35,
            spacing=3,
            stroke_width=2,
        )

        # Date as day number
        day_str = now.strftime("%d")
        render_string(
            self.fb,
            day_str,
            220,
            205,
            char_width=24,
            char_height=35,
            spacing=3,
            stroke_width=2,
        )

        # Decorative corner elements with small hexagons and patterns
        # Calculate breathing radius for corner hexagons
        base_corner_radius = 18
        if self.animation_enabled:
            corner_breathing = self._breathing_scale(speed=1.0, min_val=0.9, max_val=1.1)
            corner_radius = int(base_corner_radius * corner_breathing)
        else:
            corner_radius = base_corner_radius

        # Top-left corner
        tl_hex = generate_hexagon(25, 25, corner_radius)
        draw_polygon(self.fb, tl_hex, True)
        fill_polygon_pattern(self.fb, tl_hex, Pattern.SPARSE)

        # Top-right corner
        tr_hex = generate_hexagon(375, 25, corner_radius)
        draw_polygon(self.fb, tr_hex, True)
        fill_polygon_pattern(self.fb, tr_hex, Pattern.SPARSE)

        # Decorative bezier flourishes in corners
        bottom_left_flourish = [
            (10.0, 280.0),
            (30.0, 260.0),
            (60.0, 275.0),
        ]
        stroke_bezier_texture_ball(
            self.fb,
            bottom_left_flourish,
            smoothness=0.7,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.0,
        )

        bottom_right_flourish = [
            (340.0, 275.0),
            (370.0, 260.0),
            (390.0, 280.0),
        ]
        stroke_bezier_texture_ball(
            self.fb,
            bottom_right_flourish,
            smoothness=0.7,
            ball_texture=DEFAULT_BALL_8X8,
            spacing=2.0,
        )

        # Small SOLID_BLACK accent hexagons at bottom
        bl_hex = generate_hexagon(25, 265, 12)
        fill_polygon(self.fb, bl_hex, True)

        br_hex = generate_hexagon(375, 265, 12)
        fill_polygon(self.fb, br_hex, True)

    def _demo_typography(self) -> None:
        """
        Demo mode 5: Typography showcase.

        Displays the full alphabet and various text samples:
        - Full A-Z in two rows
        - Sample phrase "THE QUICK BROWN FOX"
        - Day abbreviations
        - Month abbreviations
        - Mixed display with date/time/weather format
        """
        y_offset = 10

        # Row 1: A-M
        render_string(
            self.fb,
            "ABCDEFGHIJKLM",
            10,
            y_offset,
            char_width=28,
            char_height=36,
            spacing=2,
            stroke_width=2,
        )
        y_offset += 42

        # Row 2: N-Z
        render_string(
            self.fb,
            "NOPQRSTUVWXYZ",
            10,
            y_offset,
            char_width=28,
            char_height=36,
            spacing=2,
            stroke_width=2,
        )
        y_offset += 48

        # Sample phrase
        render_string(
            self.fb,
            "THE QUICK BROWN FOX",
            10,
            y_offset,
            char_width=18,
            char_height=24,
            spacing=2,
            stroke_width=2,
        )
        y_offset += 32

        # Day abbreviations
        render_string(
            self.fb,
            "SUN MON TUE WED THU FRI SAT",
            10,
            y_offset,
            char_width=12,
            char_height=16,
            spacing=2,
            stroke_width=1,
        )
        y_offset += 24

        # Month abbreviations - row 1
        render_string(
            self.fb,
            "JAN FEB MAR APR MAY JUN",
            10,
            y_offset,
            char_width=12,
            char_height=16,
            spacing=2,
            stroke_width=1,
        )
        y_offset += 22

        # Month abbreviations - row 2
        render_string(
            self.fb,
            "JUL AUG SEP OCT NOV DEC",
            10,
            y_offset,
            char_width=12,
            char_height=16,
            spacing=2,
            stroke_width=1,
        )
        y_offset += 28

        # Mixed display: date/time/temperature format
        render_string(
            self.fb,
            "MON 02/10 72F 45%",
            20,
            y_offset,
            char_width=20,
            char_height=28,
            spacing=3,
            stroke_width=2,
        )


def run_demo(scale: int = 2) -> int:
    """
    Run the interactive demo.

    Args:
        scale: Window scale factor (default 2)

    Returns:
        Exit code (0 for success)
    """
    try:
        import pygame
        from rendering import Display
    except ImportError as e:
        print(f"Error: pygame is required for the demo. {e}")
        return 1

    # Initialize framebuffer and display (display takes reference to fb)
    fb = Framebuffer()
    display = Display(fb, scale=scale)

    demo = ToolkitDemo(fb)

    print("RLCD Toolkit Demo")
    print(f"Display: {fb.WIDTH}x{fb.HEIGHT} (scale: {scale}x)")
    print("\nControls:")
    print("  SPACE  - Next mode")
    print("  1-5    - Jump to mode")
    print("  A      - Toggle animation")
    print("  S      - Save screenshot")
    print("  Q/ESC  - Quit")
    print(f"\nStarting in mode: {demo.get_mode_name()}")

    running = True

    try:
        while running:
            # Handle events
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.KEYDOWN:
                    if event.key == pygame.K_ESCAPE or event.key == pygame.K_q:
                        running = False
                    elif event.key == pygame.K_SPACE:
                        demo.next_mode()
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_1:
                        demo.set_mode(0)
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_2:
                        demo.set_mode(1)
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_3:
                        demo.set_mode(2)
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_4:
                        demo.set_mode(3)
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_5:
                        demo.set_mode(4)
                        print(f"Mode: {demo.get_mode_name()}")
                    elif event.key == pygame.K_a:
                        enabled = demo.toggle_animation()
                        print(f"Animation: {'ON' if enabled else 'OFF'}")
                    elif event.key == pygame.K_s:
                        filename = f"demo_mode{demo.mode + 1}_{int(time.time())}.png"
                        display.save_screenshot(filename)
                        print(f"Saved: {filename}")

            # Draw current demo (updates framebuffer)
            demo.draw()

            # Render framebuffer to display
            display.render()

            # Cap frame rate
            pygame.time.delay(50)

    except KeyboardInterrupt:
        print("\nInterrupted")
    finally:
        display.close()

    return 0


def main():
    """Entry point with argument parsing."""
    import argparse

    parser = argparse.ArgumentParser(description="RLCD Toolkit Demo")
    parser.add_argument("--scale", type=int, default=2, help="Window scale factor")
    args = parser.parse_args()

    return run_demo(scale=args.scale)


if __name__ == "__main__":
    import sys

    sys.exit(main())
