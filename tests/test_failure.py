# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0

import os
from pathlib import Path
import shutil

import cocotb_tools.config
from cocotb_tools.runner import get_runner
import subprocess
import pytest


@pytest.mark.xfail(reason="expected failure")
def test_failure_runner():
    shutil.rmtree("sim_build", ignore_errors=True)

    sim = os.getenv("SIM", "icarus")

    proj_path = Path(__file__).resolve().parent

    sources = [proj_path / "dff.sv"]

    cpp_src = proj_path / "test_failure.cpp"
    cpp_so = Path("sim_build/lib_failure.so").resolve()
    cocotb_include_path = cocotb_tools.config.share_dir / "include"
    extra_env = {"GPI_USERS": f"{cpp_so},cocotb_entry_point"}
    build_cpp_cmd = f"gcc -x c++ -std=c++20 -fPIC -shared \
        -Wall -Wextra -Wpedantic \
        -I. -I{proj_path / ".." / "src"} -I{cocotb_include_path} {cpp_src} \
        -lstdc++ \
        -o {cpp_so}".split()

    runner = get_runner(sim)
    runner.build(
        sources=sources,
        hdl_toplevel="dff",
        always=True,
    )

    result = subprocess.run(build_cpp_cmd)
    if result.returncode != 0:
        raise RuntimeError(f"Failed to build C++ shared library (exit code {result.returncode})")

    # do not check for xml results file for now
    os.environ.pop("PYTEST_CURRENT_TEST", None)

    runner.test(hdl_toplevel="dff", test_module="", extra_env=extra_env)


if __name__ == "__main__":
    test_failure_runner()
