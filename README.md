# cocotb-cpp - experimental playground

This repository explores writing cocotb-style tests in modern C++, using coroutines and strong typing, with the goal of maintaining a Python-like test style while potentially improving simulation performance.
Project also iclude demonstration of Python nanobind wrapper enables seamless top-level test control from Python while using high-performance C++ underneath.

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
    assert(dut["q"].value == 0, "q should be 0");
}
```

### How to use

You need a supported simulator (currently tested with Icarus Verilog), Python with cocotb master, and a C++20-capable compiler.
```bash
pip install https://github.com/cocotb/cocotb/archive/refs/heads/master.zip
```

Build the C++ test shared library (requires C++20):
```bash
g++ -shared -std=c++20 -fPIC -O3 \
  -I./src -I$(cocotb-config --share)/include \
  tests/test_dff.cpp src/cocotb_core.cpp \
  -o lib_dff.so
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

### Python package build (`scikit-build-core` + `CMake` + `nanobind`)

Install in editable mode:

```bash
pip install -e .
```

Install the AXI extension project:

```bash
pip install -e examples/axil_ext
```

Then run the Python + C++ AXI-Lite path:

```bash
pytest -q examples/axil_ext/tests/test_axil_ext.py
```

### Example extension

The AXI-Lite extension is intentionally separated as an example of a user-defined cocotb-cpp extension:

- `examples/axil_ext`

This is the reference pattern for adding custom C++ drivers/monitors and exposing them to Python.

### Run tests in isolated envs (`nox`)

Run the core cocotb-cpp tests (`tests/`):

```bash
nox -s tests
```

Run the AXI extension tests (`examples/axil_ext/tests/`):

```bash
nox -s axil_ext_tests
```


### Performance: AXI-Lite Example

The following table compares simulation runtime for the reference AXI-Lite memory test (`tests/axil.sv`, random access, 1,000,000 writre/read transactions) with different cocotb/cocotb-cpp configurations. All results measured with Icarus Verilog 12.0 on the same machine.

| Approach                | Execution Time | Speedup vs cocotb |
|-------------------------|---------------|-------------------|
| cocotb                  | 53.67s        | 1.00x             |
| cocotb-cpp              | 12.63s        | 4.25x             |
| cocotb-cpp + Python mix | 17.52s        | 3.01x             |

- **cocotb:** Standard Python-based cocotb testbench/driver (`tests/test_axil.py`). 
- **cocotb-cpp:** Testbench, driver, and transaction logic implemented fully in C++ with cocotb-cpp API (`tests/test_axil.cpp`).
- **cocotb-cpp + Python:** Mixed testbench, e.g., C++ driver invoked from Python test via cocotb-cpp bridge (`examples/axil_ext/tests/test_axil_ext.py`).

### Status

This is an experimental playground. APIs, build steps, and cocotb compatibility may change without notice.

Parts of code generated by LLMs.