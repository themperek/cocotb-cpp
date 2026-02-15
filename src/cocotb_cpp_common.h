// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

#include <cocotb.h>

namespace cocotb_cpp::common {

inline gpi_sim_hdl get_root_handle() {
    gpi_sim_hdl top = gpi_get_root_handle(nullptr);
    if (!top) {
        if (const char *env_top = std::getenv("TOPLEVEL")) {
            top = gpi_get_root_handle(env_top);
        }
    }
    return top;
}

inline gpi_sim_hdl resolve_handle_from_path(const std::string &path) {
    gpi_sim_hdl top = get_root_handle();
    if (!top) {
        return nullptr;
    }

    if (path.empty()) {
        return top;
    }

    if (const char *top_name = gpi_get_signal_name_str(top)) {
        if (path == top_name) {
            return top;
        }
    }

    if (auto full = gpi_get_handle_by_name(top, path.c_str(), GPI_AUTO)) {
        return full;
    }

    if (auto dot = path.find('.'); dot != std::string::npos) {
        const std::string relative = path.substr(dot + 1);
        if (auto relative_hdl = gpi_get_handle_by_name(top, relative.c_str(), GPI_AUTO)) {
            return relative_hdl;
        }
    }

    return nullptr;
}

inline cocotb::unit unit_from_string(std::string_view unit_name) {
    if (unit_name == "fs") {
        return cocotb::unit::fs;
    }
    if (unit_name == "ps") {
        return cocotb::unit::ps;
    }
    if (unit_name == "ns") {
        return cocotb::unit::ns;
    }
    if (unit_name == "us") {
        return cocotb::unit::us;
    }
    if (unit_name == "ms") {
        return cocotb::unit::ms;
    }
    if (unit_name == "sec") {
        return cocotb::unit::sec;
    }
    if (unit_name == "step") {
        return cocotb::unit::step;
    }
    throw std::runtime_error("Unsupported time unit: " + std::string(unit_name));
}

inline std::string_view unit_to_string(cocotb::unit u) {
    switch (u) {
    case cocotb::unit::fs:
        return "fs";
    case cocotb::unit::ps:
        return "ps";
    case cocotb::unit::ns:
        return "ns";
    case cocotb::unit::us:
        return "us";
    case cocotb::unit::ms:
        return "ms";
    case cocotb::unit::sec:
        return "sec";
    case cocotb::unit::step:
        return "step";
    }
    return "step";
}

} // namespace cocotb_cpp::common

