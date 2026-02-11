"""Data providers for clock display - time, date, and sensor data."""

import math
import time
from abc import ABC, abstractmethod
from dataclasses import dataclass
from datetime import datetime


@dataclass
class TimeData:
    """Current time information."""
    hour: int        # 0-23
    minute: int      # 0-59
    second: int      # 0-59
    hour_12: int     # 1-12
    is_pm: bool


@dataclass
class DateData:
    """Current date information."""
    year: int
    month: int       # 1-12
    day: int         # 1-31
    weekday: int     # 0-6 (Monday=0)
    weekday_name: str


@dataclass
class SensorData:
    """Sensor readings."""
    temperature: float   # Celsius
    humidity: float      # 0-100 percent


class TimeProvider(ABC):
    """Abstract time provider."""

    @abstractmethod
    def get_time(self) -> TimeData:
        pass

    @abstractmethod
    def get_date(self) -> DateData:
        pass


class SensorProvider(ABC):
    """Abstract sensor provider."""

    @abstractmethod
    def get_sensors(self) -> SensorData:
        pass


class SystemTimeProvider(TimeProvider):
    """Provides system time."""

    def get_time(self) -> TimeData:
        now = datetime.now()
        hour_12 = now.hour % 12
        if hour_12 == 0:
            hour_12 = 12
        return TimeData(
            hour=now.hour,
            minute=now.minute,
            second=now.second,
            hour_12=hour_12,
            is_pm=now.hour >= 12,
        )

    def get_date(self) -> DateData:
        now = datetime.now()
        weekdays = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
        return DateData(
            year=now.year,
            month=now.month,
            day=now.day,
            weekday=now.weekday(),
            weekday_name=weekdays[now.weekday()],
        )


class MockSensorProvider(SensorProvider):
    """Provides simulated sensor data with slow variation."""

    def __init__(self, base_temp: float = 22.0, base_humidity: float = 45.0):
        self.base_temp = base_temp
        self.base_humidity = base_humidity
        self.start_time = time.time()

    def get_sensors(self) -> SensorData:
        # Slowly varying sinusoidal values
        elapsed = time.time() - self.start_time

        # Temperature: varies +/- 3 degrees over ~2 minutes
        temp_variation = 3.0 * math.sin(elapsed / 60.0)
        temp = self.base_temp + temp_variation

        # Humidity: varies +/- 10% over ~3 minutes
        humidity_variation = 10.0 * math.sin(elapsed / 90.0 + 1.5)
        humidity = self.base_humidity + humidity_variation

        return SensorData(
            temperature=temp,
            humidity=max(0, min(100, humidity)),
        )


class StaticSensorProvider(SensorProvider):
    """Provides fixed sensor values for testing."""

    def __init__(self, temperature: float = 22.0, humidity: float = 45.0):
        self.temperature = temperature
        self.humidity = humidity

    def get_sensors(self) -> SensorData:
        return SensorData(
            temperature=self.temperature,
            humidity=self.humidity,
        )
