"""
Framebuffer implementation for 1-bit packed pixel display.

This module provides the core byte-buffer for a 400x300 1-bit display.
Designed to be portable to ESP32-S3 C++ firmware.

Bit ordering: Bit 7 = leftmost pixel in each byte (MSB first).
Color convention: True/1 = black, False/0 = white.
"""


class Framebuffer:
    """
    1-bit packed pixel framebuffer for 400x300 display.

    Memory layout:
    - 50 bytes per row (400 pixels / 8 bits per byte)
    - 15,000 bytes total (50 * 300)
    - Bit 7 is the leftmost pixel in each byte (MSB first)

    Color convention:
    - True/1 = black (ink)
    - False/0 = white (paper)
    """

    WIDTH = 400
    HEIGHT = 300
    BYTES_PER_ROW = 50  # 400 / 8

    def __init__(self):
        """Initialize framebuffer with all white pixels (zeros)."""
        self.buffer = bytearray(self.BYTES_PER_ROW * self.HEIGHT)

    def clear(self, color: bool = False) -> None:
        """
        Fill entire buffer with a single color.

        Args:
            color: True for black (all 1s), False for white (all 0s).
        """
        fill_value = 0xFF if color else 0x00
        for i in range(len(self.buffer)):
            self.buffer[i] = fill_value

    def set_pixel(self, x: int, y: int, color: bool) -> None:
        """
        Set a single pixel. Bounds-checked (out of bounds is a no-op).

        Args:
            x: X coordinate (0-399)
            y: Y coordinate (0-299)
            color: True for black, False for white
        """
        if x < 0 or x >= self.WIDTH or y < 0 or y >= self.HEIGHT:
            return

        byte_index = y * self.BYTES_PER_ROW + (x >> 3)
        bit_index = 7 - (x & 7)  # Bit 7 = leftmost pixel

        if color:
            self.buffer[byte_index] |= (1 << bit_index)
        else:
            self.buffer[byte_index] &= ~(1 << bit_index)

    def get_pixel(self, x: int, y: int) -> bool:
        """
        Read a single pixel.

        Args:
            x: X coordinate (0-399)
            y: Y coordinate (0-299)

        Returns:
            True for black, False for white. Returns False if out of bounds.
        """
        if x < 0 or x >= self.WIDTH or y < 0 or y >= self.HEIGHT:
            return False

        byte_index = y * self.BYTES_PER_ROW + (x >> 3)
        bit_index = 7 - (x & 7)  # Bit 7 = leftmost pixel

        return bool(self.buffer[byte_index] & (1 << bit_index))

    def fill_span(self, y: int, x_start: int, x_end: int, color: bool) -> None:
        """
        Optimized horizontal span fill using byte operations.

        Fills pixels from x_start (inclusive) to x_end (exclusive).
        Uses full-byte writes where possible for 8x speedup over pixel-by-pixel.

        Args:
            y: Y coordinate (0-299)
            x_start: Starting X coordinate (inclusive)
            x_end: Ending X coordinate (exclusive)
            color: True for black, False for white
        """
        # Bounds check on y
        if y < 0 or y >= self.HEIGHT:
            return

        # Clamp x coordinates to valid range
        if x_start < 0:
            x_start = 0
        if x_end > self.WIDTH:
            x_end = self.WIDTH

        # Empty or invalid span
        if x_start >= x_end:
            return

        row_offset = y * self.BYTES_PER_ROW

        # Calculate byte boundaries
        start_byte = x_start >> 3
        end_byte = (x_end - 1) >> 3  # Byte containing the last pixel

        start_bit = x_start & 7  # Bit position within start byte (0-7)
        end_bit = (x_end - 1) & 7  # Bit position of last pixel within end byte

        if start_byte == end_byte:
            # All pixels are in the same byte
            # Create mask for bits from start_bit to end_bit (inclusive)
            # Bit 7 = leftmost, so we need bits (7-start_bit) down to (7-end_bit)
            mask = 0
            for bit in range(start_bit, end_bit + 1):
                mask |= (1 << (7 - bit))

            byte_idx = row_offset + start_byte
            if color:
                self.buffer[byte_idx] |= mask
            else:
                self.buffer[byte_idx] &= ~mask
        else:
            # Span crosses multiple bytes
            # Track the range of full bytes to fill
            first_full_byte = start_byte
            last_full_byte = end_byte

            # Handle partial start byte (if not byte-aligned)
            if start_bit != 0:
                # Mask for bits from start_bit to 7 (rightward to end of byte)
                # These are bits (7-start_bit) down to 0
                mask = (1 << (8 - start_bit)) - 1
                byte_idx = row_offset + start_byte
                if color:
                    self.buffer[byte_idx] |= mask
                else:
                    self.buffer[byte_idx] &= ~mask
                first_full_byte = start_byte + 1

            # Handle partial end byte (if not byte-aligned)
            # end_bit is the bit position of the last pixel to fill
            # We need to fill bits 0 through end_bit (inclusive)
            if end_bit != 7:
                # Mask for bits from 0 to end_bit (leftward from start of byte)
                # These are bits 7 down to (7-end_bit), mask to 8 bits
                mask = (0xFF << (7 - end_bit)) & 0xFF
                byte_idx = row_offset + end_byte
                if color:
                    self.buffer[byte_idx] |= mask
                else:
                    self.buffer[byte_idx] &= ~mask
                last_full_byte = end_byte - 1

            # Fill full bytes in the middle (the fast path)
            if first_full_byte <= last_full_byte:
                fill_value = 0xFF if color else 0x00
                for byte_idx in range(row_offset + first_full_byte,
                                      row_offset + last_full_byte + 1):
                    self.buffer[byte_idx] = fill_value
