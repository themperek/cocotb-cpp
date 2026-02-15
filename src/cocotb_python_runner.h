// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Python.h>

#include <cstdlib>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include <cocotb_cpp_common.h>
#include <cocotb_python_op.h>
#include <cocotb.h>

namespace cocotb_cpp {

namespace detail {

// This file runs a Python async test from C++ (selected by COCOTB_MODULE/COCOTB_TEST)
// and translates yielded cocotb_cpp awaitable tokens into native C++ cocotb triggers.

class PyRef {
  public:
    PyRef() = default;
    explicit PyRef(PyObject *obj) : obj_(obj) {}
    ~PyRef() { reset(); }

    PyRef(const PyRef &) = delete;
    PyRef &operator=(const PyRef &) = delete;

    PyRef(PyRef &&other) noexcept : obj_(other.obj_) { other.obj_ = nullptr; }
    PyRef &operator=(PyRef &&other) noexcept {
        if (this != &other) {
            reset();
            obj_ = other.obj_;
            other.obj_ = nullptr;
        }
        return *this;
    }

    PyObject *get() const { return obj_; }
    explicit operator bool() const { return obj_ != nullptr; }

    PyObject *release() {
        PyObject *tmp = obj_;
        obj_ = nullptr;
        return tmp;
    }

    void reset(PyObject *obj = nullptr) {
        if (obj_) {
            if (Py_IsInitialized()) {
                PyGILState_STATE state = PyGILState_Ensure();
                Py_DECREF(obj_);
                PyGILState_Release(state);
            }
        }
        obj_ = obj;
    }

  private:
    PyObject *obj_{nullptr};
};

inline std::string fetch_python_error() {
    if (!PyErr_Occurred()) {
        return "Unknown Python error.";
    }

    PyObject *ptype = nullptr;
    PyObject *pvalue = nullptr;
    PyObject *ptraceback = nullptr;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    PyErr_NormalizeException(&ptype, &pvalue, &ptraceback);

    std::string msg = "Python exception";
    if (pvalue) {
        PyObject *s = PyObject_Str(pvalue);
        if (s) {
            const char *txt = PyUnicode_AsUTF8(s);
            if (txt) {
                msg = txt;
            }
            Py_DECREF(s);
        }
    }

    Py_XDECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
    return msg;
}

inline std::string repr_sys_path() {
    PyObject *sys_path = PySys_GetObject("path"); // borrowed
    if (!sys_path) {
        return "<sys.path unavailable>";
    }
    PyRef repr(PyObject_Repr(sys_path));
    if (!repr) {
        return "<sys.path repr failed>";
    }
    const char *txt = PyUnicode_AsUTF8(repr.get());
    if (!txt) {
        return "<sys.path utf8 conversion failed>";
    }
    return txt;
}

inline void append_env_pythonpath() {
    const char *pythonpath = std::getenv("PYTHONPATH");
    if (!pythonpath || !*pythonpath) {
        return;
    }

    PyObject *sys_path = PySys_GetObject("path"); // borrowed reference
    if (!sys_path || !PyList_Check(sys_path)) {
        throw std::runtime_error("Python sys.path is not available.");
    }

    std::string_view all_paths(pythonpath);
    size_t pos = 0;
    while (pos <= all_paths.size()) {
        size_t next = all_paths.find(':', pos);
        if (next == std::string_view::npos) {
            next = all_paths.size();
        }
        std::string_view entry = all_paths.substr(pos, next - pos);
        if (!entry.empty()) {
            PyRef py_entry(PyUnicode_FromStringAndSize(entry.data(), static_cast<Py_ssize_t>(entry.size())));
            if (!py_entry) {
                throw std::runtime_error(fetch_python_error());
            }
            if (PyList_Append(sys_path, py_entry.get()) != 0) {
                throw std::runtime_error(fetch_python_error());
            }
        }
        if (next == all_paths.size()) {
            break;
        }
        pos = next + 1;
    }
}

inline void append_env_single_path(const char *env_name) {
    const char *path = std::getenv(env_name);
    if (!path || !*path) {
        return;
    }
    PyObject *sys_path = PySys_GetObject("path"); // borrowed reference
    if (!sys_path || !PyList_Check(sys_path)) {
        throw std::runtime_error("Python sys.path is not available.");
    }
    PyRef py_entry(PyUnicode_FromString(path));
    if (!py_entry) {
        throw std::runtime_error(fetch_python_error());
    }
    if (PyList_Append(sys_path, py_entry.get()) != 0) {
        throw std::runtime_error(fetch_python_error());
    }
}

struct AwaitSpec {
    enum class Kind { Timer, RisingEdge, Op } kind;
    uint64_t delay{0};
    cocotb::unit unit{cocotb::unit::step};
    std::string path;
    std::shared_ptr<cocotb_cpp::PythonOpState> op_state;
};

inline AwaitSpec parse_awaitable(PyObject *yielded) {
    AwaitSpec spec;

    PyRef kind_obj(PyObject_GetAttrString(yielded, "_cocotb_cpp_kind"));
    if (!kind_obj) {
        throw std::runtime_error("Yielded object is not a cocotb_cpp awaitable (missing _cocotb_cpp_kind).");
    }
    const char *kind_cstr = PyUnicode_AsUTF8(kind_obj.get());
    if (!kind_cstr) {
        throw std::runtime_error(fetch_python_error());
    }
    const std::string kind(kind_cstr);

    PyRef payload(PyObject_GetAttrString(yielded, "_cocotb_cpp_payload"));
    if (!payload) {
        throw std::runtime_error("Yielded object is not a cocotb_cpp awaitable (missing _cocotb_cpp_payload).");
    }

    if (kind == "timer") {
        if (!PyTuple_Check(payload.get()) || PyTuple_Size(payload.get()) != 2) {
            throw std::runtime_error("Timer awaitable payload must be (delay, unit).");
        }
        PyObject *delay_obj = PyTuple_GetItem(payload.get(), 0);
        PyObject *unit_obj = PyTuple_GetItem(payload.get(), 1);
        if (!delay_obj || !unit_obj) {
            throw std::runtime_error("Malformed timer awaitable payload.");
        }
        long long delay = PyLong_AsLongLong(delay_obj);
        if (delay < 0 || (delay == -1 && PyErr_Occurred())) {
            throw std::runtime_error(fetch_python_error());
        }
        const char *unit_cstr = PyUnicode_AsUTF8(unit_obj);
        if (!unit_cstr) {
            throw std::runtime_error(fetch_python_error());
        }
        spec.kind = AwaitSpec::Kind::Timer;
        spec.delay = static_cast<uint64_t>(delay);
        spec.unit = common::unit_from_string(unit_cstr);
        return spec;
    }

    if (kind == "rising_edge") {
        const char *path_cstr = PyUnicode_AsUTF8(payload.get());
        if (!path_cstr) {
            throw std::runtime_error(fetch_python_error());
        }
        spec.kind = AwaitSpec::Kind::RisingEdge;
        spec.path = path_cstr;
        return spec;
    }

    if (kind == "op") {
        if (!PyCapsule_CheckExact(payload.get())) {
            throw std::runtime_error("Op awaitable payload must be a capsule.");
        }
        void *raw = PyCapsule_GetPointer(payload.get(), cocotb_cpp::kPythonOpCapsuleName);
        if (!raw) {
            throw std::runtime_error(fetch_python_error());
        }
        auto *holder = static_cast<std::shared_ptr<cocotb_cpp::PythonOpState> *>(raw);
        spec.kind = AwaitSpec::Kind::Op;
        spec.op_state = *holder;
        if (!spec.op_state) {
            throw std::runtime_error("Op capsule contains null state.");
        }
        return spec;
    }

    throw std::runtime_error("Unsupported cocotb_cpp awaitable kind: " + kind);
}

} // namespace detail

inline cocotb::task<> run_python_test(cocotb::Dut &dut) {
    const char *module_env = std::getenv("COCOTB_MODULE");
    const char *test_env = std::getenv("COCOTB_TEST");
    if (!module_env || !*module_env) {
        throw std::runtime_error("COCOTB_MODULE is not set.");
    }
    if (!test_env || !*test_env) {
        throw std::runtime_error("COCOTB_TEST is not set.");
    }
    const std::string module_name(module_env);
    const std::string test_name(test_env);
    const bool debug = cocotb_cpp::debug_enabled();

    if (!Py_IsInitialized()) {
        Py_Initialize();
    }

    detail::PyRef coro;
    {
        PyGILState_STATE gil = PyGILState_Ensure();
        detail::append_env_pythonpath();
        detail::append_env_single_path("COCOTB_CPP_ROOT");
        detail::append_env_single_path("COCOTB_CPP_TESTS");

        detail::PyRef native_mod(PyImport_ImportModule("cocotb_cpp._native"));
        if (!native_mod) {
            std::string err = detail::fetch_python_error();
            const char *py_path_env = std::getenv("PYTHONPATH");
            std::string path_dbg = detail::repr_sys_path();
            PyGILState_Release(gil);
            throw std::runtime_error("Failed to import cocotb_cpp._native: " + err + " | PYTHONPATH=" + (py_path_env ? py_path_env : "<unset>") + " | sys.path=" + path_dbg);
        }

        detail::PyRef handle_cls(PyObject_GetAttrString(native_mod.get(), "Handle"));
        if (!handle_cls) {
            std::string err = detail::fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error("cocotb_cpp._native.Handle is missing: " + err);
        }

        const char *top_name = gpi_get_signal_name_str(dut.raw());
        detail::PyRef top_name_obj(PyUnicode_FromString(top_name ? top_name : ""));
        detail::PyRef dut_obj(PyObject_CallFunctionObjArgs(handle_cls.get(), top_name_obj.get(), nullptr));
        if (!dut_obj) {
            std::string err = detail::fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error("Failed to construct Python DUT handle: " + err);
        }

        detail::PyRef module(PyImport_ImportModule(module_name.c_str()));
        if (!module) {
            std::string err = detail::fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error("Failed to import Python module '" + module_name + "': " + err);
        }

        detail::PyRef fn(PyObject_GetAttrString(module.get(), test_name.c_str()));
        if (!fn || !PyCallable_Check(fn.get())) {
            std::string err = detail::fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error("Python test callable '" + test_name + "' not found: " + err);
        }

        coro.reset(PyObject_CallFunctionObjArgs(fn.get(), dut_obj.get(), nullptr));
        if (!coro) {
            std::string err = detail::fetch_python_error();
            PyGILState_Release(gil);
            throw std::runtime_error("Failed to create coroutine for '" + module_name + "." + test_name + "': " + err);
        }

        PyGILState_Release(gil);
    }

    if (debug) {
        cocotb::log.info(std::format("cocotb_cpp: python coroutine created for {}.{}", module_name, test_name));
    }

    detail::PyRef send_value;
    {
        PyGILState_STATE gil = PyGILState_Ensure();
        Py_INCREF(Py_None);
        send_value.reset(Py_None);
        PyGILState_Release(gil);
    }

    while (true) {
        if (debug) {
            cocotb::log.info("cocotb_cpp: send() into python coroutine");
        }
        detail::AwaitSpec spec;
        {
            PyGILState_STATE gil = PyGILState_Ensure();
            detail::PyRef yielded;
            yielded.reset(PyObject_CallMethod(coro.get(), "send", "O", send_value.get()));
            send_value.reset();
            if (!yielded) {
                if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
                    PyErr_Clear();
                    PyGILState_Release(gil);
                    break;
                }
                std::string err = detail::fetch_python_error();
                PyGILState_Release(gil);
                throw std::runtime_error("Python coroutine raised: " + err);
            }
            try {
                spec = detail::parse_awaitable(yielded.get());
            } catch (...) {
                PyGILState_Release(gil);
                throw;
            }
            PyGILState_Release(gil);
        }

        if (debug) {
            const char *k = spec.kind == detail::AwaitSpec::Kind::Timer      ? "timer"
                            : spec.kind == detail::AwaitSpec::Kind::RisingEdge ? "rising_edge"
                                                                               : "op";
            cocotb::log.info(std::format("cocotb_cpp: yielded {}", k));
        }

        if (spec.kind == detail::AwaitSpec::Kind::Timer) {
            co_await cocotb::Timer(spec.delay, spec.unit);
            PyGILState_STATE gil = PyGILState_Ensure();
            Py_INCREF(Py_None);
            send_value.reset(Py_None);
            PyGILState_Release(gil);
            continue;
        }

        if (spec.kind == detail::AwaitSpec::Kind::RisingEdge) {
            cocotb::Handle h(common::resolve_handle_from_path(spec.path));
            if (!h.valid()) {
                throw std::runtime_error("Failed to resolve signal path for RisingEdge: " + spec.path);
            }
            co_await cocotb::RisingEdge(h);
            PyGILState_STATE gil = PyGILState_Ensure();
            Py_INCREF(Py_None);
            send_value.reset(Py_None);
            PyGILState_Release(gil);
            continue;
        }

        if (!spec.op_state->handle) {
            throw std::runtime_error("Op state has empty coroutine handle.");
        }
        co_await cocotb::task<>::join_awaiter{spec.op_state->handle};
        spec.op_state->handle = {};
        if (debug) {
            cocotb::log.info("cocotb_cpp: op completed");
        }

        {
            PyGILState_STATE gil = PyGILState_Ensure();
            if (!spec.op_state->error.empty()) {
                PyGILState_Release(gil);
                throw std::runtime_error(spec.op_state->error);
            }

            if (spec.op_state->result_kind == cocotb_cpp::PythonOpResultKind::None) {
                Py_INCREF(Py_None);
                send_value.reset(Py_None);
            } else if (spec.op_state->result_kind == cocotb_cpp::PythonOpResultKind::U32) {
                send_value.reset(PyLong_FromUnsignedLong(spec.op_state->value_u32));
            } else {
                PyGILState_Release(gil);
                throw std::runtime_error("Unsupported op result kind.");
            }

            if (!send_value) {
                std::string err = detail::fetch_python_error();
                PyGILState_Release(gil);
                throw std::runtime_error("Failed to build Python op result value: " + err);
            }
            PyGILState_Release(gil);
        }
    }
}

} // namespace cocotb_cpp
