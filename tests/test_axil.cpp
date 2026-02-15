// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause


#include <cocotb.h>
#include <cstdint>
#include <format>
#include <random>
#include "axil.h"

using namespace cocotb;

COCOTB_TEST(axil_simple_test) // registers the test
task<> axil_simple_test(Dut &dut) {
    cocotb::log.info("Starting test_axil coroutine");

    auto clk = dut["ACLK"];
    AxiLiteDriver driver(dut, clk);

    co_await driver.reset();
    while (dut["ARESETn"].value == 0) {
        co_await RisingEdge(clk);
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<uint32_t> addr_dist(0, 1023);
    std::uniform_int_distribution<uint32_t> data_dist(0, 0xFFFFFFFF);
    std::vector<uint32_t> mem(1024, 0);

    for (int i = 0; i < 1000000; ++i) {
        uint32_t addr = addr_dist(rng);
        uint32_t data = data_dist(rng);
        mem[addr] = data;
        co_await driver.write(addr * 4, data);

        uint32_t addr_rd = addr_dist(rng);
        uint32_t data_rd;
        co_await driver.read(addr_rd * 4, data_rd);
        assert(data_rd == mem[addr_rd], std::format("Read back 0x{:X} from address 0x{:X}", data_rd, addr_rd));
    }

    co_await driver.write(0x100, 0xDEADBEEF);
    uint32_t data_rd;
    co_await driver.read(0x100, data_rd);
    assert(data_rd == 0xDEADBEEF, std::format("Read back 0x{:X} from address 0x{:X}", data_rd, 0x100));

    co_await Timer(10, unit::us);

    cocotb::log.info("Completed test_axil coroutine");
}
