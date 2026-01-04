# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0
from __future__ import annotations

import os
from pathlib import Path
import shutil
import random

import cocotb
from cocotb.triggers import RisingEdge, Timer
from cocotb_tools.runner import get_runner


class AxiLiteDriver:
    def __init__(self, dut, clk):
        self.dut = dut
        self.clk = clk

    async def reset(self):
        self.dut.AWVALID.value = 0
        self.dut.WVALID.value = 0
        self.dut.BREADY.value = 0
        self.dut.ARVALID.value = 0
        self.dut.RREADY.value = 0
        await RisingEdge(self.clk)

    async def write(self, addr, data, wstrb=0xF):
        # Write address
        self.dut.AWADDR.value = addr
        self.dut.AWVALID.value = 1

        # Write data
        self.dut.WDATA.value = data
        self.dut.WSTRB.value = wstrb
        self.dut.WVALID.value = 1

        # Wait for address and data handshake
        while True:
            await RisingEdge(self.clk)
            if self.dut.AWREADY.value and self.dut.WREADY.value:
                break

        self.dut.AWVALID.value = 0
        self.dut.WVALID.value = 0

        # Wait for write response
        self.dut.BREADY.value = 1
        while True:
            await RisingEdge(self.clk)
            if self.dut.BVALID.value:
                break

        self.dut.BREADY.value = 0

    async def read(self, addr):
        # Read address
        self.dut.ARADDR.value = addr
        self.dut.ARVALID.value = 1

        # Wait for address handshake
        while True:
            await RisingEdge(self.clk)
            if self.dut.ARREADY.value:
                break

        self.dut.ARVALID.value = 0

        # Wait for read data
        self.dut.RREADY.value = 1
        while True:
            await RisingEdge(self.clk)
            if self.dut.RVALID.value:
                data = int(self.dut.RDATA.value)
                break

        self.dut.RREADY.value = 0
        return data


@cocotb.test()
async def axil_simple_test(dut):
    clk = dut.ACLK

    driver = AxiLiteDriver(dut, clk)

    # Reset sequence
    await driver.reset()
    while dut.ARESETn.value == 0:
        await RisingEdge(clk)

    mem = [0] * 1024

    for _ in range(1000):
        addr = random.randint(0, 1023)
        data = random.randint(0, 0xFFFFFFFF)
        mem[addr] = data
        await driver.write(addr * 4, data)

        addr_rd = random.randint(0, 1023)
        data_rd = await driver.read(addr_rd * 4)
        assert data_rd == mem[addr_rd], f"Read backaddress {addr_rd} 0x{data_rd:08X} from memory 0x{mem[addr_rd]:08X}"

    await driver.write(0x100, 0xDEADBEEF)
    data = await driver.read(0x100)
    assert data == 0xDEADBEEF, f"Read back 0x{data:08X}"

    await Timer(10, unit="ns")


def test_axil_runner():
    shutil.rmtree("sim_build", ignore_errors=True)

    sim = os.getenv("SIM", "icarus")

    proj_path = Path(__file__).resolve().parent

    sources = [proj_path / "axil.sv"]

    runner = get_runner(sim)
    runner.build(
        sources=sources,
        hdl_toplevel="top",
        always=True,
    )

    runner.test(hdl_toplevel="top", test_module="test_axil,")


if __name__ == "__main__":
    test_axil_runner()
