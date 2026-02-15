// Copyright cocotb contributors
// Licensed under the Revised BSD License, see LICENSE for details.
// SPDX-License-Identifier: BSD-3-Clause

#include <cocotb.h>

namespace cocotb {

Scheduler &Scheduler::instance() {
    static Scheduler sched;
    return sched;
}

} // namespace cocotb
