"""
Tests for the bezier module.

Tests cover:
- auto_tangent: control handle generation for smooth curves
- cubic_bezier: De Casteljau bezier evaluation
- cubic_bezier_derivative: tangent vector calculation
- subdivide_bezier: adaptive subdivision into line segments
- stroke_bezier_texture_ball: Pope's texture-ball stroke technique
- draw_bezier_curve: simple bezier curve drawing
"""

import math
import pytest

try:
    from .bezier import (
        auto_tangent,
        cubic_bezier,
        cubic_bezier_derivative,
        subdivide_bezier,
        stroke_bezier_texture_ball,
        draw_bezier_curve,
        DEFAULT_BALL_8X8,
        _bezier_flatness,
    )
    from .framebuffer import Framebuffer
except ImportError:
    from bezier import (
        auto_tangent,
        cubic_bezier,
        cubic_bezier_derivative,
        subdivide_bezier,
        stroke_bezier_texture_ball,
        draw_bezier_curve,
        DEFAULT_BALL_8X8,
        _bezier_flatness,
    )
    from framebuffer import Framebuffer


class TestAutoTangent:
    """Tests for auto_tangent function."""

    def test_empty_points(self):
        """Empty point list returns empty handles."""
        result = auto_tangent([])
        assert result == []

    def test_single_point(self):
        """Single point returns zero handles."""
        result = auto_tangent([(100.0, 100.0)])
        assert len(result) == 1
        assert result[0] == ((0.0, 0.0), (0.0, 0.0))

    def test_two_points(self):
        """Two points creates handles along the line direction."""
        points = [(0.0, 0.0), (100.0, 0.0)]
        handles = auto_tangent(points, smoothness=0.5)

        assert len(handles) == 2

        # First point: no incoming, outgoing along positive x
        h0_in, h0_out = handles[0]
        assert h0_in == (0.0, 0.0)  # No distance to previous
        assert h0_out[0] > 0  # Points right

        # Second point: incoming along positive x, no outgoing
        h1_in, h1_out = handles[1]
        assert h1_in[0] < 0  # Points left (toward first point)
        assert h1_out == (0.0, 0.0)  # No distance to next

    def test_three_points_horizontal(self):
        """Three horizontal points creates horizontal tangents."""
        points = [(0.0, 0.0), (50.0, 0.0), (100.0, 0.0)]
        handles = auto_tangent(points, smoothness=0.5)

        assert len(handles) == 3

        # Middle point should have horizontal tangent
        h_in, h_out = handles[1]
        # Tangent direction should be purely horizontal
        assert abs(h_in[1]) < 1e-10
        assert abs(h_out[1]) < 1e-10

    def test_three_points_diagonal(self):
        """Three diagonal points creates diagonal tangents."""
        points = [(0.0, 0.0), (50.0, 50.0), (100.0, 100.0)]
        handles = auto_tangent(points, smoothness=0.5)

        assert len(handles) == 3

        # Middle point should have 45-degree tangent
        h_in, h_out = handles[1]
        # Ratio of y to x should be 1 (45 degrees)
        if abs(h_out[0]) > 1e-10:
            ratio = h_out[1] / h_out[0]
            assert abs(ratio - 1.0) < 1e-10

    def test_smoothness_zero(self):
        """Smoothness 0 creates zero-length handles (sharp corners)."""
        points = [(0.0, 0.0), (50.0, 50.0), (100.0, 0.0)]
        handles = auto_tangent(points, smoothness=0.0)

        for h_in, h_out in handles:
            assert h_in == (0.0, 0.0)
            assert h_out == (0.0, 0.0)

    def test_smoothness_affects_handle_length(self):
        """Higher smoothness creates longer handles."""
        points = [(0.0, 0.0), (50.0, 50.0), (100.0, 100.0)]

        handles_low = auto_tangent(points, smoothness=0.25)
        handles_high = auto_tangent(points, smoothness=0.75)

        # Get handle lengths for middle point
        h_low = handles_low[1][1]
        h_high = handles_high[1][1]

        len_low = math.sqrt(h_low[0] ** 2 + h_low[1] ** 2)
        len_high = math.sqrt(h_high[0] ** 2 + h_high[1] ** 2)

        assert len_high > len_low

    def test_curve_through_points(self):
        """Handles create a curve that passes through control points."""
        points = [(0.0, 0.0), (100.0, 50.0), (200.0, 0.0)]
        handles = auto_tangent(points, smoothness=0.5)

        # Verify that bezier evaluation at t=0 and t=1 gives endpoints
        p0 = points[0]
        p3 = points[1]
        h_out = handles[0][1]
        h_in = handles[1][0]

        p1 = (p0[0] + h_out[0], p0[1] + h_out[1])
        p2 = (p3[0] + h_in[0], p3[1] + h_in[1])

        # At t=0, bezier should be at p0
        result = cubic_bezier(p0, p1, p2, p3, 0.0)
        assert abs(result[0] - p0[0]) < 1e-10
        assert abs(result[1] - p0[1]) < 1e-10

        # At t=1, bezier should be at p3
        result = cubic_bezier(p0, p1, p2, p3, 1.0)
        assert abs(result[0] - p3[0]) < 1e-10
        assert abs(result[1] - p3[1]) < 1e-10


class TestCubicBezier:
    """Tests for cubic_bezier function using De Casteljau's algorithm."""

    def test_t_zero_returns_start(self):
        """t=0 returns the start point."""
        p0 = (0.0, 0.0)
        p1 = (50.0, 100.0)
        p2 = (150.0, 100.0)
        p3 = (200.0, 0.0)

        result = cubic_bezier(p0, p1, p2, p3, 0.0)
        assert result == p0

    def test_t_one_returns_end(self):
        """t=1 returns the end point."""
        p0 = (0.0, 0.0)
        p1 = (50.0, 100.0)
        p2 = (150.0, 100.0)
        p3 = (200.0, 0.0)

        result = cubic_bezier(p0, p1, p2, p3, 1.0)
        assert result == p3

    def test_t_half_symmetric_curve(self):
        """t=0.5 on symmetric curve returns center point."""
        # Symmetric curve: control points mirror about center
        p0 = (0.0, 0.0)
        p1 = (0.0, 100.0)
        p2 = (200.0, 100.0)
        p3 = (200.0, 0.0)

        result = cubic_bezier(p0, p1, p2, p3, 0.5)
        # X should be at center (100)
        assert abs(result[0] - 100.0) < 1e-10
        # Y should be elevated due to control points
        assert result[1] > 0

    def test_straight_line(self):
        """Bezier with collinear points is a straight line."""
        p0 = (0.0, 0.0)
        p1 = (100.0 / 3, 0.0)
        p2 = (200.0 / 3, 0.0)
        p3 = (100.0, 0.0)

        # Points along the line
        for t in [0.0, 0.25, 0.5, 0.75, 1.0]:
            result = cubic_bezier(p0, p1, p2, p3, t)
            # Y should always be 0
            assert abs(result[1]) < 1e-10
            # X should be linearly interpolated
            expected_x = t * 100.0
            assert abs(result[0] - expected_x) < 1e-10

    def test_de_casteljau_accuracy(self):
        """Verify De Casteljau matches explicit Bernstein formula."""
        p0 = (10.0, 20.0)
        p1 = (30.0, 80.0)
        p2 = (70.0, 90.0)
        p3 = (90.0, 30.0)

        for t in [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0]:
            result = cubic_bezier(p0, p1, p2, p3, t)

            # Explicit Bernstein polynomial:
            # B(t) = (1-t)^3*P0 + 3*(1-t)^2*t*P1 + 3*(1-t)*t^2*P2 + t^3*P3
            u = 1 - t
            expected_x = (
                u**3 * p0[0]
                + 3 * u**2 * t * p1[0]
                + 3 * u * t**2 * p2[0]
                + t**3 * p3[0]
            )
            expected_y = (
                u**3 * p0[1]
                + 3 * u**2 * t * p1[1]
                + 3 * u * t**2 * p2[1]
                + t**3 * p3[1]
            )

            assert abs(result[0] - expected_x) < 1e-10
            assert abs(result[1] - expected_y) < 1e-10


class TestCubicBezierDerivative:
    """Tests for cubic_bezier_derivative function."""

    def test_horizontal_line_tangent(self):
        """Horizontal line has horizontal tangent."""
        p0 = (0.0, 0.0)
        p1 = (33.3, 0.0)
        p2 = (66.6, 0.0)
        p3 = (100.0, 0.0)

        tangent = cubic_bezier_derivative(p0, p1, p2, p3, 0.5)
        # Should point right (positive x, zero y)
        assert tangent[0] > 0
        assert abs(tangent[1]) < 1e-10

    def test_vertical_line_tangent(self):
        """Vertical line has vertical tangent."""
        p0 = (0.0, 0.0)
        p1 = (0.0, 33.3)
        p2 = (0.0, 66.6)
        p3 = (0.0, 100.0)

        tangent = cubic_bezier_derivative(p0, p1, p2, p3, 0.5)
        # Should point up (zero x, positive y)
        assert abs(tangent[0]) < 1e-10
        assert tangent[1] > 0

    def test_tangent_at_endpoints(self):
        """Tangent at t=0 points from p0 toward p1."""
        p0 = (0.0, 0.0)
        p1 = (100.0, 50.0)
        p2 = (200.0, 50.0)
        p3 = (300.0, 0.0)

        tangent = cubic_bezier_derivative(p0, p1, p2, p3, 0.0)
        # Tangent should be 3*(P1-P0) = (300, 150)
        expected = (3 * (p1[0] - p0[0]), 3 * (p1[1] - p0[1]))
        assert abs(tangent[0] - expected[0]) < 1e-10
        assert abs(tangent[1] - expected[1]) < 1e-10


class TestSubdivideBezier:
    """Tests for subdivide_bezier function."""

    def test_straight_line_minimal_subdivision(self):
        """Straight line requires minimal subdivision."""
        p0 = (0.0, 0.0)
        c0 = (50.0, 0.0)
        c1 = (100.0, 0.0)
        p1 = (150.0, 0.0)

        points = subdivide_bezier(p0, c0, c1, p1, tolerance=1.0)
        # Should just have start and end since line is flat
        assert len(points) == 2
        assert points[0] == (0, 0)
        assert points[1] == (150, 0)

    def test_curved_bezier_subdivides(self):
        """Curved bezier produces multiple segments."""
        p0 = (0.0, 0.0)
        c0 = (0.0, 100.0)
        c1 = (100.0, 100.0)
        p1 = (100.0, 0.0)

        points = subdivide_bezier(p0, c0, c1, p1, tolerance=1.0)
        # Should have more than 2 points due to curvature
        assert len(points) > 2
        # First and last should be endpoints
        assert points[0] == (0, 0)
        assert points[-1] == (100, 0)

    def test_lower_tolerance_more_segments(self):
        """Lower tolerance produces more segments."""
        p0 = (0.0, 0.0)
        c0 = (0.0, 50.0)
        c1 = (100.0, 50.0)
        p1 = (100.0, 0.0)

        points_coarse = subdivide_bezier(p0, c0, c1, p1, tolerance=5.0)
        points_fine = subdivide_bezier(p0, c0, c1, p1, tolerance=0.5)

        assert len(points_fine) >= len(points_coarse)

    def test_integer_output(self):
        """Output points are integers."""
        p0 = (0.5, 0.5)
        c0 = (25.7, 50.3)
        c1 = (75.2, 50.8)
        p1 = (99.5, 0.5)

        points = subdivide_bezier(p0, c0, c1, p1, tolerance=1.0)

        for x, y in points:
            assert isinstance(x, int)
            assert isinstance(y, int)


class TestBezierFlatness:
    """Tests for the _bezier_flatness helper function."""

    def test_flat_line_zero_flatness(self):
        """Collinear points have zero flatness."""
        p0 = (0.0, 0.0)
        c0 = (33.0, 0.0)
        c1 = (66.0, 0.0)
        p1 = (100.0, 0.0)

        flatness = _bezier_flatness(p0, c0, c1, p1)
        assert flatness < 1e-10

    def test_curved_has_positive_flatness(self):
        """Control points off the line have positive flatness."""
        p0 = (0.0, 0.0)
        c0 = (25.0, 50.0)  # 50 pixels above line
        c1 = (75.0, 50.0)
        p1 = (100.0, 0.0)

        flatness = _bezier_flatness(p0, c0, c1, p1)
        assert flatness > 0

    def test_flatness_increases_with_deviation(self):
        """More deviation = higher flatness."""
        p0 = (0.0, 0.0)
        p1 = (100.0, 0.0)

        flat1 = _bezier_flatness(p0, (50.0, 10.0), (50.0, 10.0), p1)
        flat2 = _bezier_flatness(p0, (50.0, 50.0), (50.0, 50.0), p1)

        assert flat2 > flat1


class TestDefaultBall:
    """Tests for DEFAULT_BALL_8X8 texture."""

    def test_is_8x8(self):
        """Texture is 8x8."""
        assert len(DEFAULT_BALL_8X8) == 8
        for row in DEFAULT_BALL_8X8:
            assert len(row) == 8

    def test_contains_both_values(self):
        """Texture contains both True and False values."""
        has_true = False
        has_false = False

        for row in DEFAULT_BALL_8X8:
            for pixel in row:
                if pixel:
                    has_true = True
                else:
                    has_false = True

        assert has_true, "Texture should contain True (ink) values"
        assert has_false, "Texture should contain False (transparent) values"

    def test_roughly_circular(self):
        """Texture has corners empty (roughly circular shape)."""
        # Corners should be False (transparent)
        assert not DEFAULT_BALL_8X8[0][0]
        assert not DEFAULT_BALL_8X8[0][7]
        assert not DEFAULT_BALL_8X8[7][0]
        assert not DEFAULT_BALL_8X8[7][7]


class TestStrokeBezierTextureBall:
    """Tests for stroke_bezier_texture_ball function."""

    def test_single_point_stamps_texture(self):
        """Single point stamps texture at that location."""
        fb = Framebuffer()
        texture = [[True, True], [True, True]]

        stroke_bezier_texture_ball(fb, [(200.0, 150.0)], 0.5, texture)

        # Should have pixels near the point
        pixels_near = 0
        for dy in range(-5, 6):
            for dx in range(-5, 6):
                if fb.get_pixel(200 + dx, 150 + dy):
                    pixels_near += 1

        assert pixels_near > 0

    def test_two_points_creates_stroke(self):
        """Two points creates a stroke between them."""
        fb = Framebuffer()
        texture = [[True]]  # Simple 1x1 texture

        stroke_bezier_texture_ball(
            fb, [(50.0, 150.0), (350.0, 150.0)], 0.5, texture, spacing=5.0
        )

        # Should have pixels along the path
        pixels_on_path = 0
        for x in range(50, 351, 10):
            if fb.get_pixel(x, 150):
                pixels_on_path += 1

        assert pixels_on_path > 0

    def test_smoothness_affects_curve(self):
        """Different smoothness creates different curves."""
        points = [(50.0, 150.0), (200.0, 50.0), (350.0, 150.0)]
        texture = [[True]]

        fb_sharp = Framebuffer()
        stroke_bezier_texture_ball(fb_sharp, points, 0.0, texture, spacing=2.0)

        fb_smooth = Framebuffer()
        stroke_bezier_texture_ball(fb_smooth, points, 1.0, texture, spacing=2.0)

        # Count differing pixels
        diff_count = 0
        for y in range(fb_sharp.HEIGHT):
            for x in range(fb_sharp.WIDTH):
                if fb_sharp.get_pixel(x, y) != fb_smooth.get_pixel(x, y):
                    diff_count += 1

        # Should have some differences (curves are different)
        assert diff_count > 0

    def test_spacing_affects_density(self):
        """Smaller spacing creates denser strokes."""
        points = [(50.0, 150.0), (350.0, 150.0)]
        texture = [[True, True], [True, True]]

        fb_sparse = Framebuffer()
        stroke_bezier_texture_ball(fb_sparse, points, 0.5, texture, spacing=10.0)

        fb_dense = Framebuffer()
        stroke_bezier_texture_ball(fb_dense, points, 0.5, texture, spacing=2.0)

        # Count pixels
        sparse_count = sum(
            1 for y in range(fb_sparse.HEIGHT) for x in range(fb_sparse.WIDTH)
            if fb_sparse.get_pixel(x, y)
        )
        dense_count = sum(
            1 for y in range(fb_dense.HEIGHT) for x in range(fb_dense.WIDTH)
            if fb_dense.get_pixel(x, y)
        )

        assert dense_count >= sparse_count

    def test_empty_points_no_crash(self):
        """Empty point list doesn't crash."""
        fb = Framebuffer()
        stroke_bezier_texture_ball(fb, [], 0.5, [[True]])
        # Should complete without error

    def test_default_texture_works(self):
        """DEFAULT_BALL_8X8 works with stroke function."""
        fb = Framebuffer()

        stroke_bezier_texture_ball(
            fb,
            [(100.0, 150.0), (300.0, 150.0)],
            0.5,
            DEFAULT_BALL_8X8,
            spacing=4.0,
        )

        # Should have drawn something
        pixel_count = sum(
            1 for y in range(fb.HEIGHT) for x in range(fb.WIDTH) if fb.get_pixel(x, y)
        )
        assert pixel_count > 0


class TestDrawBezierCurve:
    """Tests for draw_bezier_curve function."""

    def test_draws_curve(self):
        """Curve is drawn on framebuffer."""
        fb = Framebuffer()

        draw_bezier_curve(
            fb, [(50.0, 150.0), (200.0, 50.0), (350.0, 150.0)], smoothness=0.5
        )

        # Should have pixels
        pixel_count = sum(
            1 for y in range(fb.HEIGHT) for x in range(fb.WIDTH) if fb.get_pixel(x, y)
        )
        assert pixel_count > 0

    def test_color_parameter(self):
        """Color parameter works correctly."""
        fb = Framebuffer()
        fb.clear(True)  # Fill with black

        draw_bezier_curve(
            fb, [(50.0, 150.0), (350.0, 150.0)], smoothness=0.5, color=False
        )

        # Should have white pixels (curve erases black)
        white_count = sum(
            1 for y in range(fb.HEIGHT) for x in range(fb.WIDTH)
            if not fb.get_pixel(x, y)
        )
        assert white_count > 0

    def test_single_point_draws_pixel(self):
        """Single point draws a pixel."""
        fb = Framebuffer()

        draw_bezier_curve(fb, [(200.0, 150.0)], smoothness=0.5)

        assert fb.get_pixel(200, 150)

    def test_empty_points_no_crash(self):
        """Empty point list doesn't crash."""
        fb = Framebuffer()
        draw_bezier_curve(fb, [], smoothness=0.5)
        # Should complete without error


class TestIntegration:
    """Integration tests combining multiple bezier functions."""

    def test_smooth_wave_curve(self):
        """Draw a smooth wave pattern."""
        fb = Framebuffer()

        # Wave points
        points = [
            (50.0, 150.0),
            (100.0, 100.0),
            (150.0, 150.0),
            (200.0, 200.0),
            (250.0, 150.0),
            (300.0, 100.0),
            (350.0, 150.0),
        ]

        stroke_bezier_texture_ball(fb, points, 0.5, DEFAULT_BALL_8X8, spacing=3.0)

        # Verify continuous stroke
        # Should have pixels at regular intervals
        pixels_found = 0
        for x in range(50, 351, 25):
            for y in range(75, 225):
                if fb.get_pixel(x, y):
                    pixels_found += 1
                    break

        assert pixels_found >= 8  # At least one pixel per 25-pixel column

    def test_closed_loop(self):
        """Draw a closed loop shape."""
        fb = Framebuffer()

        # Diamond shape
        points = [
            (200.0, 50.0),
            (350.0, 150.0),
            (200.0, 250.0),
            (50.0, 150.0),
            (200.0, 50.0),  # Close the loop
        ]

        stroke_bezier_texture_ball(fb, points, 0.7, DEFAULT_BALL_8X8, spacing=3.0)

        # Should form a continuous closed shape
        pixel_count = sum(
            1 for y in range(fb.HEIGHT) for x in range(fb.WIDTH) if fb.get_pixel(x, y)
        )
        assert pixel_count > 500  # Substantial number of pixels

    def test_auto_tangent_creates_smooth_handles(self):
        """Auto-tangent generates opposing handles at interior points."""
        points = [
            (0.0, 0.0),
            (50.0, 50.0),
            (100.0, 0.0),
        ]

        handles = auto_tangent(points, smoothness=0.5)

        # Middle point should have opposing handles
        h_in, h_out = handles[1]

        # Handles should be opposite directions (dot product negative)
        dot = h_in[0] * h_out[0] + h_in[1] * h_out[1]
        assert dot <= 0  # Opposite or perpendicular

    def test_bezier_through_all_points(self):
        """Verify curve passes through all specified points."""
        points = [
            (10.0, 10.0),
            (100.0, 50.0),
            (150.0, 30.0),
        ]

        handles = auto_tangent(points, smoothness=0.5)

        # Each segment should start and end at the specified points
        for i in range(len(points) - 1):
            p0 = points[i]
            p3 = points[i + 1]

            h_out = handles[i][1]
            h_in = handles[i + 1][0]

            p1 = (p0[0] + h_out[0], p0[1] + h_out[1])
            p2 = (p3[0] + h_in[0], p3[1] + h_in[1])

            # At t=0
            result = cubic_bezier(p0, p1, p2, p3, 0.0)
            assert abs(result[0] - p0[0]) < 1e-10
            assert abs(result[1] - p0[1]) < 1e-10

            # At t=1
            result = cubic_bezier(p0, p1, p2, p3, 1.0)
            assert abs(result[0] - p3[0]) < 1e-10
            assert abs(result[1] - p3[1]) < 1e-10
