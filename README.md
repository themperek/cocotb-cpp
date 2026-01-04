# cocotb-cpp - experimental playground

This repository explores writing cocotb-style tests in modern C++, using coroutines and strong typing, with the goal of maintaining a Python-like test style while potentially improving simulation performance.

Thanks to recent changes to [cocotb](https://github.com/cocotb/cocotb/) (shoutout to [@ktbarrett](https://github.com/ktbarrett)), it is now possible to build directly against the GPI interface.

```cpp
#include <cocotb.h>

using namespace cocotb;

COCOTB_TEST(test_dff)
task<> test_dff(Dut &dut) {
    dut["d"].value = 0;
    auto clk = start_soon(Clock(dut, 10, unit::us));
    co_await RisingEdge(dut["clk"]);
    dut["d"].value = 1;
    co_await RisingEdge(dut["clk"]);
    assert(dut["q"].value == 0, "q should follow 0");
}
```

### How to use

You need a supported simulator (currently tested with Icarus Verilog), Python with cocotb master, and a C++20-capable compiler.
```bash
pip install https://github.com/cocotb/cocotb/archive/refs/heads/master.zip
```

Build the C++ test shared library (requires C++20):
```bash
g++ -shared -std=c++20 -fPIC -O3 -I./src -I$(cocotb-config --share)/include tests/test_dff.cpp -o lib_dff.so
```

Compile the Design:
```bash
iverilog -g2012 tests/dff.sv -o dff.vvp
```

Set `GPI_USERS` (loads the C++ shared library and registers the cocotb entry point) and run simulation:
```bash
GPI_USERS=./lib_dff.so,cocotb_entry_point vvp -M $(cocotb-config --lib-dir) -m $(cocotb-config --lib-name vpi icarus) dff.vvp
```

Expected Output:
```
     -.--ns INFO     gpi                                ../gpi/GpiCommon.cpp:231  in gpi_load_users                  Running entry func 'cocotb_entry_point' from loaded library './lib_dff.so'
     -.--ns INFO     gpi                                      ./src/cocotb.h:1017 in cocotb_entry_point              Entry point registered
     -.--ns INFO     gpi                                      ./src/cocotb.h:957  in on_sim_start                    Start of simulation
     0.00ns   INFO     cocotb.regression                  Running tests
     0.00ns   INFO     cocotb.regression                  running test_dff (1/2)
     0.00ns   INFO     cocotb                             Starting test_dff coroutine
115000.00ns   INFO     cocotb                             test_dff completed successfully
115000.00ns   INFO     cocotb.regression                  test_dff passed execution time: 0.000 s
115000.00ns   INFO     cocotb.regression                  running test_dff_post (2/2)
135940.00ns   INFO     cocotb.regression                  test_dff_post passed execution time: 0.000 s
135940.00ns   INFO     cocotb.regression                  ***************************************************************************************
135940.00ns   INFO     cocotb.regression                  ** TEST                          STATUS  REAL TIME (s)                               **
135940.00ns   INFO     cocotb.regression                  ***************************************************************************************
135940.00ns   INFO     cocotb.regression                  ** test_dff                       PASS           0.000                               **
135940.00ns   INFO     cocotb.regression                  ** test_dff_post                  PASS           0.000                               **
135940.00ns   INFO     cocotb.regression                  ***************************************************************************************
135940.00ns   INFO     cocotb.regression                  ** TESTS=2 PASS=2 FAIL=0                                                             **
135940.00ns   INFO     cocotb.regression                  ***************************************************************************************
```

You can find more examples and tests in the `tests` folder.

### Run all tests/examples

Pytest uses the cocotb runner under the hood:

```bash
pytest
```

### Status

This is an experimental playground. APIs, build steps, and cocotb compatibility may change without notice.