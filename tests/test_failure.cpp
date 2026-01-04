// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cocotb.h>

using namespace cocotb;

COCOTB_TEST(test_pass_before_failure)
task<> test_pass_before_failure([[maybe_unused]] Dut &dut) {
  co_await Timer(10);
}

COCOTB_TEST(test_failure)
task<> test_failure([[maybe_unused]] Dut &dut) {
  cocotb::log.info("Starting test_dff_failure coroutine");

    assert(false, "expected failure");

    co_await Timer(10);

}

COCOTB_TEST(test_pass_after_failure)
task<> test_pass_after_failure([[maybe_unused]] Dut &dut) {
  co_await Timer(10);
}
