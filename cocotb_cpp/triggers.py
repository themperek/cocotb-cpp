"""Awaitables executed by the cocotb-cpp C++ runtime."""

from __future__ import annotations

from ._native import make_rising_edge_awaitable, make_timer_awaitable


def RisingEdge(signal):
    return make_rising_edge_awaitable(signal)


def Timer(time, unit="step"):
    return make_timer_awaitable(int(time), str(unit))


__all__ = ["RisingEdge", "Timer"]

