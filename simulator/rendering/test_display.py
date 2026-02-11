"""
Tests for the Display class (pygame visualization wrapper).

These tests use mocking to avoid requiring an actual pygame display.
They verify:
- Initialization with framebuffer and scale
- Rendering logic (framebuffer reading, color mapping)
- Screenshot saving
- Event handling (quit on ESC, Q, window close)
"""

import sys
import unittest
from unittest.mock import MagicMock, patch

# Create a comprehensive pygame mock before any imports
pygame_mock = MagicMock()
pygame_mock.QUIT = 256  # pygame.QUIT constant
pygame_mock.KEYDOWN = 768  # pygame.KEYDOWN constant
pygame_mock.K_ESCAPE = 27
pygame_mock.K_q = 113

# Install the mock
sys.modules['pygame'] = pygame_mock

# Now import our modules - display will use the mock pygame
from framebuffer import Framebuffer
from display import Display


class TestDisplayInitialization(unittest.TestCase):
    """Test Display initialization."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_init_default_scale(self):
        """Display should initialize with default scale of 2."""
        fb = Framebuffer()
        display = Display(fb)

        self.assertEqual(display.fb, fb)
        self.assertEqual(display.scale, 2)
        self.assertEqual(display.width, 800)  # 400 * 2
        self.assertEqual(display.height, 600)  # 300 * 2

    def test_init_custom_scale(self):
        """Display should accept custom scale factor."""
        fb = Framebuffer()
        display = Display(fb, scale=3)

        self.assertEqual(display.scale, 3)
        self.assertEqual(display.width, 1200)  # 400 * 3
        self.assertEqual(display.height, 900)  # 300 * 3

    def test_init_scale_one(self):
        """Display should work with scale of 1 (no scaling)."""
        fb = Framebuffer()
        display = Display(fb, scale=1)

        self.assertEqual(display.scale, 1)
        self.assertEqual(display.width, 400)
        self.assertEqual(display.height, 300)

    def test_init_calls_pygame_init(self):
        """Display initialization should call pygame.init()."""
        fb = Framebuffer()
        Display(fb)

        pygame_mock.init.assert_called()

    def test_init_sets_display_mode(self):
        """Display should set pygame display mode to correct size."""
        fb = Framebuffer()
        Display(fb, scale=2)

        pygame_mock.display.set_mode.assert_called_with((800, 600))

    def test_init_creates_surface(self):
        """Display should create surface at framebuffer resolution."""
        fb = Framebuffer()
        Display(fb)

        pygame_mock.Surface.assert_called_with((400, 300))


class TestDisplayRender(unittest.TestCase):
    """Test Display render method."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_render_reads_framebuffer(self):
        """Render should read pixels from framebuffer."""
        fb = Framebuffer()
        fb.set_pixel(10, 20, True)  # Set one pixel black

        display = Display(fb, scale=1)

        # Mock the pixel array and surface
        mock_pixels = MagicMock()
        pygame_mock.PixelArray.return_value = mock_pixels
        display.surface = MagicMock()
        display.surface.map_rgb.side_effect = lambda c: c

        display.render()

        # Verify pixel array was created
        pygame_mock.PixelArray.assert_called_once()

    def test_render_updates_display(self):
        """Render should call pygame.display.flip()."""
        fb = Framebuffer()
        display = Display(fb, scale=1)

        # Mock pixel array
        mock_pixels = MagicMock()
        pygame_mock.PixelArray.return_value = mock_pixels
        display.surface = MagicMock()
        display.surface.map_rgb.side_effect = lambda c: c

        display.render()

        pygame_mock.display.flip.assert_called()


class TestDisplayScreenshot(unittest.TestCase):
    """Test Display save_screenshot method."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_save_screenshot(self):
        """save_screenshot should save screen to file."""
        fb = Framebuffer()
        display = Display(fb)

        display.save_screenshot("/tmp/test.png")

        pygame_mock.image.save.assert_called_once()
        # Verify the path argument
        call_args = pygame_mock.image.save.call_args
        self.assertEqual(call_args[0][1], "/tmp/test.png")


class TestDisplayEventHandling(unittest.TestCase):
    """Test Display handle_events method."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_handle_events_no_events(self):
        """handle_events should return True when no quit events."""
        fb = Framebuffer()
        display = Display(fb)

        pygame_mock.event.get.return_value = []

        result = display.handle_events()
        self.assertTrue(result)

    def test_handle_events_quit_event(self):
        """handle_events should return False on QUIT event."""
        fb = Framebuffer()
        display = Display(fb)

        quit_event = MagicMock()
        quit_event.type = pygame_mock.QUIT
        pygame_mock.event.get.return_value = [quit_event]

        result = display.handle_events()
        self.assertFalse(result)

    def test_handle_events_escape_key(self):
        """handle_events should return False on ESC key."""
        fb = Framebuffer()
        display = Display(fb)

        key_event = MagicMock()
        key_event.type = pygame_mock.KEYDOWN
        key_event.key = pygame_mock.K_ESCAPE
        pygame_mock.event.get.return_value = [key_event]

        result = display.handle_events()
        self.assertFalse(result)

    def test_handle_events_q_key(self):
        """handle_events should return False on Q key."""
        fb = Framebuffer()
        display = Display(fb)

        key_event = MagicMock()
        key_event.type = pygame_mock.KEYDOWN
        key_event.key = pygame_mock.K_q
        pygame_mock.event.get.return_value = [key_event]

        result = display.handle_events()
        self.assertFalse(result)

    def test_handle_events_other_key(self):
        """handle_events should return True for non-quit keys."""
        fb = Framebuffer()
        display = Display(fb)

        key_event = MagicMock()
        key_event.type = pygame_mock.KEYDOWN
        key_event.key = ord('a')  # Some other key
        pygame_mock.event.get.return_value = [key_event]

        result = display.handle_events()
        self.assertTrue(result)


class TestDisplayClose(unittest.TestCase):
    """Test Display close method."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_close_calls_pygame_quit(self):
        """close() should call pygame.quit()."""
        fb = Framebuffer()
        display = Display(fb)

        display.close()

        pygame_mock.quit.assert_called()


class TestDisplayDoesNotModifyFramebuffer(unittest.TestCase):
    """Verify Display does not modify the framebuffer."""

    def setUp(self):
        """Set up mocks before each test."""
        pygame_mock.reset_mock()

    def test_render_does_not_modify_framebuffer(self):
        """Render should only read from framebuffer, not write."""
        fb = Framebuffer()
        # Set some pixels
        fb.set_pixel(0, 0, True)
        fb.set_pixel(100, 150, True)

        # Copy buffer state
        buffer_before = bytes(fb.buffer)

        display = Display(fb, scale=1)

        # Mock pixel array
        mock_pixels = MagicMock()
        pygame_mock.PixelArray.return_value = mock_pixels
        display.surface = MagicMock()
        display.surface.map_rgb.side_effect = lambda c: c

        display.render()

        # Buffer should be unchanged
        self.assertEqual(fb.buffer, bytearray(buffer_before))


class TestDisplayColorConstants(unittest.TestCase):
    """Test Display color constants."""

    def test_white_constant(self):
        """WHITE should be (255, 255, 255)."""
        self.assertEqual(Display.WHITE, (255, 255, 255))

    def test_black_constant(self):
        """BLACK should be (0, 0, 0)."""
        self.assertEqual(Display.BLACK, (0, 0, 0))


if __name__ == "__main__":
    unittest.main()
