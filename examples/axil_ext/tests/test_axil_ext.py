# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import random
import shutil

from cocotb_tools.runner import get_runner

async def axil_cpp_py_simple_test(dut):
    from cocotb_cpp.triggers import RisingEdge, Timer
    from axil_ext import AxiLiteDriver

    clk = dut.ACLK
    driver = AxiLiteDriver(dut, clk)

    await driver.reset()
    while dut.ARESETn.value == 0:
        await RisingEdge(clk)

    mem = [0] * 1024

    for _ in range(1000000):
        addr = random.randint(0, 1023)
        data = random.randint(0, 0xFFFFFFFF)
        mem[addr] = data
        await driver.write(addr * 4, data)

        addr_rd = random.randint(0, 1023)
        data_rd = await driver.read(addr_rd * 4)
        assert data_rd == mem[addr_rd], f"Read back address {addr_rd} 0x{data_rd:08X} from memory 0x{mem[addr_rd]:08X}"

    await driver.write(0x100, 0xDEADBEEF)
    data = await driver.read(0x100)
    assert data == 0xDEADBEEF, f"Read back 0x{data:08X}"

    await Timer(10, unit="ns")


def _get_installed_pkg_dir(pkg_name: str) -> Path:
    spec = importlib.util.find_spec(pkg_name)
    if spec is None or not spec.submodule_search_locations:
        raise RuntimeError(f"{pkg_name} is not installed")
    return Path(next(iter(spec.submodule_search_locations))).resolve()


def _get_installed_cocotb_cpp_entry_path() -> Path:
    pkg_dir = _get_installed_pkg_dir("cocotb_cpp")
    candidates = sorted(pkg_dir.glob("cocotb_cpp_entry*"))
    shared = [p for p in candidates if p.suffix in {".so", ".dylib", ".pyd"}]
    if not shared:
        raise RuntimeError(f"Could not find cocotb_cpp_entry shared library in {pkg_dir}")
    return shared[0]


def test_axil_ext_runner():
    shutil.rmtree("sim_build", ignore_errors=True)

    repo_root = Path(__file__).resolve().parents[3]

    sim = os.getenv("SIM", "icarus")
    sources = [repo_root / "tests" / "axil.sv"]

    runner = get_runner(sim)
    runner.build(
        sources=sources,
        hdl_toplevel="top",
        always=True,
    )

    entry_so = _get_installed_cocotb_cpp_entry_path()

    extra_env = {
        "GPI_USERS": f"{entry_so},cocotb_entry_point",
        "COCOTB_MODULE": "test_axil_ext",
        "COCOTB_TEST": "axil_cpp_py_simple_test",
        "COCOTB_CPP_TESTS": str(Path(__file__).resolve().parent),
    }

    os.environ.pop("PYTEST_CURRENT_TEST", None)
    runner.test(hdl_toplevel="top", test_module="", extra_env=extra_env)


if __name__ == "__main__":
    test_axil_ext_runner()
