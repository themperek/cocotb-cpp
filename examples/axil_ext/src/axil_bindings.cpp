// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include <nanobind/nanobind.h>

#include <cocotb_nanobind.h>
#include <cocotb_python_op.h>

#include "axil.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace {

class PyAxiLiteDriver {
  public:
    PyAxiLiteDriver(const nb::object &dut_obj, const nb::object &clk_obj)
        : dut_(cocotb_cpp::dut_from_py(dut_obj)),
          clk_(cocotb_cpp::handle_from_py(clk_obj)) {
        if (!dut_.valid()) {
            throw std::runtime_error("Invalid DUT handle passed to AxiLiteDriver.");
        }
        if (!clk_.valid()) {
            clk_ = dut_["ACLK"];
        }
        if (!clk_.valid()) {
            throw std::runtime_error("Invalid clock handle passed to AxiLiteDriver.");
        }
        driver_ = std::make_shared<AxiLiteDriver>(dut_, clk_);
    }

    nb::object reset() {
        auto driver = driver_;
        return cocotb_cpp::make_python_op_awaitable(
            cocotb_cpp::PythonOpResultKind::None,
            [driver](const std::shared_ptr<cocotb_cpp::PythonOpState> &state) {
                return cocotb_cpp::run_python_op(state, "axil: run_reset", [driver]() -> cocotb::task<> {
                    co_await driver->reset();
                });
            },
            "axil: reset"
        );
    }

    nb::object write(uint32_t addr, uint32_t data, uint32_t wstrb) {
        auto driver = driver_;
        return cocotb_cpp::make_python_op_awaitable(
            cocotb_cpp::PythonOpResultKind::None,
            [driver, addr, data, wstrb](const std::shared_ptr<cocotb_cpp::PythonOpState> &state) {
                return cocotb_cpp::run_python_op(state, "axil: run_write", [driver, addr, data, wstrb]() -> cocotb::task<> {
                    co_await driver->write(addr, data, wstrb);
                });
            },
            "axil: write"
        );
    }

    nb::object read(uint32_t addr) {
        auto driver = driver_;
        return cocotb_cpp::make_python_op_awaitable(
            cocotb_cpp::PythonOpResultKind::U32,
            [driver, addr](const std::shared_ptr<cocotb_cpp::PythonOpState> &state) {
                return cocotb_cpp::run_python_op(state, "axil: run_read", [driver, addr, state]() -> cocotb::task<> {
                    uint32_t out = 0;
                    co_await driver->read(addr, out);
                    state->value_u32 = out;
                });
            },
            "axil: read"
        );
    }

  private:
    cocotb::Dut dut_;
    cocotb::Handle clk_;
    std::shared_ptr<AxiLiteDriver> driver_;
};

} // namespace

NB_MODULE(axil, m) {
    m.doc() = "nanobind AXI-Lite cocotb-cpp driver wrapper";

    nb::class_<PyAxiLiteDriver>(m, "AxiLiteDriver")
        .def(nb::init<const nb::object &, const nb::object &>(), "dut"_a, "clk"_a)
        .def("reset", &PyAxiLiteDriver::reset)
        .def("write", &PyAxiLiteDriver::write, "addr"_a, "data"_a, "wstrb"_a = 0xF)
        .def("read", &PyAxiLiteDriver::read, "addr"_a);
}
