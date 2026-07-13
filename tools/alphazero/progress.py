"""Small terminal progress renderer with rate and ETA, without extra dependencies."""

from __future__ import annotations

import sys
import time


def format_duration(seconds: float | None) -> str:
    if seconds is None or seconds == float("inf"):
        return "--:--"
    seconds = max(0, int(seconds))
    hours, remainder = divmod(seconds, 3600)
    minutes, secs = divmod(remainder, 60)
    return f"{hours:d}:{minutes:02d}:{secs:02d}" if hours else f"{minutes:02d}:{secs:02d}"


class ProgressBar:
    def __init__(self, phase: str, total: int, unit: str, enabled: bool = True,
                 stream=None, min_interval: float = 0.2):
        self.phase = phase
        self.total = max(total, 0)
        self.unit = unit
        self.enabled = enabled
        self.stream = stream or sys.stderr
        self.min_interval = min_interval
        self.started = time.monotonic()
        self.last_rendered = 0.0
        self.current = 0

    def line(self, current: int | None = None, now: float | None = None) -> str:
        current = self.current if current is None else current
        elapsed = max((time.monotonic() if now is None else now) - self.started, 1e-9)
        rate = current / elapsed
        remaining = (self.total - current) / rate if rate > 0 else None
        percent = 100.0 * current / self.total if self.total else 100.0
        return (f"{self.phase:<12} {current:>4}/{self.total:<4} {percent:6.1f}% | "
                f"{rate:7.2f} {self.unit}/s | elapsed {format_duration(elapsed)} | "
                f"ETA {format_duration(remaining)}")

    def update(self, current: int, force: bool = False) -> None:
        self.current = current
        if not self.enabled:
            return
        now = time.monotonic()
        if not force and current < self.total and now - self.last_rendered < self.min_interval:
            return
        self.last_rendered = now
        interactive = bool(getattr(self.stream, "isatty", lambda: False)())
        self.stream.write(("\r" if interactive else "") + self.line(now=now) +
                          ("" if interactive and current < self.total else "\n"))
        self.stream.flush()

    def finish(self) -> None:
        self.update(self.total, force=True)
