# This file is public domain, it can be freely copied without restrictions.
# SPDX-License-Identifier: CC0-1.0

import os
from pathlib import Path
import shutil

from cocotb_tools.runner import get_runner

from cmake_build import build_cpp_test_lib


def test_dff_cpp_runner():
    shutil.rmtree("sim_build", ignore_errors=True)

    sim = os.getenv("SIM", "icarus")

    proj_path = Path(__file__).resolve().parent

    sources = [proj_path / "dff.sv"]

    cpp_so = build_cpp_test_lib(proj_path, "cocotb_test_dff", "lib_dff")
    extra_env = {"GPI_USERS": f"{cpp_so},cocotb_entry_point"}

    runner = get_runner(sim)
    runner.build(
        sources=sources,
        hdl_toplevel="dff",
        always=True,
    )

    # do not check for xml results file for now
    os.environ.pop("PYTEST_CURRENT_TEST", None)

    runner.test(hdl_toplevel="dff", test_module="", extra_env=extra_env)


if __name__ == "__main__":
    test_dff_cpp_runner()
