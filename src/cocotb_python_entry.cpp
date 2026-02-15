// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cocotb.h>
#include <cocotb_python_runner.h>

using namespace cocotb;

// Generic entrypoint: select Python module/test through COCOTB_MODULE/COCOTB_TEST.
COCOTB_TEST(run_python_env_test)
task<> run_python_env_test(Dut &dut) {
    co_await cocotb_cpp::run_python_test(dut);
}

