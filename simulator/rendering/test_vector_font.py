"""
Tests for the vector_font module.

These tests verify:
- GLYPHS/NUMERALS dictionary structure and completeness
- render_numeral scaling and stroke width
- render_string character spacing and layout
- get_string_width calculations
- Layout helpers (centered, right-aligned, multiline)
- Edge cases and boundary conditions
"""

import unittest
from rendering.framebuffer import Framebuffer
from rendering.vector_font import (
    GLYPHS,
    NUMERALS,
    render_numeral,
    render_string,
    render_string_centered,
    render_string_right,
    render_multiline,
    get_string_width,
    _scale_point,
    _draw_thick_line,
    _get_char_width,
)


class TestGlyphsStructure(unittest.TestCase):
    """Test the GLYPHS dictionary structure."""

    def test_all_digits_defined(self):
        """All digits 0-9 should be defined."""
        for digit in '0123456789':
            self.assertIn(digit, GLYPHS, f"Digit '{digit}' should be defined")

    def test_all_uppercase_letters_defined(self):
        """All uppercase letters A-Z should be defined."""
        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            self.assertIn(letter, GLYPHS, f"Letter '{letter}' should be defined")

    def test_colon_defined(self):
        """Colon should be defined for clock display."""
        self.assertIn(':', GLYPHS)

    def test_additional_characters_defined(self):
        """Additional useful characters should be defined."""
        self.assertIn('-', GLYPHS)
        self.assertIn('.', GLYPHS)
        self.assertIn('/', GLYPHS)
        self.assertIn('°', GLYPHS)
        self.assertIn('%', GLYPHS)

    def test_numerals_is_alias_for_glyphs(self):
        """NUMERALS should be an alias for GLYPHS for backward compatibility."""
        self.assertIs(NUMERALS, GLYPHS)

    def test_glyph_structure(self):
        """Each glyph should be a list of strokes, each stroke a list of points."""
        for char, strokes in GLYPHS.items():
            self.assertIsInstance(strokes, list, f"'{char}' should have a list of strokes")
            self.assertGreater(len(strokes), 0, f"'{char}' should have at least one stroke")

            for i, stroke in enumerate(strokes):
                self.assertIsInstance(stroke, list,
                                     f"'{char}' stroke {i} should be a list")
                self.assertGreater(len(stroke), 0,
                                  f"'{char}' stroke {i} should have points")

                for j, point in enumerate(stroke):
                    self.assertIsInstance(point, tuple,
                                         f"'{char}' stroke {i} point {j} should be a tuple")
                    self.assertEqual(len(point), 2,
                                   f"'{char}' stroke {i} point {j} should have 2 coordinates")

    def test_coordinates_in_range(self):
        """All coordinates should be in 0-100 range."""
        for char, strokes in GLYPHS.items():
            for i, stroke in enumerate(strokes):
                for j, (x, y) in enumerate(stroke):
                    self.assertGreaterEqual(x, 0,
                        f"'{char}' stroke {i} point {j} x={x} should be >= 0")
                    self.assertLessEqual(x, 100,
                        f"'{char}' stroke {i} point {j} x={x} should be <= 100")
                    self.assertGreaterEqual(y, 0,
                        f"'{char}' stroke {i} point {j} y={y} should be >= 0")
                    self.assertLessEqual(y, 100,
                        f"'{char}' stroke {i} point {j} y={y} should be <= 100")


class TestNumeralsStructure(unittest.TestCase):
    """Test the NUMERALS dictionary structure (backward compatibility)."""

    def test_all_digits_defined(self):
        """All digits 0-9 should be defined."""
        for digit in '0123456789':
            self.assertIn(digit, NUMERALS, f"Digit '{digit}' should be defined")

    def test_colon_defined(self):
        """Colon should be defined for clock display."""
        self.assertIn(':', NUMERALS)

    def test_additional_characters_defined(self):
        """Additional useful characters should be defined."""
        self.assertIn('-', NUMERALS)
        self.assertIn('.', NUMERALS)


class TestScalePoint(unittest.TestCase):
    """Test the _scale_point helper function."""

    def test_origin(self):
        """Point (0, 0) should map to destination origin."""
        x, y = _scale_point(0, 0, 10, 20, 100, 50)
        self.assertEqual(x, 10)
        self.assertEqual(y, 20)

    def test_corner(self):
        """Point (100, 100) should map to opposite corner."""
        x, y = _scale_point(100, 100, 10, 20, 100, 50)
        self.assertEqual(x, 110)  # 10 + 100
        self.assertEqual(y, 70)   # 20 + 50

    def test_center(self):
        """Point (50, 50) should map to center of destination."""
        x, y = _scale_point(50, 50, 0, 0, 100, 100)
        self.assertEqual(x, 50)
        self.assertEqual(y, 50)

    def test_with_offset(self):
        """Scaling with offset destination."""
        x, y = _scale_point(50, 50, 100, 200, 80, 60)
        self.assertEqual(x, 140)  # 100 + 50 * 80 / 100 = 140
        self.assertEqual(y, 230)  # 200 + 50 * 60 / 100 = 230


class TestDrawThickLine(unittest.TestCase):
    """Test the _draw_thick_line helper function."""

    def test_stroke_width_1(self):
        """Stroke width 1 should behave like normal line."""
        fb = Framebuffer()
        _draw_thick_line(fb, 10, 10, 20, 10, 1, True)

        for x in range(10, 21):
            self.assertTrue(fb.get_pixel(x, 10))

    def test_stroke_width_3_horizontal(self):
        """Horizontal line with stroke width 3."""
        fb = Framebuffer()
        _draw_thick_line(fb, 10, 50, 30, 50, 3, True)

        # Center line should be filled
        for x in range(10, 31):
            self.assertTrue(fb.get_pixel(x, 50))

        # Adjacent lines should also be filled
        for x in range(10, 31):
            self.assertTrue(fb.get_pixel(x, 49))
            self.assertTrue(fb.get_pixel(x, 51))

    def test_stroke_width_3_vertical(self):
        """Vertical line with stroke width 3."""
        fb = Framebuffer()
        _draw_thick_line(fb, 50, 10, 50, 30, 3, True)

        # Center column should be filled
        for y in range(10, 31):
            self.assertTrue(fb.get_pixel(50, y))

        # Adjacent columns should also be filled
        for y in range(10, 31):
            self.assertTrue(fb.get_pixel(49, y))
            self.assertTrue(fb.get_pixel(51, y))

    def test_degenerate_point(self):
        """Degenerate case where start equals end."""
        fb = Framebuffer()
        _draw_thick_line(fb, 50, 50, 50, 50, 3, True)

        # Should fill a small area around the point
        self.assertTrue(fb.get_pixel(50, 50))


class TestRenderNumeral(unittest.TestCase):
    """Test render_numeral function."""

    def test_unknown_character(self):
        """Unknown character should not cause error or draw anything."""
        fb = Framebuffer()
        render_numeral(fb, '@', 10, 10, 50, 80)  # '@' is not defined

        # Buffer should be empty
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_basic_rendering(self):
        """Basic numeral rendering should set pixels."""
        fb = Framebuffer()
        render_numeral(fb, '0', 10, 10, 50, 80)

        # Count set pixels - should have some
        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_all_digits_render(self):
        """All digits should render without error."""
        for digit in '0123456789':
            fb = Framebuffer()
            render_numeral(fb, digit, 50, 50, 40, 60)

            # Each digit should have some pixels set
            count = sum(1 for x in range(100) for y in range(120)
                       if fb.get_pixel(x, y))
            self.assertGreater(count, 0, f"Digit '{digit}' should render pixels")

    def test_colon_renders(self):
        """Colon should render correctly."""
        fb = Framebuffer()
        render_numeral(fb, ':', 50, 50, 20, 60)

        count = sum(1 for x in range(100) for y in range(120)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0, "Colon should render pixels")

    def test_rendering_stays_in_bounds(self):
        """Rendered numeral should stay within bounding box (approximately)."""
        fb = Framebuffer()
        x, y, w, h = 100, 100, 50, 80
        render_numeral(fb, '8', x, y, w, h, stroke_width=2)

        # Check that no pixels are set far outside the bounding box
        margin = 5  # Allow small margin for stroke width
        for px in range(fb.WIDTH):
            for py in range(fb.HEIGHT):
                if fb.get_pixel(px, py):
                    self.assertGreaterEqual(px, x - margin,
                        f"Pixel at ({px}, {py}) is too far left")
                    self.assertLess(px, x + w + margin,
                        f"Pixel at ({px}, {py}) is too far right")
                    self.assertGreaterEqual(py, y - margin,
                        f"Pixel at ({px}, {py}) is too far up")
                    self.assertLess(py, y + h + margin,
                        f"Pixel at ({px}, {py}) is too far down")

    def test_stroke_width_affects_pixel_count(self):
        """Larger stroke width should result in more pixels."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_numeral(fb1, '1', 50, 50, 40, 60, stroke_width=1)
        render_numeral(fb2, '1', 50, 50, 40, 60, stroke_width=3)

        count1 = sum(1 for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                    if fb1.get_pixel(x, y))
        count2 = sum(1 for x in range(fb2.WIDTH) for y in range(fb2.HEIGHT)
                    if fb2.get_pixel(x, y))

        self.assertGreater(count2, count1,
                          "Stroke width 3 should have more pixels than width 1")

    def test_scaling(self):
        """Larger dimensions should result in larger rendered numeral."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_numeral(fb1, '0', 0, 0, 30, 40, stroke_width=1)
        render_numeral(fb2, '0', 0, 0, 60, 80, stroke_width=1)

        # Count pixels in each region
        count1 = sum(1 for x in range(40) for y in range(50)
                    if fb1.get_pixel(x, y))
        count2 = sum(1 for x in range(70) for y in range(90)
                    if fb2.get_pixel(x, y))

        # Larger numeral should have more pixels (proportional to scale)
        self.assertGreater(count2, count1)

    def test_white_on_black(self):
        """Rendering with color=False should draw white."""
        fb = Framebuffer()
        fb.clear(True)  # Fill with black
        render_numeral(fb, '5', 50, 50, 40, 60, color=False)

        # Some pixels should be white (False) in the numeral area
        white_count = sum(1 for x in range(50, 91) for y in range(50, 111)
                         if not fb.get_pixel(x, y))
        self.assertGreater(white_count, 0)


class TestRenderString(unittest.TestCase):
    """Test render_string function."""

    def test_empty_string(self):
        """Empty string should not draw anything."""
        fb = Framebuffer()
        render_string(fb, "", 10, 10, 30, 50)

        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_single_digit(self):
        """Single digit string should render."""
        fb = Framebuffer()
        render_string(fb, "5", 10, 10, 30, 50)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_multiple_digits(self):
        """Multiple digits should render with spacing."""
        fb = Framebuffer()
        render_string(fb, "123", 10, 10, 30, 50, spacing=5)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_time_format(self):
        """Clock time format should render correctly."""
        fb = Framebuffer()
        render_string(fb, "12:34", 10, 10, 30, 50)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_spacing_increases_width(self):
        """Larger spacing should spread characters further apart."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_string(fb1, "12", 10, 10, 30, 50, spacing=2)
        render_string(fb2, "12", 10, 10, 30, 50, spacing=20)

        # Find rightmost pixel in each
        rightmost1 = max((x for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                         if fb1.get_pixel(x, y)), default=0)
        rightmost2 = max((x for x in range(fb2.WIDTH) for y in range(fb2.HEIGHT)
                         if fb2.get_pixel(x, y)), default=0)

        self.assertGreater(rightmost2, rightmost1)

    def test_with_space(self):
        """Space character should advance position."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_string(fb1, "12", 10, 10, 30, 50)
        render_string(fb2, "1 2", 10, 10, 30, 50)

        rightmost1 = max((x for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                         if fb1.get_pixel(x, y)), default=0)
        rightmost2 = max((x for x in range(fb2.WIDTH) for y in range(fb2.HEIGHT)
                         if fb2.get_pixel(x, y)), default=0)

        self.assertGreater(rightmost2, rightmost1)

    def test_kwargs_passed_through(self):
        """Keyword arguments should be passed to render_numeral."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_string(fb1, "1", 10, 10, 30, 50, stroke_width=1)
        render_string(fb2, "1", 10, 10, 30, 50, stroke_width=4)

        count1 = sum(1 for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                    if fb1.get_pixel(x, y))
        count2 = sum(1 for x in range(fb2.WIDTH) for y in range(fb2.HEIGHT)
                    if fb2.get_pixel(x, y))

        self.assertGreater(count2, count1)


class TestGetStringWidth(unittest.TestCase):
    """Test get_string_width function."""

    def test_empty_string(self):
        """Empty string should have width 0."""
        self.assertEqual(get_string_width("", 30), 0)

    def test_single_digit(self):
        """Single digit should have width equal to char_width."""
        self.assertEqual(get_string_width("5", 30, spacing=0), 30)

    def test_multiple_digits(self):
        """Multiple digits should account for spacing."""
        # "12" = 30 + 4 + 30 = 64
        self.assertEqual(get_string_width("12", 30, spacing=4), 64)

    def test_colon_narrower(self):
        """Colon should be narrower than regular digit."""
        width_digit = get_string_width("1", 30, spacing=0)
        width_colon = get_string_width(":", 30, spacing=0)

        self.assertLess(width_colon, width_digit)

    def test_time_format(self):
        """Calculate width for clock time format."""
        # "12:34" = 30 + 4 + 30 + 4 + 15 + 4 + 30 + 4 + 30 = 151
        # (digits are 30, colon is 15, spacing is 4)
        width = get_string_width("12:34", 30, spacing=4)
        self.assertEqual(width, 151)

    def test_with_space(self):
        """Space should contribute half char_width."""
        # "1 2" = 30 + 4 + 15 + 4 + 30 = 83
        width = get_string_width("1 2", 30, spacing=4)
        self.assertEqual(width, 83)


class TestEdgeCases(unittest.TestCase):
    """Test edge cases and boundary conditions."""

    def test_tiny_dimensions(self):
        """Very small dimensions should still work."""
        fb = Framebuffer()
        render_numeral(fb, '8', 10, 10, 5, 8, stroke_width=1)

        # Should render something without crashing
        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_large_dimensions(self):
        """Large dimensions should work correctly."""
        fb = Framebuffer()
        render_numeral(fb, '0', 10, 10, 300, 250, stroke_width=5)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_at_boundaries(self):
        """Rendering at framebuffer boundaries should clip correctly."""
        fb = Framebuffer()
        render_numeral(fb, '1', 380, 10, 30, 50)

        # Should not crash, some pixels may be clipped
        # No assertion needed, just verify no crash

    def test_negative_position(self):
        """Rendering at negative position should clip correctly."""
        fb = Framebuffer()
        render_numeral(fb, '1', -10, -10, 30, 50)

        # Should not crash
        # Some pixels may be visible if they're within bounds

    def test_all_supported_chars_in_string(self):
        """All supported characters should render in a string."""
        fb = Framebuffer()
        render_string(fb, "0123456789:.-", 10, 10, 25, 40, spacing=2)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_all_letters_render(self):
        """All uppercase letters should render without error."""
        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZ':
            fb = Framebuffer()
            render_numeral(fb, letter, 50, 50, 40, 60)

            count = sum(1 for x in range(100) for y in range(120)
                       if fb.get_pixel(x, y))
            self.assertGreater(count, 0, f"Letter '{letter}' should render pixels")

    def test_punctuation_renders(self):
        """Punctuation characters should render correctly."""
        for char in '/%°':
            fb = Framebuffer()
            render_numeral(fb, char, 50, 50, 40, 60)

            count = sum(1 for x in range(100) for y in range(120)
                       if fb.get_pixel(x, y))
            self.assertGreater(count, 0, f"Character '{char}' should render pixels")


class TestGetCharWidth(unittest.TestCase):
    """Test the _get_char_width helper function."""

    def test_regular_char_full_width(self):
        """Regular characters should have full width."""
        self.assertEqual(_get_char_width('A', 30), 30)
        self.assertEqual(_get_char_width('0', 30), 30)

    def test_colon_half_width(self):
        """Colon should be half width."""
        self.assertEqual(_get_char_width(':', 30), 15)

    def test_period_third_width(self):
        """Period should be one-third width."""
        self.assertEqual(_get_char_width('.', 30), 10)

    def test_minus_two_thirds_width(self):
        """Minus should be two-thirds width."""
        self.assertEqual(_get_char_width('-', 30), 20)

    def test_slash_half_width(self):
        """Slash should be half width."""
        self.assertEqual(_get_char_width('/', 30), 15)

    def test_degree_third_width(self):
        """Degree symbol should be one-third width."""
        self.assertEqual(_get_char_width('°', 30), 10)

    def test_percent_full_width(self):
        """Percent should be full width."""
        self.assertEqual(_get_char_width('%', 30), 30)

    def test_space_half_width(self):
        """Space should be half width."""
        self.assertEqual(_get_char_width(' ', 30), 15)


class TestGetStringWidthExtended(unittest.TestCase):
    """Test get_string_width with new characters."""

    def test_slash_width(self):
        """Slash should contribute half char_width."""
        # "/" = 15
        width = get_string_width("/", 30, spacing=0)
        self.assertEqual(width, 15)

    def test_degree_width(self):
        """Degree symbol should contribute third char_width."""
        # "°" = 10
        width = get_string_width("°", 30, spacing=0)
        self.assertEqual(width, 10)

    def test_percent_width(self):
        """Percent should contribute full char_width."""
        # "%" = 30
        width = get_string_width("%", 30, spacing=0)
        self.assertEqual(width, 30)

    def test_mixed_string(self):
        """Mixed string with new characters."""
        # "25°C" = 30 + 4 + 30 + 4 + 10 + 4 + 30 = 112
        width = get_string_width("25°C", 30, spacing=4)
        self.assertEqual(width, 112)

    def test_letters_width(self):
        """Letters should have full width."""
        # "ABC" = 30 + 4 + 30 + 4 + 30 = 98
        width = get_string_width("ABC", 30, spacing=4)
        self.assertEqual(width, 98)


class TestRenderStringCentered(unittest.TestCase):
    """Test render_string_centered function."""

    def test_centered_string(self):
        """String should be centered on given x coordinate."""
        fb = Framebuffer()
        center_x = 200
        render_string_centered(fb, "ABC", center_x, 50, 30, 50, spacing=4)

        # Find leftmost and rightmost pixels
        pixels = [(x, y) for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                  if fb.get_pixel(x, y)]
        self.assertGreater(len(pixels), 0)

        leftmost = min(p[0] for p in pixels)
        rightmost = max(p[0] for p in pixels)
        center = (leftmost + rightmost) // 2

        # Center should be close to center_x (within a few pixels)
        self.assertAlmostEqual(center, center_x, delta=5)

    def test_centered_single_char(self):
        """Single character should be centered."""
        fb = Framebuffer()
        render_string_centered(fb, "X", 200, 50, 40, 60)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)


class TestRenderStringRight(unittest.TestCase):
    """Test render_string_right function."""

    def test_right_aligned_string(self):
        """String should end at given x coordinate."""
        fb = Framebuffer()
        right_x = 300
        render_string_right(fb, "123", right_x, 50, 30, 50, spacing=4)

        # Find rightmost pixel
        pixels = [(x, y) for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                  if fb.get_pixel(x, y)]
        self.assertGreater(len(pixels), 0)

        rightmost = max(p[0] for p in pixels)

        # Rightmost should be close to right_x (within stroke width)
        self.assertLessEqual(rightmost, right_x + 2)

    def test_right_aligned_single_char(self):
        """Single character should be right-aligned."""
        fb = Framebuffer()
        render_string_right(fb, "5", 250, 50, 40, 60)

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)


class TestRenderMultiline(unittest.TestCase):
    """Test render_multiline function."""

    def test_multiline_left_aligned(self):
        """Multiple lines should render with vertical spacing."""
        fb = Framebuffer()
        lines = ["AB", "CD"]
        render_multiline(fb, lines, 50, 50, 25, 40, line_spacing=10, align="left")

        # Should have pixels in both top and bottom regions
        top_count = sum(1 for x in range(fb.WIDTH) for y in range(50, 90)
                       if fb.get_pixel(x, y))
        bottom_count = sum(1 for x in range(fb.WIDTH) for y in range(100, 140)
                          if fb.get_pixel(x, y))

        self.assertGreater(top_count, 0, "Top line should have pixels")
        self.assertGreater(bottom_count, 0, "Bottom line should have pixels")

    def test_multiline_centered(self):
        """Centered multiline text."""
        fb = Framebuffer()
        lines = ["X", "XYZ"]
        render_multiline(fb, lines, 200, 50, 25, 40, line_spacing=10, align="center")

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_multiline_right_aligned(self):
        """Right-aligned multiline text."""
        fb = Framebuffer()
        lines = ["12", "345"]
        render_multiline(fb, lines, 300, 50, 25, 40, line_spacing=10, align="right")

        count = sum(1 for x in range(fb.WIDTH) for y in range(fb.HEIGHT)
                   if fb.get_pixel(x, y))
        self.assertGreater(count, 0)

    def test_empty_lines(self):
        """Empty lines list should not crash."""
        fb = Framebuffer()
        render_multiline(fb, [], 50, 50, 25, 40)

        # Should not crash, buffer should be empty
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_kwargs_passed_through(self):
        """Keyword arguments should be passed to render functions."""
        fb1 = Framebuffer()
        fb2 = Framebuffer()

        render_multiline(fb1, ["A"], 50, 50, 30, 50, stroke_width=1)
        render_multiline(fb2, ["A"], 50, 50, 30, 50, stroke_width=4)

        count1 = sum(1 for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                    if fb1.get_pixel(x, y))
        count2 = sum(1 for x in range(fb2.WIDTH) for y in range(fb2.HEIGHT)
                    if fb2.get_pixel(x, y))

        self.assertGreater(count2, count1)


class TestVisualOutput(unittest.TestCase):
    """Test visual rendering characteristics."""

    def test_eight_has_two_loops(self):
        """Digit 8 should have distinctive top and bottom regions."""
        fb = Framebuffer()
        render_numeral(fb, '8', 50, 50, 60, 100, stroke_width=2)

        # Check that there are pixels in both upper and lower halves
        upper_count = sum(1 for x in range(50, 111) for y in range(50, 100)
                         if fb.get_pixel(x, y))
        lower_count = sum(1 for x in range(50, 111) for y in range(100, 151)
                         if fb.get_pixel(x, y))

        self.assertGreater(upper_count, 0, "Digit 8 should have pixels in upper half")
        self.assertGreater(lower_count, 0, "Digit 8 should have pixels in lower half")

    def test_one_is_narrow(self):
        """Digit 1 should be relatively narrow."""
        fb1 = Framebuffer()
        fb8 = Framebuffer()

        render_numeral(fb1, '1', 50, 50, 60, 100, stroke_width=2)
        render_numeral(fb8, '8', 50, 50, 60, 100, stroke_width=2)

        # Find leftmost and rightmost pixels
        pixels_1 = [(x, y) for x in range(fb1.WIDTH) for y in range(fb1.HEIGHT)
                    if fb1.get_pixel(x, y)]
        pixels_8 = [(x, y) for x in range(fb8.WIDTH) for y in range(fb8.HEIGHT)
                    if fb8.get_pixel(x, y)]

        if pixels_1 and pixels_8:
            width_1 = max(p[0] for p in pixels_1) - min(p[0] for p in pixels_1)
            width_8 = max(p[0] for p in pixels_8) - min(p[0] for p in pixels_8)

            self.assertLess(width_1, width_8,
                           "Digit 1 should be narrower than digit 8")


if __name__ == "__main__":
    unittest.main()
