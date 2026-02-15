// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cstdint>
#include <string>

#include <nanobind/nanobind.h>

#include <cocotb_nanobind.h>

namespace nb = nanobind;
using namespace nb::literals;

namespace {

// Native module used by Python tests.
// It exposes simulator Handle objects plus trigger token factories that the
// C++ runner (cocotb_python_runner.h) understands.

class SimHandle {
  public:
    SimHandle() : SimHandle(std::string{}) {}
    explicit SimHandle(cocotb::Handle handle, std::string path) : handle_(std::move(handle)), path_(std::move(path)) {}
    explicit SimHandle(const std::string &path) {
        gpi_sim_hdl raw = cocotb_cpp::common::resolve_handle_from_path(path);
        if (!raw) {
            throw std::runtime_error("Could not resolve simulator handle at path '" + path + "'.");
        }
        handle_ = cocotb::Handle(raw);
        if (path.empty()) {
            const char *top_name = gpi_get_signal_name_str(raw);
            path_ = top_name ? std::string(top_name) : std::string{};
        } else {
            path_ = path;
        }
    }

    static SimHandle from_py(const nb::object &obj) { return SimHandle(cocotb_cpp::handle_from_py(obj), cocotb_cpp::get_path_from_py_handle(obj)); }

    SimHandle child(const std::string &name) const {
        cocotb::Handle child_handle = handle_[name];
        std::string child_path = path_.empty() ? name : (path_ + "." + name);
        return SimHandle(child_handle, std::move(child_path));
    }

    bool valid() const { return handle_.valid(); }
    uint32_t get() const { return static_cast<uint32_t>(handle_.value); }
    void set(uint32_t value) { handle_.value = value; }
    const std::string &path() const { return path_; }

  private:
    cocotb::Handle handle_;
    std::string path_;
};

nb::object make_timer_awaitable(uint64_t delay, const std::string &unit_name) {
    nb::tuple payload = nb::make_tuple(delay, unit_name);
    return cocotb_cpp::make_python_awaitable("timer", payload);
}

nb::object make_rising_edge_awaitable(const nb::object &signal) {
    const std::string path = cocotb_cpp::get_path_from_py_handle(signal);
    return cocotb_cpp::make_python_awaitable("rising_edge", nb::str(path.c_str()));
}

} // namespace

NB_MODULE(_native, m) {
    m.doc() = "nanobind bridge for cocotb-cpp primitives";

    nb::enum_<cocotb::unit>(m, "Unit")
        .value("fs", cocotb::unit::fs)
        .value("ps", cocotb::unit::ps)
        .value("ns", cocotb::unit::ns)
        .value("us", cocotb::unit::us)
        .value("ms", cocotb::unit::ms)
        .value("sec", cocotb::unit::sec)
        .value("step", cocotb::unit::step);

    nb::class_<SimHandle>(m, "Handle")
        .def(nb::init<>())
        .def(nb::init<const std::string &>(), "path"_a)
        .def_static("from_py", &SimHandle::from_py, "handle"_a)
        .def("child", &SimHandle::child, "name"_a)
        .def("valid", &SimHandle::valid)
        .def("__getattr__", &SimHandle::child, "name"_a)
        .def_prop_rw(
            "value",
            [](const SimHandle &self) { return self.get(); },
            [](SimHandle &self, int64_t value) { self.set(static_cast<uint32_t>(value)); })
        .def("get", &SimHandle::get)
        .def("set", [](SimHandle &self, int64_t value) { self.set(static_cast<uint32_t>(value)); }, "value"_a)
        .def_prop_ro("_path", &SimHandle::path)
        .def_prop_ro("path", &SimHandle::path);

    m.def("get_precision", &cocotb::get_precision);
    m.def("get_sim_time", &cocotb::get_sim_time, "time_unit"_a = cocotb::unit::step);
    m.def("unit_to_string", [](cocotb::unit u) { return std::string(cocotb_cpp::common::unit_to_string(u)); }, "unit"_a);
    m.def("unit_from_string", [](const std::string &unit_name) { return cocotb_cpp::common::unit_from_string(unit_name); }, "unit_name"_a);
    m.def("make_timer_awaitable", &make_timer_awaitable, "delay"_a, "unit_name"_a = "step");
    m.def("make_rising_edge_awaitable", &make_rising_edge_awaitable, "signal"_a);
}
