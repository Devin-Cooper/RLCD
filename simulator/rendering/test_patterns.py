"""
Tests for the patterns module.

These tests verify:
- BAYER_4X4 matrix correctness
- Pattern class constants
- pattern_test function behavior
- fill_polygon_pattern with different patterns
- Pattern tiling from global (0,0)
"""

import unittest
from rendering.framebuffer import Framebuffer
from rendering.patterns import (
    BAYER_4X4,
    Pattern,
    pattern_test,
    fill_polygon_pattern,
)


class TestBayerMatrix(unittest.TestCase):
    """Test the Bayer 4x4 matrix."""

    def test_matrix_dimensions(self):
        """Matrix should be 4x4."""
        self.assertEqual(len(BAYER_4X4), 4)
        for row in BAYER_4X4:
            self.assertEqual(len(row), 4)

    def test_matrix_values_range(self):
        """All values should be in range 0-15."""
        for row in BAYER_4X4:
            for value in row:
                self.assertGreaterEqual(value, 0)
                self.assertLessEqual(value, 15)

    def test_matrix_values_unique(self):
        """All 16 values should be unique (0-15)."""
        values = set()
        for row in BAYER_4X4:
            for value in row:
                values.add(value)
        self.assertEqual(len(values), 16)
        self.assertEqual(values, set(range(16)))

    def test_matrix_correct_values(self):
        """Matrix should have the correct Bayer pattern values."""
        expected = [
            [0, 8, 2, 10],
            [12, 4, 14, 6],
            [3, 11, 1, 9],
            [15, 7, 13, 5],
        ]
        self.assertEqual(BAYER_4X4, expected)


class TestPatternClass(unittest.TestCase):
    """Test Pattern class constants."""

    def test_pattern_constants_defined(self):
        """All pattern constants should be defined."""
        self.assertEqual(Pattern.SOLID_BLACK, 0)
        self.assertEqual(Pattern.DENSE, 1)
        self.assertEqual(Pattern.MEDIUM, 2)
        self.assertEqual(Pattern.SPARSE, 3)
        self.assertEqual(Pattern.SOLID_WHITE, 4)

    def test_pattern_constants_unique(self):
        """All pattern constants should be unique."""
        patterns = [
            Pattern.SOLID_BLACK,
            Pattern.DENSE,
            Pattern.MEDIUM,
            Pattern.SPARSE,
            Pattern.SOLID_WHITE,
        ]
        self.assertEqual(len(set(patterns)), 5)


class TestPatternTest(unittest.TestCase):
    """Test pattern_test function."""

    def test_solid_black_always_true(self):
        """SOLID_BLACK should always return True."""
        for x in range(8):
            for y in range(8):
                self.assertTrue(
                    pattern_test(Pattern.SOLID_BLACK, x, y),
                    f"SOLID_BLACK should be True at ({x}, {y})"
                )

    def test_solid_white_always_false(self):
        """SOLID_WHITE should always return False."""
        for x in range(8):
            for y in range(8):
                self.assertFalse(
                    pattern_test(Pattern.SOLID_WHITE, x, y),
                    f"SOLID_WHITE should be False at ({x}, {y})"
                )

    def test_pattern_density_dense(self):
        """DENSE pattern should have ~75% fill (12/16 pixels in 4x4)."""
        filled_count = 0
        for x in range(4):
            for y in range(4):
                if pattern_test(Pattern.DENSE, x, y):
                    filled_count += 1
        # DENSE threshold is 12, so 12 pixels out of 16 should be filled
        self.assertEqual(filled_count, 12)

    def test_pattern_density_medium(self):
        """MEDIUM pattern should have ~50% fill (8/16 pixels in 4x4)."""
        filled_count = 0
        for x in range(4):
            for y in range(4):
                if pattern_test(Pattern.MEDIUM, x, y):
                    filled_count += 1
        # MEDIUM threshold is 8, so 8 pixels out of 16 should be filled
        self.assertEqual(filled_count, 8)

    def test_pattern_density_sparse(self):
        """SPARSE pattern should have ~25% fill (4/16 pixels in 4x4)."""
        filled_count = 0
        for x in range(4):
            for y in range(4):
                if pattern_test(Pattern.SPARSE, x, y):
                    filled_count += 1
        # SPARSE threshold is 4, so 4 pixels out of 16 should be filled
        self.assertEqual(filled_count, 4)

    def test_pattern_tiling(self):
        """Pattern should tile correctly from global (0,0)."""
        # Test that pattern repeats every 4 pixels
        for x in range(16):
            for y in range(16):
                result1 = pattern_test(Pattern.MEDIUM, x, y)
                result2 = pattern_test(Pattern.MEDIUM, x % 4, y % 4)
                self.assertEqual(
                    result1, result2,
                    f"Pattern should tile at ({x}, {y})"
                )

    def test_pattern_at_negative_coords(self):
        """Pattern should handle negative coordinates via modulo."""
        # pattern_test uses & 3 which works like modulo for negative numbers
        # in Python, -1 & 3 = 3, so patterns tile correctly
        for pattern in [Pattern.DENSE, Pattern.MEDIUM, Pattern.SPARSE]:
            result_pos = pattern_test(pattern, 3, 3)
            result_neg = pattern_test(pattern, -1, -1)
            self.assertEqual(
                result_pos, result_neg,
                f"Pattern {pattern} should tile correctly for negative coords"
            )


class TestFillPolygonPattern(unittest.TestCase):
    """Test fill_polygon_pattern function."""

    def test_fill_square_solid_black(self):
        """Fill square with SOLID_BLACK should fill all pixels."""
        fb = Framebuffer()
        points = [(10, 10), (20, 10), (20, 20), (10, 20)]
        fill_polygon_pattern(fb, points, Pattern.SOLID_BLACK)

        # Check interior pixels are filled (note: y=20 not filled per scanline algorithm)
        for x in range(10, 21):
            for y in range(10, 20):
                self.assertTrue(
                    fb.get_pixel(x, y),
                    f"Pixel ({x}, {y}) should be filled"
                )

    def test_fill_square_solid_white(self):
        """Fill square with SOLID_WHITE should not fill any pixels."""
        fb = Framebuffer()
        points = [(10, 10), (20, 10), (20, 20), (10, 20)]
        fill_polygon_pattern(fb, points, Pattern.SOLID_WHITE)

        # No pixels should be filled
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_fill_square_medium_density(self):
        """Fill square with MEDIUM should have ~50% filled pixels."""
        fb = Framebuffer()
        # Use a square that's a multiple of 4 for consistent pattern measurement
        points = [(0, 0), (8, 0), (8, 8), (0, 8)]
        fill_polygon_pattern(fb, points, Pattern.MEDIUM)

        filled_count = 0
        total_count = 0
        # Note: scanline algorithm doesn't fill y=8 (uses y < y1)
        for x in range(0, 9):
            for y in range(0, 8):
                total_count += 1
                if fb.get_pixel(x, y):
                    filled_count += 1

        # Should be close to 50% fill
        # 9 * 8 = 72 pixels, approximately 36 should be filled
        self.assertAlmostEqual(filled_count / total_count, 0.5, delta=0.1)

    def test_fill_square_dense_density(self):
        """Fill square with DENSE should have ~75% filled pixels."""
        fb = Framebuffer()
        points = [(0, 0), (8, 0), (8, 8), (0, 8)]
        fill_polygon_pattern(fb, points, Pattern.DENSE)

        filled_count = 0
        total_count = 0
        for x in range(0, 9):
            for y in range(0, 8):
                total_count += 1
                if fb.get_pixel(x, y):
                    filled_count += 1

        # Should be close to 75% fill
        self.assertAlmostEqual(filled_count / total_count, 0.75, delta=0.1)

    def test_fill_square_sparse_density(self):
        """Fill square with SPARSE should have ~25% filled pixels."""
        fb = Framebuffer()
        points = [(0, 0), (8, 0), (8, 8), (0, 8)]
        fill_polygon_pattern(fb, points, Pattern.SPARSE)

        filled_count = 0
        total_count = 0
        for x in range(0, 9):
            for y in range(0, 8):
                total_count += 1
                if fb.get_pixel(x, y):
                    filled_count += 1

        # Should be close to 25% fill
        self.assertAlmostEqual(filled_count / total_count, 0.25, delta=0.1)

    def test_fill_triangle(self):
        """Fill triangle with pattern."""
        fb = Framebuffer()
        points = [(50, 10), (10, 50), (90, 50)]
        fill_polygon_pattern(fb, points, Pattern.MEDIUM)

        # Check some interior points - pattern should have some filled
        # At least some interior pixels should be filled
        filled_interior = any(
            fb.get_pixel(50, y) for y in range(20, 50)
        )
        self.assertTrue(filled_interior, "Triangle interior should have some filled pixels")

    def test_fill_too_few_points(self):
        """Fill with fewer than 3 points should do nothing."""
        fb = Framebuffer()
        fill_polygon_pattern(fb, [(10, 10), (20, 20)], Pattern.SOLID_BLACK)
        self.assertTrue(all(b == 0 for b in fb.buffer))

        fb2 = Framebuffer()
        fill_polygon_pattern(fb2, [(10, 10)], Pattern.SOLID_BLACK)
        self.assertTrue(all(b == 0 for b in fb2.buffer))

        fb3 = Framebuffer()
        fill_polygon_pattern(fb3, [], Pattern.SOLID_BLACK)
        self.assertTrue(all(b == 0 for b in fb3.buffer))

    def test_pattern_alignment_global(self):
        """Two adjacent polygons should have aligned patterns."""
        fb = Framebuffer()
        # Two adjacent squares
        points1 = [(0, 0), (8, 0), (8, 8), (0, 8)]
        points2 = [(8, 0), (16, 0), (16, 8), (8, 8)]

        fill_polygon_pattern(fb, points1, Pattern.MEDIUM)
        fill_polygon_pattern(fb, points2, Pattern.MEDIUM)

        # The pattern should be continuous across the boundary
        # Check that the same global position has the same pattern
        # at x=8, the pattern should match what pattern_test returns
        for y in range(0, 8):
            expected = pattern_test(Pattern.MEDIUM, 8, y)
            actual = fb.get_pixel(8, y)
            self.assertEqual(
                expected, actual,
                f"Pattern should be aligned at ({8}, {y})"
            )

    def test_pattern_at_offset_position(self):
        """Pattern at offset position should match global tiling."""
        fb = Framebuffer()
        # A square not aligned to 4x4 boundary
        points = [(5, 5), (13, 5), (13, 13), (5, 13)]
        fill_polygon_pattern(fb, points, Pattern.MEDIUM)

        # Each pixel should match what pattern_test returns
        for x in range(5, 14):
            for y in range(5, 13):  # y=13 not filled by scanline
                expected = pattern_test(Pattern.MEDIUM, x, y)
                actual = fb.get_pixel(x, y)
                self.assertEqual(
                    expected, actual,
                    f"Pattern should match at ({x}, {y})"
                )


class TestPatternIntegration(unittest.TestCase):
    """Integration tests for patterns."""

    def test_all_patterns_different(self):
        """Each pattern should produce different results."""
        results = {}
        for pattern in [
            Pattern.SOLID_BLACK,
            Pattern.DENSE,
            Pattern.MEDIUM,
            Pattern.SPARSE,
            Pattern.SOLID_WHITE,
        ]:
            fb = Framebuffer()
            points = [(0, 0), (16, 0), (16, 16), (0, 16)]
            fill_polygon_pattern(fb, points, pattern)
            count = sum(1 for x in range(17) for y in range(16) if fb.get_pixel(x, y))
            results[pattern] = count

        # All counts should be different
        self.assertEqual(len(set(results.values())), 5)

        # Verify ordering: SOLID_BLACK > DENSE > MEDIUM > SPARSE > SOLID_WHITE
        self.assertGreater(results[Pattern.SOLID_BLACK], results[Pattern.DENSE])
        self.assertGreater(results[Pattern.DENSE], results[Pattern.MEDIUM])
        self.assertGreater(results[Pattern.MEDIUM], results[Pattern.SPARSE])
        self.assertGreater(results[Pattern.SPARSE], results[Pattern.SOLID_WHITE])

    def test_large_polygon_performance(self):
        """Large polygon should complete in reasonable time."""
        import time
        fb = Framebuffer()
        points = [(0, 0), (399, 0), (399, 299), (0, 299)]

        start = time.time()
        fill_polygon_pattern(fb, points, Pattern.MEDIUM)
        elapsed = time.time() - start

        # Should complete in under 2 seconds (generous limit for pure Python)
        self.assertLess(elapsed, 2.0)

        # Should have approximately 50% pixels filled
        total = fb.WIDTH * (fb.HEIGHT - 1)  # y=299 not filled
        filled = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT - 1) if fb.get_pixel(x, y))
        self.assertAlmostEqual(filled / total, 0.5, delta=0.05)


if __name__ == "__main__":
    unittest.main()
