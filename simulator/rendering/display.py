"""
Pygame visualization wrapper for 1-bit framebuffer display.

This is the ONLY module in the rendering toolkit that depends on pygame.
All other modules (framebuffer, primitives, patterns, bezier, vector_font)
are pure Python and portable to ESP32-S3 C++ firmware.
"""

import pygame

try:
    from .framebuffer import Framebuffer
except ImportError:
    from framebuffer import Framebuffer


class Display:
    """
    Pygame display wrapper for visualizing a 1-bit framebuffer.

    Takes a Framebuffer instance and renders it to a pygame window.
    Does not own or modify the framebuffer - only reads from it.

    Color mapping:
    - True/1 (black in framebuffer) -> BLACK on screen
    - False/0 (white in framebuffer) -> WHITE on screen
    """

    # Colors for rendering (RGB)
    WHITE = (255, 255, 255)
    BLACK = (0, 0, 0)

    def __init__(self, fb: Framebuffer, scale: int = 2):
        """
        Initialize pygame window at framebuffer size times scale.

        Args:
            fb: Framebuffer instance to display (not modified, only read)
            scale: Integer scale factor for the display (default 2)
        """
        self.fb = fb
        self.scale = scale
        self.width = fb.WIDTH * scale
        self.height = fb.HEIGHT * scale

        # Initialize pygame display
        pygame.init()
        self.screen = pygame.display.set_mode((self.width, self.height))
        pygame.display.set_caption("1-bit Display Simulator")

        # Create a surface at framebuffer resolution for efficient blitting
        self.surface = pygame.Surface((fb.WIDTH, fb.HEIGHT))

    def render(self) -> None:
        """
        Blit framebuffer contents to pygame screen.

        Reads all pixels from the framebuffer and updates the display.
        Uses intermediate surface for efficient scaling.
        """
        # Lock the surface for direct pixel access (faster than set_at)
        pixels = pygame.PixelArray(self.surface)

        # Map framebuffer colors to screen colors
        white_color = self.surface.map_rgb(self.WHITE)
        black_color = self.surface.map_rgb(self.BLACK)

        # Read each pixel from framebuffer and set on surface
        for y in range(self.fb.HEIGHT):
            for x in range(self.fb.WIDTH):
                color = black_color if self.fb.get_pixel(x, y) else white_color
                pixels[x, y] = color

        # Release the pixel array (required before other operations)
        del pixels

        # Scale and blit to screen
        if self.scale == 1:
            self.screen.blit(self.surface, (0, 0))
        else:
            scaled = pygame.transform.scale(self.surface, (self.width, self.height))
            self.screen.blit(scaled, (0, 0))

        # Update the display
        pygame.display.flip()

    def save_screenshot(self, path: str) -> None:
        """
        Save current screen contents to an image file.

        Args:
            path: File path to save the screenshot (should end in .png)
        """
        pygame.image.save(self.screen, path)

    def handle_events(self) -> bool:
        """
        Process pygame events.

        Handles quit events (window close, ESC key, Q key).

        Returns:
            False if quit requested, True otherwise.
        """
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                return False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE or event.key == pygame.K_q:
                    return False
        return True

    def close(self) -> None:
        """
        Clean up pygame resources.

        Call this when done with the display to properly shut down pygame.
        """
        pygame.quit()
