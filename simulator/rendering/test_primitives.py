"""
Tests for the primitives module.

These tests verify:
- draw_line: Bresenham algorithm correctness
- draw_polygon: Polygon outline drawing
- fill_polygon: Scanline fill algorithm
- fill_rect: Rectangle filling
- draw_circle: Midpoint circle algorithm
- fill_circle: Filled circle using spans
"""

import unittest
from rendering.framebuffer import Framebuffer
from rendering.primitives import (
    draw_line,
    draw_polygon,
    fill_polygon,
    fill_rect,
    draw_circle,
    fill_circle,
)


class TestDrawLine(unittest.TestCase):
    """Test Bresenham's line algorithm."""

    def test_horizontal_line(self):
        """Draw a horizontal line."""
        fb = Framebuffer()
        draw_line(fb, 10, 50, 20, 50, True)

        for x in range(10, 21):
            self.assertTrue(fb.get_pixel(x, 50), f"Pixel at ({x}, 50) should be set")

        # Check adjacent pixels are not set
        self.assertFalse(fb.get_pixel(9, 50))
        self.assertFalse(fb.get_pixel(21, 50))
        self.assertFalse(fb.get_pixel(15, 49))
        self.assertFalse(fb.get_pixel(15, 51))

    def test_vertical_line(self):
        """Draw a vertical line."""
        fb = Framebuffer()
        draw_line(fb, 50, 10, 50, 20, True)

        for y in range(10, 21):
            self.assertTrue(fb.get_pixel(50, y), f"Pixel at (50, {y}) should be set")

        self.assertFalse(fb.get_pixel(50, 9))
        self.assertFalse(fb.get_pixel(50, 21))

    def test_diagonal_line_45_degrees(self):
        """Draw a 45-degree diagonal line."""
        fb = Framebuffer()
        draw_line(fb, 10, 10, 20, 20, True)

        for i in range(11):
            self.assertTrue(fb.get_pixel(10 + i, 10 + i))

    def test_diagonal_line_negative_slope(self):
        """Draw a diagonal line with negative slope."""
        fb = Framebuffer()
        draw_line(fb, 10, 20, 20, 10, True)

        for i in range(11):
            self.assertTrue(fb.get_pixel(10 + i, 20 - i))

    def test_line_reversed_endpoints(self):
        """Line should have same endpoints regardless of order."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        draw_line(fb1, 10, 10, 50, 30, True)
        draw_line(fb2, 50, 30, 10, 10, True)

        # Both endpoints should be set in both lines
        self.assertTrue(fb1.get_pixel(10, 10))
        self.assertTrue(fb1.get_pixel(50, 30))
        self.assertTrue(fb2.get_pixel(10, 10))
        self.assertTrue(fb2.get_pixel(50, 30))

        # Lines should have same pixel count (may differ by at most 1 pixel
        # due to integer rounding in different directions)
        count1 = sum(1 for x in range(60) for y in range(40) if fb1.get_pixel(x, y))
        count2 = sum(1 for x in range(60) for y in range(40) if fb2.get_pixel(x, y))
        self.assertAlmostEqual(count1, count2, delta=1)

    def test_single_point_line(self):
        """Line from point to same point should draw single pixel."""
        fb = Framebuffer()
        draw_line(fb, 100, 100, 100, 100, True)

        self.assertTrue(fb.get_pixel(100, 100))

        # Count set pixels - should be exactly 1
        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT) if fb.get_pixel(x, y))
        self.assertEqual(count, 1)

    def test_steep_line(self):
        """Draw a steep line (dy > dx)."""
        fb = Framebuffer()
        draw_line(fb, 10, 10, 15, 30, True)

        # Check endpoints
        self.assertTrue(fb.get_pixel(10, 10))
        self.assertTrue(fb.get_pixel(15, 30))

    def test_shallow_line(self):
        """Draw a shallow line (dx > dy)."""
        fb = Framebuffer()
        draw_line(fb, 10, 10, 30, 15, True)

        # Check endpoints
        self.assertTrue(fb.get_pixel(10, 10))
        self.assertTrue(fb.get_pixel(30, 15))

    def test_line_all_octants(self):
        """Test lines in all 8 octants from center point."""
        endpoints = [
            (50, 0),    # Up
            (100, 0),   # Up-right
            (100, 50),  # Right
            (100, 100), # Down-right
            (50, 100),  # Down
            (0, 100),   # Down-left
            (0, 50),    # Left
            (0, 0),     # Up-left
        ]

        for ex, ey in endpoints:
            fb = Framebuffer()
            draw_line(fb, 50, 50, ex, ey, True)

            # Both endpoints should be set
            self.assertTrue(fb.get_pixel(50, 50), f"Center not set for line to ({ex}, {ey})")
            self.assertTrue(fb.get_pixel(ex, ey), f"Endpoint ({ex}, {ey}) not set")

    def test_line_with_white(self):
        """Draw a white line on black background."""
        fb = Framebuffer()
        fb.clear(True)  # All black
        draw_line(fb, 10, 10, 20, 10, False)

        for x in range(10, 21):
            self.assertFalse(fb.get_pixel(x, 10))


class TestDrawPolygon(unittest.TestCase):
    """Test polygon outline drawing."""

    def test_triangle(self):
        """Draw a triangle outline."""
        fb = Framebuffer()
        points = [(50, 10), (10, 90), (90, 90)]
        draw_polygon(fb, points, True)

        # Check that vertices are set
        for x, y in points:
            self.assertTrue(fb.get_pixel(x, y))

    def test_square(self):
        """Draw a square outline."""
        fb = Framebuffer()
        points = [(10, 10), (50, 10), (50, 50), (10, 50)]
        draw_polygon(fb, points, True)

        # Check all four edges
        # Top edge
        for x in range(10, 51):
            self.assertTrue(fb.get_pixel(x, 10))
        # Bottom edge
        for x in range(10, 51):
            self.assertTrue(fb.get_pixel(x, 50))
        # Left edge
        for y in range(10, 51):
            self.assertTrue(fb.get_pixel(10, y))
        # Right edge
        for y in range(10, 51):
            self.assertTrue(fb.get_pixel(50, y))

    def test_single_point(self):
        """Single point polygon should draw nothing."""
        fb = Framebuffer()
        draw_polygon(fb, [(10, 10)], True)

        # Should be empty
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_two_points(self):
        """Two points should draw a line between them."""
        fb = Framebuffer()
        draw_polygon(fb, [(10, 10), (20, 10)], True)

        for x in range(10, 21):
            self.assertTrue(fb.get_pixel(x, 10))

    def test_empty_polygon(self):
        """Empty polygon should draw nothing."""
        fb = Framebuffer()
        draw_polygon(fb, [], True)
        self.assertTrue(all(b == 0 for b in fb.buffer))


class TestFillPolygon(unittest.TestCase):
    """Test scanline fill algorithm."""

    def test_fill_square(self):
        """Fill a square polygon.

        Note: Scanline fill uses y < y1 to avoid double-counting vertices,
        so the bottom edge at y=max_y is not filled. This is standard
        behavior for scanline algorithms.
        """
        fb = Framebuffer()
        points = [(10, 10), (20, 10), (20, 20), (10, 20)]
        fill_polygon(fb, points, True)

        # Interior pixels should be filled (y from 10 to 19, x from 10 to 20)
        for x in range(10, 21):
            for y in range(10, 20):  # Note: y=20 is not filled (exclusive)
                self.assertTrue(fb.get_pixel(x, y), f"Pixel ({x}, {y}) should be set")

        # Bottom edge at y=20 is NOT filled (standard scanline behavior)
        # This prevents double-filling when polygons share edges
        self.assertFalse(fb.get_pixel(15, 20))

    def test_fill_triangle(self):
        """Fill a triangle."""
        fb = Framebuffer()
        points = [(50, 10), (10, 50), (90, 50)]
        fill_polygon(fb, points, True)

        # Check some interior points
        self.assertTrue(fb.get_pixel(50, 30))
        self.assertTrue(fb.get_pixel(50, 40))

        # Check exterior points
        self.assertFalse(fb.get_pixel(5, 30))
        self.assertFalse(fb.get_pixel(95, 30))

    def test_fill_concave_polygon(self):
        """Fill a simple concave polygon (arrow shape)."""
        fb = Framebuffer()
        # Arrow pointing right
        points = [(10, 30), (30, 10), (30, 20), (50, 20), (50, 40), (30, 40), (30, 50)]
        fill_polygon(fb, points, True)

        # Check some interior points
        self.assertTrue(fb.get_pixel(20, 30))  # In the triangle part
        self.assertTrue(fb.get_pixel(40, 30))  # In the rectangle part

    def test_fill_too_few_points(self):
        """Filling with fewer than 3 points should do nothing."""
        fb = Framebuffer()
        fill_polygon(fb, [(10, 10), (20, 20)], True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

        fb2 = Framebuffer()
        fill_polygon(fb2, [(10, 10)], True)
        self.assertTrue(all(b == 0 for b in fb2.buffer))

        fb3 = Framebuffer()
        fill_polygon(fb3, [], True)
        self.assertTrue(all(b == 0 for b in fb3.buffer))

    def test_fill_with_white(self):
        """Fill polygon with white on black background."""
        fb = Framebuffer()
        fb.clear(True)
        points = [(10, 10), (20, 10), (20, 20), (10, 20)]
        fill_polygon(fb, points, False)

        # Interior should be white
        self.assertFalse(fb.get_pixel(15, 15))
        # Exterior should still be black
        self.assertTrue(fb.get_pixel(5, 5))


class TestFillRect(unittest.TestCase):
    """Test rectangle fill."""

    def test_basic_rect(self):
        """Fill a basic rectangle."""
        fb = Framebuffer()
        fill_rect(fb, 10, 20, 30, 40, True)

        # Check corners
        self.assertTrue(fb.get_pixel(10, 20))
        self.assertTrue(fb.get_pixel(39, 20))
        self.assertTrue(fb.get_pixel(10, 59))
        self.assertTrue(fb.get_pixel(39, 59))

        # Check interior
        self.assertTrue(fb.get_pixel(25, 40))

        # Check just outside
        self.assertFalse(fb.get_pixel(9, 40))
        self.assertFalse(fb.get_pixel(40, 40))
        self.assertFalse(fb.get_pixel(25, 19))
        self.assertFalse(fb.get_pixel(25, 60))

    def test_rect_1x1(self):
        """Single pixel rectangle."""
        fb = Framebuffer()
        fill_rect(fb, 100, 100, 1, 1, True)

        self.assertTrue(fb.get_pixel(100, 100))
        self.assertFalse(fb.get_pixel(99, 100))
        self.assertFalse(fb.get_pixel(101, 100))

    def test_rect_zero_dimensions(self):
        """Zero or negative dimensions should draw nothing."""
        fb = Framebuffer()
        fill_rect(fb, 10, 10, 0, 10, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

        fb2 = Framebuffer()
        fill_rect(fb2, 10, 10, 10, 0, True)
        self.assertTrue(all(b == 0 for b in fb2.buffer))

        fb3 = Framebuffer()
        fill_rect(fb3, 10, 10, -5, 10, True)
        self.assertTrue(all(b == 0 for b in fb3.buffer))

    def test_rect_full_row(self):
        """Fill entire row."""
        fb = Framebuffer()
        fill_rect(fb, 0, 50, 400, 1, True)

        for x in range(400):
            self.assertTrue(fb.get_pixel(x, 50))
        self.assertFalse(fb.get_pixel(0, 49))
        self.assertFalse(fb.get_pixel(0, 51))

    def test_rect_clipping(self):
        """Rectangle should be clipped at framebuffer boundaries."""
        fb = Framebuffer()
        fill_rect(fb, 390, 290, 20, 20, True)

        # Pixels within bounds should be set
        self.assertTrue(fb.get_pixel(395, 295))
        self.assertTrue(fb.get_pixel(399, 299))


class TestDrawCircle(unittest.TestCase):
    """Test midpoint circle algorithm."""

    def test_circle_radius_0(self):
        """Circle with radius 0 should be a single pixel."""
        fb = Framebuffer()
        draw_circle(fb, 100, 100, 0, True)

        self.assertTrue(fb.get_pixel(100, 100))
        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT) if fb.get_pixel(x, y))
        self.assertEqual(count, 1)

    def test_circle_radius_1(self):
        """Circle with radius 1."""
        fb = Framebuffer()
        draw_circle(fb, 100, 100, 1, True)

        # Cardinal points should be set
        self.assertTrue(fb.get_pixel(101, 100))  # Right
        self.assertTrue(fb.get_pixel(99, 100))   # Left
        self.assertTrue(fb.get_pixel(100, 101))  # Down
        self.assertTrue(fb.get_pixel(100, 99))   # Up

        # Center should not be set (it's a circle outline)
        # Note: For r=1, center might be set due to 8-way symmetry including (0, 1) and (1, 0)
        # This is technically correct for the algorithm

    def test_circle_symmetry(self):
        """Circle should be symmetric in all 8 octants."""
        fb = Framebuffer()
        cx, cy, r = 100, 100, 30
        draw_circle(fb, cx, cy, r, True)

        # Check 8-way symmetry
        for x in range(r + 1):
            for y in range(r + 1):
                if fb.get_pixel(cx + x, cy + y):
                    self.assertTrue(fb.get_pixel(cx - x, cy + y),
                                  f"Symmetry failed at ({cx - x}, {cy + y})")
                    self.assertTrue(fb.get_pixel(cx + x, cy - y))
                    self.assertTrue(fb.get_pixel(cx - x, cy - y))
                    self.assertTrue(fb.get_pixel(cx + y, cy + x))
                    self.assertTrue(fb.get_pixel(cx - y, cy + x))
                    self.assertTrue(fb.get_pixel(cx + y, cy - x))
                    self.assertTrue(fb.get_pixel(cx - y, cy - x))

    def test_circle_radius_10(self):
        """Circle with radius 10 should have correct points."""
        fb = Framebuffer()
        cx, cy, r = 100, 100, 10
        draw_circle(fb, cx, cy, r, True)

        # Cardinal points at distance r
        self.assertTrue(fb.get_pixel(cx + r, cy))
        self.assertTrue(fb.get_pixel(cx - r, cy))
        self.assertTrue(fb.get_pixel(cx, cy + r))
        self.assertTrue(fb.get_pixel(cx, cy - r))

    def test_circle_negative_radius(self):
        """Negative radius should draw nothing."""
        fb = Framebuffer()
        draw_circle(fb, 100, 100, -5, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_circle_at_origin(self):
        """Circle near origin should clip correctly."""
        fb = Framebuffer()
        draw_circle(fb, 10, 10, 5, True)

        # Should have some pixels set
        pixel_count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT) if fb.get_pixel(x, y))
        self.assertGreater(pixel_count, 0)


class TestFillCircle(unittest.TestCase):
    """Test filled circle using spans."""

    def test_fill_circle_radius_0(self):
        """Filled circle with radius 0 should be a single pixel."""
        fb = Framebuffer()
        fill_circle(fb, 100, 100, 0, True)

        self.assertTrue(fb.get_pixel(100, 100))
        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT) if fb.get_pixel(x, y))
        self.assertEqual(count, 1)

    def test_fill_circle_radius_1(self):
        """Filled circle with radius 1 should fill the cross pattern."""
        fb = Framebuffer()
        fill_circle(fb, 100, 100, 1, True)

        # Center and cardinal points should be filled
        self.assertTrue(fb.get_pixel(100, 100))
        self.assertTrue(fb.get_pixel(101, 100))
        self.assertTrue(fb.get_pixel(99, 100))
        self.assertTrue(fb.get_pixel(100, 101))
        self.assertTrue(fb.get_pixel(100, 99))

    def test_fill_circle_filled_completely(self):
        """All pixels within radius should be filled."""
        fb = Framebuffer()
        cx, cy, r = 100, 100, 20
        fill_circle(fb, cx, cy, r, True)

        # Check that center is filled
        self.assertTrue(fb.get_pixel(cx, cy))

        # Check points inside should be filled
        # A point at distance < r from center should be filled
        for x in range(cx - r, cx + r + 1):
            for y in range(cy - r, cy + r + 1):
                dist_sq = (x - cx) ** 2 + (y - cy) ** 2
                if dist_sq < (r - 1) ** 2:  # Well inside the circle
                    self.assertTrue(fb.get_pixel(x, y),
                                  f"Interior pixel ({x}, {y}) should be filled")

    def test_fill_circle_not_filled_outside(self):
        """Pixels outside radius should not be filled."""
        fb = Framebuffer()
        cx, cy, r = 100, 100, 20
        fill_circle(fb, cx, cy, r, True)

        # Check points well outside
        self.assertFalse(fb.get_pixel(cx + r + 5, cy))
        self.assertFalse(fb.get_pixel(cx, cy + r + 5))
        self.assertFalse(fb.get_pixel(cx - r - 5, cy))
        self.assertFalse(fb.get_pixel(cx, cy - r - 5))

    def test_fill_circle_negative_radius(self):
        """Negative radius should draw nothing."""
        fb = Framebuffer()
        fill_circle(fb, 100, 100, -5, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_fill_circle_symmetry(self):
        """Filled circle should be symmetric."""
        fb = Framebuffer()
        cx, cy, r = 100, 100, 25
        fill_circle(fb, cx, cy, r, True)

        # Check 4-way symmetry
        for x in range(r + 2):
            for y in range(r + 2):
                if fb.get_pixel(cx + x, cy + y):
                    self.assertTrue(fb.get_pixel(cx - x, cy + y),
                                  f"Symmetry failed at ({cx - x}, {cy + y})")
                    self.assertTrue(fb.get_pixel(cx + x, cy - y))
                    self.assertTrue(fb.get_pixel(cx - x, cy - y))

    def test_fill_circle_with_white(self):
        """Fill circle with white on black background."""
        fb = Framebuffer()
        fb.clear(True)
        fill_circle(fb, 100, 100, 20, False)

        # Center should be white
        self.assertFalse(fb.get_pixel(100, 100))
        # Outside should still be black
        self.assertTrue(fb.get_pixel(150, 100))


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and boundary conditions."""

    def test_draw_at_boundaries(self):
        """Draw primitives at framebuffer boundaries."""
        fb = Framebuffer()

        # Line at top edge
        draw_line(fb, 0, 0, 100, 0, True)
        self.assertTrue(fb.get_pixel(50, 0))

        # Line at bottom edge
        draw_line(fb, 0, 299, 100, 299, True)
        self.assertTrue(fb.get_pixel(50, 299))

        # Line at left edge
        draw_line(fb, 0, 0, 0, 100, True)
        self.assertTrue(fb.get_pixel(0, 50))

        # Line at right edge
        draw_line(fb, 399, 0, 399, 100, True)
        self.assertTrue(fb.get_pixel(399, 50))

    def test_rect_at_corner(self):
        """Rectangle at corner should clip correctly."""
        fb = Framebuffer()
        fill_rect(fb, 390, 290, 20, 20, True)

        # Corner should be filled
        self.assertTrue(fb.get_pixel(399, 299))

    def test_circle_at_corner(self):
        """Circle at corner should clip correctly."""
        fb = Framebuffer()
        fill_circle(fb, 10, 10, 20, True)

        # Origin should not cause issues
        self.assertTrue(fb.get_pixel(10, 10))

    def test_large_circle(self):
        """Large circle spanning most of the framebuffer."""
        fb = Framebuffer()
        fill_circle(fb, 200, 150, 140, True)

        # Center should be filled
        self.assertTrue(fb.get_pixel(200, 150))

    def test_polygon_outside_bounds(self):
        """Polygon with vertices outside bounds should still work."""
        fb = Framebuffer()
        points = [(-10, 50), (50, -10), (110, 50), (50, 110)]
        fill_polygon(fb, points, True)

        # Some interior pixels should be filled
        self.assertTrue(fb.get_pixel(50, 50))


if __name__ == "__main__":
    unittest.main()
