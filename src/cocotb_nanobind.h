// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Python.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <nanobind/eval.h>
#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>

#ifdef assert
#undef assert
#endif

#include <cocotb_cpp_common.h>
#include <cocotb_python_op.h>
#include <cocotb.h>

namespace cocotb_cpp {

namespace nb = nanobind;

// This header provides nanobind-specific glue:
// - convert Python handles to cocotb::Handle/cocotb::Dut
// - create lightweight awaitable tokens consumed by cocotb_python_runner.h

inline std::string get_path_from_py_handle(const nb::object &obj) {
    if (nb::hasattr(obj, "_path")) {
        return nb::cast<std::string>(obj.attr("_path"));
    }
    if (nb::hasattr(obj, "path")) {
        return nb::cast<std::string>(obj.attr("path"));
    }
    throw std::runtime_error("Expected cocotb handle with a '_path' attribute.");
}

inline cocotb::Handle handle_from_py(const nb::object &obj) {
    const std::string path = get_path_from_py_handle(obj);
    gpi_sim_hdl raw = common::resolve_handle_from_path(path);
    if (!raw) {
        throw std::runtime_error("Failed to resolve simulator handle for path '" + path + "'.");
    }
    return cocotb::Handle(raw);
}

inline cocotb::Dut dut_from_py(const nb::object &obj) {
    const std::string path = get_path_from_py_handle(obj);
    gpi_sim_hdl raw = common::resolve_handle_from_path(path);
    if (!raw) {
        throw std::runtime_error("Failed to resolve DUT handle for path '" + path + "'.");
    }
    return cocotb::Dut(raw);
}

inline nb::object make_python_awaitable(const std::string &kind, const nb::object &payload) {
    static nb::object awaitable_cls;
    if (awaitable_cls.ptr() == nullptr) {
        nb::dict scope;
        nb::exec(R"(
class _CocotbCppAwaitable:
    __slots__ = ("_cocotb_cpp_kind", "_cocotb_cpp_payload")

    def __init__(self, kind, payload):
        self._cocotb_cpp_kind = kind
        self._cocotb_cpp_payload = payload

    def __await__(self):
        result = yield self
        return result
)",
                 scope);
        awaitable_cls = scope["_CocotbCppAwaitable"];
    }
    return awaitable_cls(kind, payload);
}

inline std::string exception_to_string(const std::exception_ptr &exc) {
    if (!exc) {
        return {};
    }
    try {
        std::rethrow_exception(exc);
    } catch (const std::exception &e) {
        return e.what();
    } catch (...) {
        return "Unknown C++ exception.";
    }
}

inline nb::object make_op_awaitable(const std::shared_ptr<PythonOpState> &state) {
    PyObject *capsule = make_python_op_capsule(state);
    if (!capsule) {
        throw std::runtime_error("Failed to create Python op capsule.");
    }
    nb::object payload = nb::steal<nb::object>(capsule);
    return make_python_awaitable("op", payload);
}

template <typename TaskFactory>
inline nb::object make_python_op_awaitable(PythonOpResultKind result_kind, TaskFactory &&task_factory, std::string_view op_name = {}) {
    // Extension authors can wrap any C++ coroutine op with this single call.
    return make_op_awaitable(make_python_op_state(result_kind, std::forward<TaskFactory>(task_factory), op_name));
}

} // namespace cocotb_cpp
