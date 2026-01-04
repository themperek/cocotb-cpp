# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0
from __future__ import annotations

import os
import random
from pathlib import Path

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer
from cocotb_tools.runner import get_runner


@cocotb.test()
async def dff_test(dut):
    """Test that d propagates to q"""

    # Set initial input value to prevent it from floating
    dut.d.value = 0

    # Create a 10us period clock driver on port `clk`
    clock = Clock(dut.clk, 10, unit="us")
    # Start the clock. Start it low to avoid issues on the first RisingEdge
    clock.start(start_high=False)

    # Synchronize with the clock. This will register the initial `d` value
    await RisingEdge(dut.clk)

    expected_val = 0  # Matches initial input value
    for i in range(10):
        val = random.randint(0, 1)
        dut.d.value = val  # Assign the random value val to the input port d
        await RisingEdge(dut.clk)
        assert dut.q.value == expected_val, f"output q was incorrect on the {i}th cycle"
        expected_val = val  # Save random value for next RisingEdge

    # Check the final input on the next clock
    await RisingEdge(dut.clk)
    assert dut.q.value == expected_val, "output q was incorrect on the last cycle"


async def Wait(dut, time):
    await Timer(time)
    dut.d.value = 1
    dut.clk.value = 1


@cocotb.test()
async def test_dff_post(dut):
    dut.d.value = 0
    dut.clk.value = 0

    wait = cocotb.start_soon(Wait(dut, 100))

    await Wait(dut, 10)

    await wait

    await Timer(10)
    assert dut.d.value == 1, "output d was incorrect"


def test_dff_runner():
    sim = os.getenv("SIM", "icarus")

    proj_path = Path(__file__).resolve().parent

    sources = [proj_path / "dff.sv"]

    runner = get_runner(sim)
    runner.build(
        sources=sources,
        hdl_toplevel="dff",
        always=True,
    )

    runner.test(hdl_toplevel="dff", test_module="test_dff,")


if __name__ == "__main__":
    test_dff_runner()
