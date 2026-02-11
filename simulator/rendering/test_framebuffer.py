"""
Tests for the Framebuffer class.

These tests verify:
- Basic buffer properties
- Pixel set/get operations
- Bit ordering (bit 7 = leftmost)
- Bounds checking
- fill_span optimization and edge cases
"""

import unittest
from rendering.framebuffer import Framebuffer


class TestFramebufferBasics(unittest.TestCase):
    """Test basic framebuffer properties and initialization."""

    def test_constants(self):
        """Verify class constants."""
        self.assertEqual(Framebuffer.WIDTH, 400)
        self.assertEqual(Framebuffer.HEIGHT, 300)
        self.assertEqual(Framebuffer.BYTES_PER_ROW, 50)

    def test_buffer_size(self):
        """Buffer should be 15,000 bytes (50 * 300)."""
        fb = Framebuffer()
        self.assertEqual(len(fb.buffer), 15000)

    def test_initial_state(self):
        """Buffer should start all white (zeros)."""
        fb = Framebuffer()
        self.assertTrue(all(b == 0 for b in fb.buffer))


class TestClear(unittest.TestCase):
    """Test the clear() method."""

    def test_clear_to_white(self):
        """clear(False) should fill with zeros."""
        fb = Framebuffer()
        fb.buffer[0] = 0xFF  # Dirty the buffer
        fb.clear(False)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_clear_to_black(self):
        """clear(True) should fill with 0xFF."""
        fb = Framebuffer()
        fb.clear(True)
        self.assertTrue(all(b == 0xFF for b in fb.buffer))

    def test_clear_default_is_white(self):
        """clear() with no argument should clear to white."""
        fb = Framebuffer()
        fb.buffer[0] = 0xFF
        fb.clear()
        self.assertTrue(all(b == 0 for b in fb.buffer))


class TestPixelOperations(unittest.TestCase):
    """Test set_pixel and get_pixel methods."""

    def test_set_and_get_pixel(self):
        """Basic set and get pixel operation."""
        fb = Framebuffer()
        fb.set_pixel(10, 20, True)
        self.assertTrue(fb.get_pixel(10, 20))
        self.assertFalse(fb.get_pixel(11, 20))

    def test_clear_pixel(self):
        """Setting a pixel to False should clear it."""
        fb = Framebuffer()
        fb.set_pixel(10, 20, True)
        fb.set_pixel(10, 20, False)
        self.assertFalse(fb.get_pixel(10, 20))

    def test_bit_ordering_leftmost(self):
        """Bit 7 should be the leftmost pixel (x=0 in byte 0)."""
        fb = Framebuffer()
        fb.set_pixel(0, 0, True)
        # Bit 7 should be set in byte 0
        self.assertEqual(fb.buffer[0], 0x80)

    def test_bit_ordering_rightmost(self):
        """Bit 0 should be the rightmost pixel (x=7 in byte 0)."""
        fb = Framebuffer()
        fb.set_pixel(7, 0, True)
        # Bit 0 should be set in byte 0
        self.assertEqual(fb.buffer[0], 0x01)

    def test_bit_ordering_all_bits(self):
        """Test all 8 bit positions in a byte."""
        fb = Framebuffer()
        # Set pixels 0-7 in row 0
        for x in range(8):
            fb.set_pixel(x, 0, True)
        # Byte should be 0xFF
        self.assertEqual(fb.buffer[0], 0xFF)

    def test_bit_ordering_individual_bits(self):
        """Verify each bit position maps correctly."""
        expected = [0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01]
        for x, expected_byte in enumerate(expected):
            fb = Framebuffer()
            fb.set_pixel(x, 0, True)
            self.assertEqual(fb.buffer[0], expected_byte,
                             f"Pixel at x={x} should set byte to {expected_byte:#04x}")

    def test_second_byte(self):
        """Pixel x=8 should be in byte 1, bit 7."""
        fb = Framebuffer()
        fb.set_pixel(8, 0, True)
        self.assertEqual(fb.buffer[0], 0x00)
        self.assertEqual(fb.buffer[1], 0x80)

    def test_row_offset(self):
        """Pixels in row 1 should start at byte 50."""
        fb = Framebuffer()
        fb.set_pixel(0, 1, True)
        self.assertEqual(fb.buffer[50], 0x80)

    def test_corner_pixels(self):
        """Test all four corner pixels."""
        fb = Framebuffer()

        # Top-left (0, 0)
        fb.set_pixel(0, 0, True)
        self.assertTrue(fb.get_pixel(0, 0))

        # Top-right (399, 0)
        fb.set_pixel(399, 0, True)
        self.assertTrue(fb.get_pixel(399, 0))

        # Bottom-left (0, 299)
        fb.set_pixel(0, 299, True)
        self.assertTrue(fb.get_pixel(0, 299))

        # Bottom-right (399, 299)
        fb.set_pixel(399, 299, True)
        self.assertTrue(fb.get_pixel(399, 299))


class TestBoundsChecking(unittest.TestCase):
    """Test bounds checking for pixel operations."""

    def test_set_pixel_negative_x(self):
        """set_pixel with negative x should be a no-op."""
        fb = Framebuffer()
        fb.set_pixel(-1, 0, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_set_pixel_negative_y(self):
        """set_pixel with negative y should be a no-op."""
        fb = Framebuffer()
        fb.set_pixel(0, -1, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_set_pixel_x_too_large(self):
        """set_pixel with x >= WIDTH should be a no-op."""
        fb = Framebuffer()
        fb.set_pixel(400, 0, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_set_pixel_y_too_large(self):
        """set_pixel with y >= HEIGHT should be a no-op."""
        fb = Framebuffer()
        fb.set_pixel(0, 300, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_get_pixel_out_of_bounds(self):
        """get_pixel out of bounds should return False."""
        fb = Framebuffer()
        self.assertFalse(fb.get_pixel(-1, 0))
        self.assertFalse(fb.get_pixel(0, -1))
        self.assertFalse(fb.get_pixel(400, 0))
        self.assertFalse(fb.get_pixel(0, 300))


class TestFillSpan(unittest.TestCase):
    """Test fill_span method including optimization and edge cases."""

    def test_fill_span_basic(self):
        """Basic span fill."""
        fb = Framebuffer()
        fb.fill_span(0, 0, 8, True)
        # Should fill first byte completely
        self.assertEqual(fb.buffer[0], 0xFF)
        self.assertEqual(fb.buffer[1], 0x00)

    def test_fill_span_exclusive_end(self):
        """x_end is exclusive (like Python ranges)."""
        fb = Framebuffer()
        fb.fill_span(0, 0, 1, True)
        # Should only set bit 7 (x=0)
        self.assertEqual(fb.buffer[0], 0x80)

    def test_fill_span_partial_byte_start(self):
        """Span starting mid-byte."""
        fb = Framebuffer()
        fb.fill_span(0, 3, 8, True)
        # Should set bits 4-0 (x=3 through x=7)
        # Bits: 0001 1111 = 0x1F
        self.assertEqual(fb.buffer[0], 0x1F)

    def test_fill_span_partial_byte_end(self):
        """Span ending mid-byte."""
        fb = Framebuffer()
        fb.fill_span(0, 0, 5, True)
        # Should set bits 7-3 (x=0 through x=4)
        # Bits: 1111 1000 = 0xF8
        self.assertEqual(fb.buffer[0], 0xF8)

    def test_fill_span_within_single_byte(self):
        """Span entirely within one byte."""
        fb = Framebuffer()
        fb.fill_span(0, 2, 6, True)
        # Should set bits 5-2 (x=2 through x=5)
        # Bits: 0011 1100 = 0x3C
        self.assertEqual(fb.buffer[0], 0x3C)

    def test_fill_span_multiple_full_bytes(self):
        """Span covering multiple full bytes."""
        fb = Framebuffer()
        fb.fill_span(0, 0, 24, True)
        # Should fill bytes 0, 1, 2 completely
        self.assertEqual(fb.buffer[0], 0xFF)
        self.assertEqual(fb.buffer[1], 0xFF)
        self.assertEqual(fb.buffer[2], 0xFF)
        self.assertEqual(fb.buffer[3], 0x00)

    def test_fill_span_partial_both_ends(self):
        """Span with partial bytes at both ends."""
        fb = Framebuffer()
        fb.fill_span(0, 3, 21, True)
        # Byte 0: bits 4-0 (x=3-7) = 0x1F
        # Byte 1: full = 0xFF
        # Byte 2: bits 7-3 (x=16-20) = 0xF8
        self.assertEqual(fb.buffer[0], 0x1F)
        self.assertEqual(fb.buffer[1], 0xFF)
        self.assertEqual(fb.buffer[2], 0xF8)

    def test_fill_span_clear(self):
        """fill_span with color=False should clear pixels."""
        fb = Framebuffer()
        fb.clear(True)  # All black
        fb.fill_span(0, 0, 8, False)
        self.assertEqual(fb.buffer[0], 0x00)
        self.assertEqual(fb.buffer[1], 0xFF)

    def test_fill_span_empty(self):
        """Empty span (x_start == x_end) should do nothing."""
        fb = Framebuffer()
        fb.fill_span(0, 5, 5, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_fill_span_inverted(self):
        """Inverted span (x_start > x_end) should do nothing."""
        fb = Framebuffer()
        fb.fill_span(0, 10, 5, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_fill_span_y_out_of_bounds(self):
        """fill_span with y out of bounds should do nothing."""
        fb = Framebuffer()
        fb.fill_span(-1, 0, 10, True)
        fb.fill_span(300, 0, 10, True)
        self.assertTrue(all(b == 0 for b in fb.buffer))

    def test_fill_span_x_clipping_start(self):
        """fill_span should clip negative x_start to 0."""
        fb = Framebuffer()
        fb.fill_span(0, -5, 8, True)
        self.assertEqual(fb.buffer[0], 0xFF)

    def test_fill_span_x_clipping_end(self):
        """fill_span should clip x_end to WIDTH."""
        fb = Framebuffer()
        fb.fill_span(0, 392, 500, True)
        # x=392 to x=399 (last 8 pixels)
        # Byte 49 (last byte): x=392-399
        self.assertEqual(fb.buffer[49], 0xFF)

    def test_fill_span_full_row(self):
        """fill_span for entire row."""
        fb = Framebuffer()
        fb.fill_span(0, 0, 400, True)
        for i in range(50):
            self.assertEqual(fb.buffer[i], 0xFF)
        self.assertEqual(fb.buffer[50], 0x00)

    def test_fill_span_row_offset(self):
        """fill_span on row 1 should affect bytes 50+."""
        fb = Framebuffer()
        fb.fill_span(1, 0, 8, True)
        self.assertEqual(fb.buffer[0], 0x00)
        self.assertEqual(fb.buffer[50], 0xFF)

    def test_fill_span_preserves_adjacent_pixels(self):
        """fill_span should not affect pixels outside the span."""
        fb = Framebuffer()
        fb.set_pixel(0, 0, True)
        fb.set_pixel(7, 0, True)
        fb.fill_span(0, 2, 6, False)  # Clear middle
        self.assertTrue(fb.get_pixel(0, 0))
        self.assertFalse(fb.get_pixel(2, 0))
        self.assertFalse(fb.get_pixel(5, 0))
        self.assertTrue(fb.get_pixel(7, 0))

    def test_fill_span_single_pixel(self):
        """fill_span for single pixel (x_end = x_start + 1)."""
        fb = Framebuffer()
        fb.fill_span(0, 5, 6, True)
        # Only bit 2 should be set (x=5)
        self.assertEqual(fb.buffer[0], 0x04)

    def test_fill_span_byte_boundary_crossing(self):
        """Span that crosses exactly at byte boundary."""
        fb = Framebuffer()
        fb.fill_span(0, 6, 10, True)
        # Byte 0: bits 1-0 (x=6-7) = 0x03
        # Byte 1: bits 7-6 (x=8-9) = 0xC0
        self.assertEqual(fb.buffer[0], 0x03)
        self.assertEqual(fb.buffer[1], 0xC0)


class TestFillSpanConsistency(unittest.TestCase):
    """Test that fill_span produces same results as pixel-by-pixel."""

    def test_fill_span_matches_set_pixel(self):
        """fill_span should produce identical results to set_pixel loop."""
        for x_start in range(0, 20, 3):
            for x_end in range(x_start, 30, 5):
                for y in [0, 1, 150, 299]:
                    fb_span = Framebuffer()
                    fb_pixel = Framebuffer()

                    fb_span.fill_span(y, x_start, x_end, True)
                    for x in range(x_start, x_end):
                        fb_pixel.set_pixel(x, y, True)

                    self.assertEqual(
                        fb_span.buffer, fb_pixel.buffer,
                        f"Mismatch for span y={y}, x={x_start}:{x_end}"
                    )


class TestLastByte(unittest.TestCase):
    """Test operations on the last byte of rows (byte 49)."""

    def test_last_pixel_in_row(self):
        """Pixel x=399 should be in byte 49, bit 0."""
        fb = Framebuffer()
        fb.set_pixel(399, 0, True)
        self.assertEqual(fb.buffer[49], 0x01)

    def test_last_byte_fill_span(self):
        """fill_span at end of row."""
        fb = Framebuffer()
        fb.fill_span(0, 392, 400, True)
        # Byte 49: all 8 bits (x=392-399)
        self.assertEqual(fb.buffer[49], 0xFF)

    def test_last_row(self):
        """Operations on last row (y=299)."""
        fb = Framebuffer()
        fb.fill_span(299, 0, 400, True)
        # Last row starts at byte 299*50 = 14950
        for i in range(50):
            self.assertEqual(fb.buffer[14950 + i], 0xFF)


if __name__ == "__main__":
    unittest.main()
