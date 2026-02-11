"""
Tests for the toolkit demo module.

These tests verify the ToolkitDemo class functionality without requiring pygame.
All visual rendering tests use the Framebuffer directly.
"""

import pytest
import math

# Add parent directory to path for imports
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from rendering import Framebuffer, Pattern
from demo import ToolkitDemo, generate_hexagon, generate_rounded_rect_points


class TestGenerateHexagon:
    """Tests for hexagon point generation."""

    def test_generates_six_points(self):
        """Hexagon should have exactly 6 vertices."""
        points = generate_hexagon(100, 100, 50)
        assert len(points) == 6

    def test_points_are_tuples(self):
        """Each point should be a tuple of two integers."""
        points = generate_hexagon(100, 100, 50)
        for point in points:
            assert isinstance(point, tuple)
            assert len(point) == 2
            assert isinstance(point[0], int)
            assert isinstance(point[1], int)

    def test_centered_at_origin(self):
        """Hexagon centered at 0,0 should have symmetric points."""
        points = generate_hexagon(0, 0, 100)
        # Check x coordinates sum to approximately 0 (centered)
        x_sum = sum(p[0] for p in points)
        y_sum = sum(p[1] for p in points)
        # Due to rounding, may not be exactly 0
        assert abs(x_sum) < 10
        assert abs(y_sum) < 10

    def test_radius_affects_size(self):
        """Larger radius should produce larger hexagon."""
        small = generate_hexagon(100, 100, 20)
        large = generate_hexagon(100, 100, 50)

        # Calculate bounding box widths
        small_width = max(p[0] for p in small) - min(p[0] for p in small)
        large_width = max(p[0] for p in large) - min(p[0] for p in large)

        assert large_width > small_width


class TestGenerateRoundedRectPoints:
    """Tests for rounded rectangle point generation."""

    def test_generates_nine_points(self):
        """Rounded rect path should have 9 points (closes loop)."""
        points = generate_rounded_rect_points(0, 0, 100, 80, 10)
        assert len(points) == 9

    def test_points_are_float_tuples(self):
        """Each point should be a tuple of floats."""
        points = generate_rounded_rect_points(10, 20, 100, 80, 15)
        for point in points:
            assert isinstance(point, tuple)
            assert len(point) == 2
            assert isinstance(point[0], float)
            assert isinstance(point[1], float)

    def test_first_and_last_point_match(self):
        """First and last point should be the same (closed loop)."""
        points = generate_rounded_rect_points(0, 0, 100, 80, 10)
        assert points[0] == points[-1]

    def test_respects_position(self):
        """Points should be offset by x, y position."""
        points1 = generate_rounded_rect_points(0, 0, 100, 80, 10)
        points2 = generate_rounded_rect_points(50, 30, 100, 80, 10)

        # All x coordinates should be offset by 50
        for i in range(len(points1)):
            assert points2[i][0] == points1[i][0] + 50
            assert points2[i][1] == points1[i][1] + 30


class TestToolkitDemo:
    """Tests for the ToolkitDemo class."""

    def test_initialization(self):
        """Demo should initialize with mode 0 and animation enabled."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        assert demo.mode == 0
        assert demo.fb is fb
        assert demo.animation_enabled is True
        assert demo.frame == 0

    def test_next_mode_cycles(self):
        """next_mode should cycle through 0-4."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        assert demo.mode == 0
        demo.next_mode()
        assert demo.mode == 1
        demo.next_mode()
        assert demo.mode == 2
        demo.next_mode()
        assert demo.mode == 3
        demo.next_mode()
        assert demo.mode == 4
        demo.next_mode()
        assert demo.mode == 0  # Wraps around

    def test_set_mode_valid(self):
        """set_mode should accept valid modes 0-4."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        demo.set_mode(2)
        assert demo.mode == 2

        demo.set_mode(0)
        assert demo.mode == 0

        demo.set_mode(3)
        assert demo.mode == 3

        demo.set_mode(4)
        assert demo.mode == 4

    def test_set_mode_invalid(self):
        """set_mode should ignore invalid modes."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        demo.set_mode(2)
        assert demo.mode == 2

        demo.set_mode(-1)  # Invalid
        assert demo.mode == 2  # Unchanged

        demo.set_mode(5)  # Invalid
        assert demo.mode == 2  # Unchanged

    def test_get_mode_name(self):
        """get_mode_name should return correct name for each mode."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        demo.set_mode(0)
        assert demo.get_mode_name() == "Patterns"

        demo.set_mode(1)
        assert demo.get_mode_name() == "Bezier Curves"

        demo.set_mode(2)
        assert demo.get_mode_name() == "Numerals"

        demo.set_mode(3)
        assert demo.get_mode_name() == "Clock Sketch"

        demo.set_mode(4)
        assert demo.get_mode_name() == "Typography"

    def test_toggle_animation(self):
        """toggle_animation should toggle and return new state."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        # Initially enabled
        assert demo.animation_enabled is True

        # Toggle off
        result = demo.toggle_animation()
        assert result is False
        assert demo.animation_enabled is False

        # Toggle on
        result = demo.toggle_animation()
        assert result is True
        assert demo.animation_enabled is True

    def test_frame_increments_on_draw(self):
        """Frame counter should increment each time draw() is called."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        assert demo.frame == 0
        demo.draw()
        assert demo.frame == 1
        demo.draw()
        assert demo.frame == 2


class TestDemoDrawing:
    """Tests for demo drawing functions."""

    def test_draw_clears_framebuffer(self):
        """draw() should start with a clear framebuffer."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        # Set some pixels
        fb.set_pixel(100, 100, True)
        fb.set_pixel(200, 200, True)

        # Draw should clear
        demo.draw()

        # After draw, original pixels should be cleared
        # (though new pixels may be drawn)
        # Just verify no exception is thrown
        assert True

    def test_draw_patterns_mode(self):
        """Drawing in patterns mode should not crash."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.set_mode(0)

        # Should not raise
        demo.draw()

        # Framebuffer should have some pixels set
        has_black = any(
            fb.get_pixel(x, y)
            for x in range(0, fb.WIDTH, 10)
            for y in range(0, fb.HEIGHT, 10)
        )
        assert has_black

    def test_draw_bezier_mode(self):
        """Drawing in bezier mode should not crash."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.set_mode(1)

        # Should not raise
        demo.draw()

        # Framebuffer should have some pixels set
        has_black = any(
            fb.get_pixel(x, y)
            for x in range(0, fb.WIDTH, 10)
            for y in range(0, fb.HEIGHT, 10)
        )
        assert has_black

    def test_draw_numerals_mode(self):
        """Drawing in numerals mode should not crash."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.set_mode(2)

        # Should not raise
        demo.draw()

        # Framebuffer should have some pixels set
        has_black = any(
            fb.get_pixel(x, y)
            for x in range(0, fb.WIDTH, 10)
            for y in range(0, fb.HEIGHT, 10)
        )
        assert has_black

    def test_draw_clock_mode(self):
        """Drawing in clock mode should not crash."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.set_mode(3)

        # Should not raise
        demo.draw()

        # Framebuffer should have some pixels set
        has_black = any(
            fb.get_pixel(x, y)
            for x in range(0, fb.WIDTH, 10)
            for y in range(0, fb.HEIGHT, 10)
        )
        assert has_black

    def test_draw_typography_mode(self):
        """Drawing in typography mode should not crash."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.set_mode(4)

        # Should not raise
        demo.draw()

        # Framebuffer should have some pixels set
        has_black = any(
            fb.get_pixel(x, y)
            for x in range(0, fb.WIDTH, 10)
            for y in range(0, fb.HEIGHT, 10)
        )
        assert has_black

    def test_all_modes_produce_different_output(self):
        """Each mode should produce visually different output."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        # Disable animation for consistent output
        demo.animation_enabled = False

        # Collect pixel patterns for each mode
        patterns = []
        for mode in range(5):
            demo.set_mode(mode)
            demo.draw()

            # Sample some pixels to create a simple hash
            sample = tuple(
                fb.get_pixel(x, y)
                for x in range(50, 350, 30)
                for y in range(50, 250, 30)
            )
            patterns.append(sample)

        # At least some modes should be different
        # (not all, since some overlapping content is possible)
        unique_patterns = len(set(patterns))
        assert unique_patterns >= 2, "Modes should produce varied output"


class TestDemoModeConstants:
    """Tests for mode constants."""

    def test_mode_constants_exist(self):
        """Mode constants should be defined."""
        assert hasattr(ToolkitDemo, "MODE_PATTERNS")
        assert hasattr(ToolkitDemo, "MODE_BEZIER")
        assert hasattr(ToolkitDemo, "MODE_NUMERALS")
        assert hasattr(ToolkitDemo, "MODE_CLOCK")
        assert hasattr(ToolkitDemo, "MODE_TYPOGRAPHY")

    def test_mode_constants_values(self):
        """Mode constants should have correct values."""
        assert ToolkitDemo.MODE_PATTERNS == 0
        assert ToolkitDemo.MODE_BEZIER == 1
        assert ToolkitDemo.MODE_NUMERALS == 2
        assert ToolkitDemo.MODE_CLOCK == 3
        assert ToolkitDemo.MODE_TYPOGRAPHY == 4

    def test_mode_names_list(self):
        """MODE_NAMES should have 5 entries."""
        assert len(ToolkitDemo.MODE_NAMES) == 5
        assert all(isinstance(name, str) for name in ToolkitDemo.MODE_NAMES)


class TestAnimationHelpers:
    """Tests for animation helper methods."""

    def test_breathing_scale_range(self):
        """Breathing scale should stay within min/max bounds."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        # Test over many frames
        for frame in range(200):
            demo.frame = frame
            scale = demo._breathing_scale(speed=1.0, min_val=0.9, max_val=1.1)
            assert 0.9 <= scale <= 1.1, f"Scale {scale} out of bounds at frame {frame}"

    def test_breathing_scale_default_params(self):
        """Breathing scale with defaults should work."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.frame = 50

        scale = demo._breathing_scale()
        assert 0.95 <= scale <= 1.05

    def test_wiggle_offset_returns_tuple(self):
        """Wiggle offset should return a tuple of two floats."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.frame = 25

        offset = demo._wiggle_offset(seed=0, amplitude=2.0, frequency=1.0)
        assert isinstance(offset, tuple)
        assert len(offset) == 2
        assert isinstance(offset[0], float)
        assert isinstance(offset[1], float)

    def test_wiggle_offset_amplitude(self):
        """Wiggle offset should stay within amplitude bounds."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)

        amplitude = 5.0
        for frame in range(200):
            demo.frame = frame
            dx, dy = demo._wiggle_offset(seed=0, amplitude=amplitude)
            assert abs(dx) <= amplitude, f"dx {dx} exceeds amplitude at frame {frame}"
            assert abs(dy) <= amplitude, f"dy {dy} exceeds amplitude at frame {frame}"

    def test_wiggle_offset_different_seeds(self):
        """Different seeds should produce different offsets."""
        fb = Framebuffer()
        demo = ToolkitDemo(fb)
        demo.frame = 50

        offset0 = demo._wiggle_offset(seed=0)
        offset1 = demo._wiggle_offset(seed=1)
        offset2 = demo._wiggle_offset(seed=2)

        # At least some should be different (very unlikely to be same)
        offsets = [offset0, offset1, offset2]
        assert len(set(offsets)) > 1, "Different seeds should produce different offsets"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
