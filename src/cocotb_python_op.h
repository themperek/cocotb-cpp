// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <Python.h>

#include <cstdlib>
#include <cstdint>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <cocotb.h>

namespace cocotb_cpp {

enum class PythonOpResultKind : uint8_t {
    None = 0,
    U32 = 1,
};

// Shared op state transported from nanobind extension to the C++ Python runner.
// Extension-side methods create a C++ coroutine handle, and runner-side code
// directly joins that coroutine from the Python await loop.
struct PythonOpState {
    cocotb::task<>::handle_type handle{};
    PythonOpResultKind result_kind{PythonOpResultKind::None};
    uint32_t value_u32{0};
    std::string error;
};

inline constexpr const char *kPythonOpCapsuleName = "cocotb_cpp.PythonOpState";

inline bool debug_enabled() {
    static const bool enabled = [] {
        const char *v = std::getenv("COCOTB_CPP_DEBUG");
        return v && *v;
    }();
    return enabled;
}

inline void python_op_capsule_destructor(PyObject *capsule) {
    void *raw = PyCapsule_GetPointer(capsule, kPythonOpCapsuleName);
    if (!raw) {
        PyErr_Clear();
        return;
    }
    auto *holder = static_cast<std::shared_ptr<PythonOpState> *>(raw);
    delete holder;
}

inline PyObject *make_python_op_capsule(const std::shared_ptr<PythonOpState> &state) {
    auto *holder = new std::shared_ptr<PythonOpState>(state);
    PyObject *capsule = PyCapsule_New(static_cast<void *>(holder), kPythonOpCapsuleName, &python_op_capsule_destructor);
    if (!capsule) {
        delete holder;
    }
    return capsule;
}

// Generic helper used by extension bindings to create and validate op state.
// The task_factory receives the created PythonOpState and must return cocotb::task<>.
template <typename TaskFactory>
inline std::shared_ptr<PythonOpState> make_python_op_state(PythonOpResultKind result_kind, TaskFactory &&task_factory, std::string_view op_name = {}) {
    auto state = std::make_shared<PythonOpState>();
    state->result_kind = result_kind;
    state->handle = std::forward<TaskFactory>(task_factory)(state).release();
    if (!state->handle) {
        if (op_name.empty()) {
            throw std::runtime_error("Failed to create op coroutine handle.");
        }
        throw std::runtime_error(std::format("Failed to create {} coroutine handle.", op_name));
    }
    if (debug_enabled() && !op_name.empty()) {
        cocotb::log.info(std::format("{} handle={}", op_name, reinterpret_cast<uintptr_t>(state->handle.address())));
    }
    return state;
}

// Execute extension coroutine body with consistent debug/error handling.
template <typename TaskFactory>
inline cocotb::task<> run_python_op(std::shared_ptr<PythonOpState> state, std::string_view op_name, TaskFactory task_factory) {
    if (debug_enabled() && !op_name.empty()) {
        cocotb::log.info(std::format("{} start", op_name));
    }
    try {
        // task_factory must be owned by this coroutine frame (by value),
        // otherwise a forwarded reference may dangle after initial suspend.
        co_await task_factory();
    } catch (const std::exception &e) {
        state->error = e.what();
    } catch (...) {
        state->error = "Unknown C++ exception.";
    }
    if (debug_enabled() && !op_name.empty()) {
        cocotb::log.info(std::format("{} done", op_name));
    }
}

} // namespace cocotb_cpp
