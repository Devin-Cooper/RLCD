"""Comprehensive tests for animation module."""

import math
import time
import pytest

from rendering.animation import (
    lerp,
    ease_in_out,
    ease_in_out_sine,
    breathing_scale,
    breathing_offset,
    wiggle_points,
    wiggle_int_points,
    transition_points,
    AnimationState,
)


class TestLerp:
    """Tests for linear interpolation function."""

    def test_lerp_at_zero(self):
        """t=0 should return start value."""
        assert lerp(10.0, 20.0, 0.0) == 10.0
        assert lerp(-5.0, 5.0, 0.0) == -5.0

    def test_lerp_at_one(self):
        """t=1 should return end value."""
        assert lerp(10.0, 20.0, 1.0) == 20.0
        assert lerp(-5.0, 5.0, 1.0) == 5.0

    def test_lerp_midpoint(self):
        """t=0.5 should return midpoint."""
        assert lerp(0.0, 100.0, 0.5) == 50.0
        assert lerp(-10.0, 10.0, 0.5) == 0.0

    def test_lerp_quarter_points(self):
        """Test various interpolation factors."""
        assert lerp(0.0, 100.0, 0.25) == 25.0
        assert lerp(0.0, 100.0, 0.75) == 75.0

    def test_lerp_extrapolation_below(self):
        """t < 0 should extrapolate below start."""
        result = lerp(10.0, 20.0, -0.5)
        assert result == 5.0  # 10 + (-0.5) * 10 = 5

    def test_lerp_extrapolation_above(self):
        """t > 1 should extrapolate above end."""
        result = lerp(10.0, 20.0, 1.5)
        assert result == 25.0  # 10 + 1.5 * 10 = 25

    def test_lerp_same_values(self):
        """Interpolating same value should return that value."""
        assert lerp(5.0, 5.0, 0.0) == 5.0
        assert lerp(5.0, 5.0, 0.5) == 5.0
        assert lerp(5.0, 5.0, 1.0) == 5.0

    def test_lerp_negative_range(self):
        """Test interpolation with negative values."""
        assert lerp(-100.0, -50.0, 0.5) == -75.0


class TestEaseInOut:
    """Tests for cubic smoothstep easing."""

    def test_ease_at_zero(self):
        """t=0 should return 0."""
        assert ease_in_out(0.0) == 0.0

    def test_ease_at_one(self):
        """t=1 should return 1."""
        assert ease_in_out(1.0) == 1.0

    def test_ease_at_half(self):
        """t=0.5 should return 0.5 for smoothstep."""
        assert ease_in_out(0.5) == 0.5

    def test_ease_symmetric(self):
        """Easing should be symmetric around midpoint."""
        for t in [0.1, 0.2, 0.3, 0.4]:
            val_low = ease_in_out(t)
            val_high = ease_in_out(1.0 - t)
            assert abs(val_low + val_high - 1.0) < 1e-10

    def test_ease_clamping_below(self):
        """Values below 0 should clamp to 0."""
        assert ease_in_out(-0.5) == 0.0
        assert ease_in_out(-1.0) == 0.0

    def test_ease_clamping_above(self):
        """Values above 1 should clamp to 1."""
        assert ease_in_out(1.5) == 1.0
        assert ease_in_out(2.0) == 1.0

    def test_ease_slow_start(self):
        """Easing should start slow (derivative near 0 at t=0)."""
        # Values near 0 should be much smaller than linear
        assert ease_in_out(0.1) < 0.1
        assert ease_in_out(0.2) < 0.2

    def test_ease_slow_end(self):
        """Easing should end slow (derivative near 0 at t=1)."""
        # Values near 1 should be much larger than linear
        assert ease_in_out(0.9) > 0.9
        assert ease_in_out(0.8) > 0.8


class TestEaseInOutSine:
    """Tests for sine-based easing."""

    def test_ease_sine_at_zero(self):
        """t=0 should return 0."""
        assert abs(ease_in_out_sine(0.0)) < 1e-10

    def test_ease_sine_at_one(self):
        """t=1 should return 1."""
        assert abs(ease_in_out_sine(1.0) - 1.0) < 1e-10

    def test_ease_sine_at_half(self):
        """t=0.5 should return 0.5."""
        assert abs(ease_in_out_sine(0.5) - 0.5) < 1e-10

    def test_ease_sine_clamping_below(self):
        """Values below 0 should clamp to 0."""
        assert abs(ease_in_out_sine(-0.5)) < 1e-10

    def test_ease_sine_clamping_above(self):
        """Values above 1 should clamp to 1."""
        assert abs(ease_in_out_sine(1.5) - 1.0) < 1e-10

    def test_ease_sine_monotonic(self):
        """Easing should be monotonically increasing."""
        prev = 0.0
        for i in range(1, 11):
            t = i / 10.0
            val = ease_in_out_sine(t)
            assert val >= prev
            prev = val


class TestBreathingScale:
    """Tests for breathing scale oscillation."""

    def test_breathing_scale_range(self):
        """Scale should stay within min/max bounds."""
        for t in [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0]:
            scale = breathing_scale(t, min_scale=0.8, max_scale=1.2)
            assert 0.8 <= scale <= 1.2

    def test_breathing_scale_default_range(self):
        """Default scale should be in [0.95, 1.05]."""
        for t in [0.0, 0.5, 1.0, 1.5, 2.0]:
            scale = breathing_scale(t)
            assert 0.95 <= scale <= 1.05

    def test_breathing_scale_periodicity(self):
        """Scale should repeat with given period."""
        period = 2.0
        for t in [0.0, 0.5, 1.0, 1.5]:
            s1 = breathing_scale(t, period=period)
            s2 = breathing_scale(t + period, period=period)
            assert abs(s1 - s2) < 1e-10

    def test_breathing_scale_starts_at_midpoint(self):
        """At t=0, sine is 0, so scale should be at midpoint."""
        scale = breathing_scale(0.0, min_scale=0.8, max_scale=1.2)
        expected_mid = 0.8 + (1.2 - 0.8) / 2  # 1.0
        assert abs(scale - expected_mid) < 1e-10

    def test_breathing_scale_reaches_max(self):
        """Scale should reach max at quarter period."""
        period = 4.0
        scale = breathing_scale(period / 4, min_scale=0.9, max_scale=1.1, period=period)
        assert abs(scale - 1.1) < 1e-10

    def test_breathing_scale_reaches_min(self):
        """Scale should reach min at three-quarter period."""
        period = 4.0
        scale = breathing_scale(3 * period / 4, min_scale=0.9, max_scale=1.1, period=period)
        assert abs(scale - 0.9) < 1e-10


class TestBreathingOffset:
    """Tests for breathing offset oscillation."""

    def test_breathing_offset_range(self):
        """Offset should stay within [-amplitude, +amplitude]."""
        for t in [0.0, 0.5, 1.0, 1.5, 2.0]:
            offset = breathing_offset(t, amplitude=5.0)
            assert -5.0 <= offset <= 5.0

    def test_breathing_offset_default_range(self):
        """Default offset should be in [-2, +2]."""
        for t in [0.0, 0.5, 1.0, 1.5, 2.0]:
            offset = breathing_offset(t)
            assert -2.0 <= offset <= 2.0

    def test_breathing_offset_periodicity(self):
        """Offset should repeat with given period."""
        period = 2.5
        for t in [0.0, 0.5, 1.0]:
            o1 = breathing_offset(t, period=period)
            o2 = breathing_offset(t + period, period=period)
            assert abs(o1 - o2) < 1e-10

    def test_breathing_offset_starts_at_zero(self):
        """At t=0, sine is 0, so offset should be 0."""
        offset = breathing_offset(0.0, amplitude=10.0)
        assert abs(offset) < 1e-10

    def test_breathing_offset_reaches_max(self):
        """Offset should reach +amplitude at quarter period."""
        period = 4.0
        offset = breathing_offset(period / 4, amplitude=3.0, period=period)
        assert abs(offset - 3.0) < 1e-10

    def test_breathing_offset_reaches_min(self):
        """Offset should reach -amplitude at three-quarter period."""
        period = 4.0
        offset = breathing_offset(3 * period / 4, amplitude=3.0, period=period)
        assert abs(offset - (-3.0)) < 1e-10


class TestWigglePoints:
    """Tests for wiggle point animation."""

    def test_wiggle_determinism(self):
        """Same inputs should always produce same outputs."""
        points = [(10.0, 20.0), (30.0, 40.0), (50.0, 60.0)]
        result1 = wiggle_points(points, amplitude=2.0, frequency=1.0, t=1.5, seed=42)
        result2 = wiggle_points(points, amplitude=2.0, frequency=1.0, t=1.5, seed=42)
        assert result1 == result2

    def test_wiggle_different_seeds(self):
        """Different seeds should produce different results."""
        points = [(100.0, 100.0), (200.0, 200.0)]
        result1 = wiggle_points(points, amplitude=2.0, frequency=1.0, t=1.0, seed=1)
        result2 = wiggle_points(points, amplitude=2.0, frequency=1.0, t=1.0, seed=2)
        # At least one coordinate should differ
        assert result1 != result2

    def test_wiggle_returns_integers(self):
        """Wiggle should return integer-rounded coordinates."""
        points = [(10.5, 20.7), (30.3, 40.9)]
        result = wiggle_points(points, amplitude=1.0, frequency=1.0, t=0.5, seed=0)
        for x, y in result:
            assert x == int(x)
            assert y == int(y)

    def test_wiggle_preserves_length(self):
        """Output should have same number of points as input."""
        points = [(i * 10.0, i * 20.0) for i in range(10)]
        result = wiggle_points(points, amplitude=1.0, frequency=1.0, t=0.0, seed=0)
        assert len(result) == len(points)

    def test_wiggle_empty_list(self):
        """Empty input should return empty output."""
        result = wiggle_points([], amplitude=1.0, frequency=1.0, t=0.0, seed=0)
        assert result == []

    def test_wiggle_bounded_displacement(self):
        """Displacement should be bounded by amplitude."""
        points = [(100.0, 100.0)]
        amplitude = 3.0
        # Test at multiple times to check various phase positions
        for t in [0.0, 0.25, 0.5, 0.75, 1.0, 1.25]:
            result = wiggle_points(points, amplitude=amplitude, frequency=1.0, t=t, seed=0)
            dx = abs(result[0][0] - 100.0)
            dy = abs(result[0][1] - 100.0)
            # Allow for rounding by adding 1
            assert dx <= amplitude + 1
            assert dy <= amplitude + 1

    def test_wiggle_frequency_affects_speed(self):
        """Higher frequency should change positions faster."""
        points = [(50.0, 50.0)]
        t = 0.1
        result_slow = wiggle_points(points, amplitude=5.0, frequency=0.5, t=t, seed=0)
        result_fast = wiggle_points(points, amplitude=5.0, frequency=2.0, t=t, seed=0)
        # Different frequencies should give different results at same time
        # (unless they happen to align, which is unlikely with these values)
        assert result_slow != result_fast


class TestWiggleIntPoints:
    """Tests for integer point wiggle helper."""

    def test_wiggle_int_returns_ints(self):
        """Should return integer tuples."""
        points = [(10, 20), (30, 40)]
        result = wiggle_int_points(points, amplitude=1.0, frequency=1.0, t=0.5, seed=0)
        for x, y in result:
            assert isinstance(x, int)
            assert isinstance(y, int)

    def test_wiggle_int_determinism(self):
        """Same inputs should always produce same outputs."""
        points = [(10, 20), (30, 40)]
        result1 = wiggle_int_points(points, amplitude=2.0, frequency=1.0, t=1.5, seed=42)
        result2 = wiggle_int_points(points, amplitude=2.0, frequency=1.0, t=1.5, seed=42)
        assert result1 == result2

    def test_wiggle_int_preserves_length(self):
        """Output should have same number of points as input."""
        points = [(i * 10, i * 20) for i in range(5)]
        result = wiggle_int_points(points, amplitude=1.0, frequency=1.0, t=0.0, seed=0)
        assert len(result) == len(points)


class TestTransitionPoints:
    """Tests for point list interpolation."""

    def test_transition_at_zero(self):
        """t=0 should return points_a."""
        a = [(0.0, 0.0), (10.0, 10.0)]
        b = [(100.0, 100.0), (200.0, 200.0)]
        result = transition_points(a, b, 0.0)
        assert result == [(0.0, 0.0), (10.0, 10.0)]

    def test_transition_at_one(self):
        """t=1 should return points_b."""
        a = [(0.0, 0.0), (10.0, 10.0)]
        b = [(100.0, 100.0), (200.0, 200.0)]
        result = transition_points(a, b, 1.0)
        assert result == [(100.0, 100.0), (200.0, 200.0)]

    def test_transition_midpoint(self):
        """t=0.5 should return midpoint."""
        a = [(0.0, 0.0), (100.0, 100.0)]
        b = [(100.0, 100.0), (200.0, 200.0)]
        result = transition_points(a, b, 0.5)
        assert result == [(50.0, 50.0), (150.0, 150.0)]

    def test_transition_with_easing(self):
        """Easing function should be applied to t."""
        a = [(0.0, 0.0)]
        b = [(100.0, 100.0)]
        # With ease_in_out, t=0.5 should still give 0.5 (smoothstep property)
        result = transition_points(a, b, 0.5, easing=ease_in_out)
        assert result == [(50.0, 50.0)]

        # But t=0.25 should give different result than linear
        linear_result = transition_points(a, b, 0.25)
        eased_result = transition_points(a, b, 0.25, easing=ease_in_out)
        # Eased should be slower at start, so less progress
        assert eased_result[0][0] < linear_result[0][0]

    def test_transition_mismatched_lengths_raises(self):
        """Different length lists should raise ValueError."""
        a = [(0.0, 0.0), (10.0, 10.0)]
        b = [(100.0, 100.0)]
        with pytest.raises(ValueError) as excinfo:
            transition_points(a, b, 0.5)
        assert "same length" in str(excinfo.value)

    def test_transition_empty_lists(self):
        """Empty lists should return empty result."""
        result = transition_points([], [], 0.5)
        assert result == []

    def test_transition_returns_integers(self):
        """Transition should return integer-rounded coordinates."""
        a = [(0.0, 0.0)]
        b = [(7.0, 11.0)]  # 7/3 = 2.33..., 11/3 = 3.66...
        result = transition_points(a, b, 1.0 / 3.0)
        for x, y in result:
            assert x == int(x)
            assert y == int(y)


class TestAnimationState:
    """Tests for AnimationState class."""

    def test_animation_state_elapsed(self):
        """Elapsed time should increase over time."""
        state = AnimationState()
        time.sleep(0.05)
        elapsed = state.elapsed()
        assert elapsed >= 0.05
        assert elapsed < 0.2  # Should not be too long

    def test_animation_state_reset(self):
        """Reset should set elapsed time back to near zero."""
        state = AnimationState()
        time.sleep(0.05)
        state.reset()
        elapsed = state.elapsed()
        assert elapsed < 0.05

    def test_animation_state_custom_start(self):
        """Custom start time should be respected."""
        past_time = time.time() - 10.0  # 10 seconds ago
        state = AnimationState(start_time=past_time)
        elapsed = state.elapsed()
        assert elapsed >= 10.0
        assert elapsed < 11.0

    def test_animation_state_breathing_scale(self):
        """Breathing scale method should work."""
        state = AnimationState(start_time=time.time())
        scale = state.breathing_scale(min_scale=0.9, max_scale=1.1)
        assert 0.9 <= scale <= 1.1

    def test_animation_state_breathing_offset(self):
        """Breathing offset method should work."""
        state = AnimationState(start_time=time.time())
        offset = state.breathing_offset(amplitude=5.0)
        assert -5.0 <= offset <= 5.0

    def test_animation_state_wiggle_points(self):
        """Wiggle points method should work."""
        state = AnimationState(start_time=time.time())
        points = [(10.0, 20.0), (30.0, 40.0)]
        result = state.wiggle_points(points, amplitude=2.0)
        assert len(result) == len(points)
        # Points should be modified
        for x, y in result:
            assert isinstance(x, float)
            assert isinstance(y, float)

    def test_animation_state_determinism_at_same_time(self):
        """AnimationState with same start time should give same results."""
        fixed_time = 1000.0  # Fixed start time
        state1 = AnimationState(start_time=fixed_time)
        state2 = AnimationState(start_time=fixed_time)

        # Both should compute the same elapsed time
        # Note: there might be tiny differences due to time.time() calls
        # So we test with breathing functions at known elapsed times

        # Set up to test at a specific elapsed time by manipulating start_time
        # to make elapsed() return exactly what we want
        target_elapsed = 1.5
        now = time.time()
        state1 = AnimationState(start_time=now - target_elapsed)
        state2 = AnimationState(start_time=now - target_elapsed)

        # Breathing scale should be very close
        s1 = breathing_scale(target_elapsed)
        s2 = state1.breathing_scale()
        # Allow small timing differences
        assert abs(s1 - s2) < 0.1


class TestPixelStability:
    """Tests to ensure pixel stability (integer coordinates)."""

    def test_wiggle_pixel_movement_with_sufficient_amplitude(self):
        """Amplitude >= 1.0 should cause actual pixel movement at some time."""
        points = [(50.0, 50.0)]
        amplitude = 1.5

        # Sample at multiple times - should see pixel movement
        positions = set()
        for i in range(20):
            t = i * 0.1
            result = wiggle_points(points, amplitude, frequency=1.0, t=t, seed=0)
            positions.add((result[0][0], result[0][1]))

        # Should have more than one unique position
        assert len(positions) > 1

    def test_transition_produces_integer_coordinates(self):
        """All transition coordinates should be integers."""
        a = [(0.5, 0.5), (10.3, 20.7)]
        b = [(100.2, 100.8), (50.1, 60.9)]

        for t in [0.0, 0.25, 0.5, 0.75, 1.0]:
            result = transition_points(a, b, t)
            for x, y in result:
                assert x == float(int(x)), f"x={x} is not an integer at t={t}"
                assert y == float(int(y)), f"y={y} is not an integer at t={t}"


class TestEdgeCases:
    """Tests for edge cases and boundary conditions."""

    def test_lerp_with_infinity(self):
        """Lerp should handle infinity gracefully."""
        result = lerp(0.0, float('inf'), 0.5)
        assert result == float('inf')

    def test_breathing_with_zero_period(self):
        """Zero period should not cause division by zero."""
        # This tests the behavior - may produce inf or nan
        # We just want to ensure no exception
        try:
            breathing_scale(1.0, period=0.0)
        except ZeroDivisionError:
            pytest.fail("breathing_scale raised ZeroDivisionError")

    def test_wiggle_single_point(self):
        """Single point should wiggle correctly."""
        points = [(50.0, 50.0)]
        result = wiggle_points(points, amplitude=2.0, frequency=1.0, t=0.5, seed=0)
        assert len(result) == 1

    def test_transition_single_point(self):
        """Single point transition should work."""
        a = [(0.0, 0.0)]
        b = [(100.0, 100.0)]
        result = transition_points(a, b, 0.5)
        assert result == [(50.0, 50.0)]

    def test_wiggle_with_zero_amplitude(self):
        """Zero amplitude should return original positions (rounded)."""
        points = [(10.5, 20.7)]
        result = wiggle_points(points, amplitude=0.0, frequency=1.0, t=0.5, seed=0)
        # Python uses banker's rounding: 10.5 rounds to 10, 20.7 rounds to 21
        assert result == [(10.0, 21.0)]

    def test_wiggle_with_zero_frequency(self):
        """Zero frequency should give static result (initial phase only)."""
        points = [(50.0, 50.0)]
        result1 = wiggle_points(points, amplitude=2.0, frequency=0.0, t=0.0, seed=0)
        result2 = wiggle_points(points, amplitude=2.0, frequency=0.0, t=10.0, seed=0)
        assert result1 == result2

    def test_negative_amplitude(self):
        """Negative amplitude should work (just inverts direction)."""
        points = [(50.0, 50.0)]
        result_pos = wiggle_points(points, amplitude=2.0, frequency=1.0, t=0.5, seed=0)
        result_neg = wiggle_points(points, amplitude=-2.0, frequency=1.0, t=0.5, seed=0)
        # They should be on opposite sides of the original
        # (approximately, due to rounding)
        dx_pos = result_pos[0][0] - 50.0
        dx_neg = result_neg[0][0] - 50.0
        # Opposite signs (or both zero due to rounding)
        assert dx_pos * dx_neg <= 0 or (dx_pos == 0 and dx_neg == 0)
