// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cocotb.h>

using namespace cocotb;

COCOTB_TEST(test_dff) // registers the test
task<> test_dff(Dut &dut) {
    cocotb::log.info("Starting test_dff coroutine");

    // Set initial input value to prevent it from floating
    dut["d"].value = 0;

    // Create a 10us period clock driver on port `clk`
    auto clock = Clock(dut, 10, unit::us);

    // Start the clock
    auto clk = start_soon(clock);

    // Synchronize with the clock. This will register the initial `d` value
    co_await RisingEdge(dut["clk"]);

    auto expected_val = 0; // Matches initial input value
    for (int i = 0; i < 10; ++i) {
        auto val = rand() % 2;
        dut["d"].value = val; // Assign the random value val to the input port d
        co_await RisingEdge(dut["clk"]);
        auto q_val = dut["q"].value;
        assert(expected_val == q_val, std::format("output q was incorrect on the {} th cycle", i));
        expected_val = val; // Save random value for next RisingEdge
    }

    co_await RisingEdge(dut["clk"]);
    assert(expected_val == dut["q"].value, "output q was incorrect on the last cycle");

    cocotb::log.info("test_dff completed successfully");
}

task<> Wait(Dut &dut, int time) {
    co_await Timer(time);
    dut["clk"].value = 0;
    co_await Timer(time);
    dut["d"].value = 1;
    co_await Timer(time);
    dut["clk"].value = 1;
    co_await Timer(time);
    dut["d"].value = 0;
}

COCOTB_TEST(test_dff_post)
task<> test_dff_post(Dut &dut) {
    cocotb::log.info("Starting test_dff_post coroutine");

    dut["d"].value = 0;
    dut["clk"].value = 0;

    auto d_int = int32_t(dut["d"].value);
    cocotb::log.info("d_int: {}", d_int);

    uint32_t d_uint = dut["d"].value;
    cocotb::log.info("d_uint: {}", d_uint);

    co_await Wait(dut, 20);
    co_await Timer(10);

    assert(1 == dut["q"].value, "1: output q was incorrect");

    co_await Timer(10);

    dut["clk"].value = 0;
    dut["d"].value = 0;
    co_await Timer(10);
    dut["clk"].value = 1;
    co_await Timer(10);
    dut["d"].value = 0;

    assert(0 == dut["q"].value, "2: output q was incorrect");

    co_await Wait(dut, 100);
    cocotb::log.info("Create wait coroutine");
    auto wait = start_soon(Wait(dut, 100));
    cocotb::log.info("Awaiting wait coroutine");
    co_await wait;
    cocotb::log.info("Awaited wait coroutine");

    co_await Timer(10);
    assert(1 == dut["q"].value, "3: output q was incorrect");

    co_await Timer(10, unit::us);

    auto sim_time = get_sim_time();
    cocotb::log.info("sim time: {}", sim_time);

    co_await Timer(10);
    assert(sim_time + 10 == get_sim_time());

    co_await Timer(10, unit::us);
    assert(sim_time + 10 + 10000 == get_sim_time());

    cocotb::log.info("Completed test_dff_post successfully");
}
