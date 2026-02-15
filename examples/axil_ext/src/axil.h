// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause


#include <cocotb.h>

using namespace cocotb;

class AxiLiteDriver {
  public:
    AxiLiteDriver(const Dut &dut, const Handle &clk) : dut_(dut), clk_(clk) {}

    task<> reset() {
        dut_["AWVALID"].value = 0;
        dut_["WVALID"].value = 0;
        dut_["BREADY"].value = 0;
        dut_["ARVALID"].value = 0;
        dut_["RREADY"].value = 0;
        co_await RisingEdge(clk_);
    }

    task<> write(uint32_t addr, uint32_t data, uint32_t wstrb = 0xF) {
        dut_["AWADDR"].value = addr;
        dut_["AWVALID"].value = 1;

        dut_["WDATA"].value = data;
        dut_["WSTRB"].value = wstrb;
        dut_["WVALID"].value = 1;

        while (true) {
            co_await RisingEdge(clk_);
            if (dut_["AWREADY"].value && dut_["WREADY"].value) {
                break;
            }
        }

        dut_["AWVALID"].value = 0;
        dut_["WVALID"].value = 0;

        dut_["BREADY"].value = 1;
        while (true) {
            co_await RisingEdge(clk_);
            if (dut_["BVALID"].value) {
                break;
            }
        }
        dut_["BREADY"].value = 0;
    }

    task<> read(uint32_t addr, uint32_t &out_data) {
        dut_["ARADDR"].value = addr;
        dut_["ARVALID"].value = 1;

        while (true) {
            co_await RisingEdge(clk_);
            if (dut_["ARREADY"].value) {
                break;
            }
        }

        dut_["ARVALID"].value = 0;

        dut_["RREADY"].value = 1;
        while (true) {
            co_await RisingEdge(clk_);
            if (dut_["RVALID"].value) {
                out_data = static_cast<uint32_t>(dut_["RDATA"].value);
                break;
            }
        }
        dut_["RREADY"].value = 0;
    }

  private:
    Handle dut_;
    Handle clk_;
};
