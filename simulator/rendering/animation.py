"""Animation utilities for 1-bit display simulator.

This module provides animation primitives for creating smooth, deterministic
animations suitable for pixel-based displays. Key features:

- Interpolation functions (lerp, easing curves)
- Breathing effects (scale and offset oscillations)
- Wiggle effects (per-vertex jitter for organic movement)
- Transition morphing (interpolate between point lists)
- AnimationState class for time-based animation management

All functions that produce coordinates round to integers for pixel stability.
Wiggle and breathing effects are deterministic - same inputs always produce
same outputs, making animations reproducible.
"""

import math
import time
from typing import Callable, Optional


def lerp(a: float, b: float, t: float) -> float:
    """Linear interpolation: a + t * (b - a).

    Args:
        a: Start value
        b: End value
        t: Interpolation factor (0.0 = a, 1.0 = b, can be outside [0,1])

    Returns:
        Interpolated value between a and b
    """
    return a + t * (b - a)


def ease_in_out(t: float) -> float:
    """Cubic smoothstep easing: 3t^2 - 2t^3.

    Input is clamped to [0, 1] for well-defined behavior.
    Starts slow, accelerates, then decelerates.

    Args:
        t: Input time factor (will be clamped to [0, 1])

    Returns:
        Eased value in [0, 1]
    """
    t = max(0.0, min(1.0, t))
    return 3 * t * t - 2 * t * t * t


def ease_in_out_sine(t: float) -> float:
    """Sine-based ease in-out curve.

    Uses cosine for smooth acceleration/deceleration.
    Input is clamped to [0, 1].

    Args:
        t: Input time factor (will be clamped to [0, 1])

    Returns:
        Eased value in [0, 1]
    """
    t = max(0.0, min(1.0, t))
    return (1 - math.cos(t * math.pi)) / 2


def breathing_scale(t: float, min_scale: float = 0.95, max_scale: float = 1.05,
                    period: float = 3.0) -> float:
    """Sine oscillation between min_scale and max_scale over period seconds.

    Creates a smooth breathing/pulsing effect for scaling transforms.

    Args:
        t: Time in seconds
        min_scale: Minimum scale factor (default 0.95)
        max_scale: Maximum scale factor (default 1.05)
        period: Full cycle duration in seconds (default 3.0)

    Returns:
        Current scale factor oscillating between min_scale and max_scale
    """
    # Handle zero period edge case
    if period == 0.0:
        return min_scale
    # Use sine wave, normalized to [0, 1], then scaled to [min_scale, max_scale]
    phase = (t / period) * 2 * math.pi
    normalized = (math.sin(phase) + 1) / 2  # [0, 1]
    return min_scale + normalized * (max_scale - min_scale)


def breathing_offset(t: float, amplitude: float = 2.0, period: float = 3.0) -> float:
    """Sine oscillation between -amplitude and +amplitude.

    Creates a smooth breathing/pulsing effect for position offsets.

    Args:
        t: Time in seconds
        amplitude: Maximum offset in either direction (default 2.0)
        period: Full cycle duration in seconds (default 3.0)

    Returns:
        Current offset oscillating between -amplitude and +amplitude
    """
    phase = (t / period) * 2 * math.pi
    return amplitude * math.sin(phase)


def wiggle_points(points: list[tuple[float, float]], amplitude: float,
                  frequency: float, t: float, seed: int = 0) -> list[tuple[float, float]]:
    """Deterministic per-vertex jitter using sin/cos with phase offsets.

    Each vertex gets a unique phase offset based on its index and the seed,
    creating organic-looking movement that is fully reproducible.

    Args:
        points: List of (x, y) coordinate tuples
        amplitude: Maximum displacement in pixels (use >= 1.0 for visible movement)
        frequency: Oscillation speed multiplier
        t: Time in seconds
        seed: Random seed for phase offsets (default 0)

    Returns:
        List of (x, y) tuples with integer-rounded coordinates for pixel stability
    """
    result = []
    for i, (x, y) in enumerate(points):
        # Unique phase offset per vertex based on index and seed
        phase_x = seed * 1.618 + i * 2.399  # Golden ratio-based offsets
        phase_y = seed * 2.718 + i * 3.141  # Different offset for y

        # Calculate displacement using sin/cos
        dx = amplitude * math.sin(t * frequency * 2 * math.pi + phase_x)
        dy = amplitude * math.cos(t * frequency * 2 * math.pi + phase_y)

        # Round to integers for pixel stability
        new_x = int(round(x + dx))
        new_y = int(round(y + dy))
        result.append((float(new_x), float(new_y)))

    return result


def wiggle_int_points(points: list[tuple[int, int]], amplitude: float,
                      frequency: float, t: float, seed: int = 0) -> list[tuple[int, int]]:
    """Helper for integer coordinate lists - returns integer tuples.

    Same as wiggle_points but takes and returns integer coordinates.

    Args:
        points: List of (x, y) integer coordinate tuples
        amplitude: Maximum displacement in pixels (use >= 1.0 for visible movement)
        frequency: Oscillation speed multiplier
        t: Time in seconds
        seed: Random seed for phase offsets (default 0)

    Returns:
        List of (x, y) integer tuples
    """
    float_points = [(float(x), float(y)) for x, y in points]
    wiggled = wiggle_points(float_points, amplitude, frequency, t, seed)
    return [(int(x), int(y)) for x, y in wiggled]


def transition_points(points_a: list[tuple[float, float]],
                      points_b: list[tuple[float, float]], t: float,
                      easing: Optional[Callable[[float], float]] = None) -> list[tuple[float, float]]:
    """Interpolate between two point lists (must be same length).

    Creates smooth morphing transitions between shapes with the same
    number of vertices.

    Args:
        points_a: Starting point list
        points_b: Ending point list (must have same length as points_a)
        t: Interpolation factor (0.0 = points_a, 1.0 = points_b)
        easing: Optional easing function to apply to t (default: linear)

    Returns:
        List of interpolated (x, y) tuples with integer-rounded coordinates

    Raises:
        ValueError: If point lists have different lengths
    """
    if len(points_a) != len(points_b):
        raise ValueError(
            f"Point lists must have same length: {len(points_a)} != {len(points_b)}"
        )

    # Apply easing if provided
    if easing is not None:
        t = easing(t)

    result = []
    for (x_a, y_a), (x_b, y_b) in zip(points_a, points_b):
        x = lerp(x_a, x_b, t)
        y = lerp(y_a, y_b, t)
        # Round to integers for pixel stability
        result.append((float(int(round(x))), float(int(round(y)))))

    return result


class AnimationState:
    """Manages animation timing and provides time-based animation methods.

    This class encapsulates a start time and provides convenient methods
    for applying time-based animations without manually tracking elapsed time.

    Example:
        anim = AnimationState()
        while running:
            scale = anim.breathing_scale()
            points = anim.wiggle_points(original_points)
            draw(points, scale)
    """

    def __init__(self, start_time: Optional[float] = None):
        """Initialize animation state.

        Args:
            start_time: Initial time in seconds (default: current time)
        """
        self.start_time = start_time if start_time is not None else time.time()

    def elapsed(self) -> float:
        """Return seconds since start.

        Returns:
            Elapsed time in seconds
        """
        return time.time() - self.start_time

    def reset(self) -> None:
        """Reset start time to now."""
        self.start_time = time.time()

    def breathing_scale(self, min_scale: float = 0.95, max_scale: float = 1.05,
                        period: float = 3.0) -> float:
        """Get current breathing scale based on elapsed time.

        Args:
            min_scale: Minimum scale factor (default 0.95)
            max_scale: Maximum scale factor (default 1.05)
            period: Full cycle duration in seconds (default 3.0)

        Returns:
            Current scale factor
        """
        return breathing_scale(self.elapsed(), min_scale, max_scale, period)

    def breathing_offset(self, amplitude: float = 2.0, period: float = 3.0) -> float:
        """Get current breathing offset based on elapsed time.

        Args:
            amplitude: Maximum offset in either direction (default 2.0)
            period: Full cycle duration in seconds (default 3.0)

        Returns:
            Current offset value
        """
        return breathing_offset(self.elapsed(), amplitude, period)

    def wiggle_points(self, points: list[tuple[float, float]],
                      amplitude: float = 1.5, frequency: float = 1.0,
                      seed: int = 0) -> list[tuple[float, float]]:
        """Apply wiggle to points based on elapsed time.

        Args:
            points: List of (x, y) coordinate tuples
            amplitude: Maximum displacement in pixels (default 1.5)
            frequency: Oscillation speed multiplier (default 1.0)
            seed: Random seed for phase offsets (default 0)

        Returns:
            List of wiggled (x, y) tuples
        """
        return wiggle_points(points, amplitude, frequency, self.elapsed(), seed)
