cmake_minimum_required(VERSION 3.20)
project(axil_ext LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT DEFINED COCOTB_CPP_SOURCE_DIR)
    get_filename_component(COCOTB_CPP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

find_package(Python REQUIRED COMPONENTS Interpreter Development Development.Module)

execute_process(
    COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
    OUTPUT_VARIABLE nanobind_DIR
    RESULT_VARIABLE nanobind_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT nanobind_status EQUAL 0)
    message(FATAL_ERROR "Could not locate nanobind. Install the Python package 'nanobind'.")
endif()
find_package(nanobind CONFIG REQUIRED PATHS "${nanobind_DIR}")

set(_cocotb_include_py "import pathlib
try:
    import cocotb_tools.config as cfg
    include_dir = pathlib.Path(cfg.share_dir) / 'include'
except Exception:
    import cocotb
    include_dir = pathlib.Path(cocotb.__file__).resolve().parent / 'share' / 'include'
print(include_dir)")
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "${_cocotb_include_py}"
    OUTPUT_VARIABLE COCOTB_INCLUDE_DIR
    RESULT_VARIABLE cocotb_include_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT cocotb_include_status EQUAL 0)
    message(FATAL_ERROR "Could not discover cocotb include path from Python cocotb installation.")
endif()

set(_cocotb_cpp_core_py "import pathlib, sysconfig
paths = []
for key in ('platlib', 'purelib'):
    p = sysconfig.get_path(key)
    if p:
        paths.append(pathlib.Path(p))
candidates = []
for base in paths:
    pkg = base / 'cocotb_cpp'
    if pkg.exists():
        candidates.extend([p for p in pkg.glob('cocotb_cpp_core*') if p.suffix in ('.so', '.dylib', '.pyd')])
candidates = sorted(set(candidates))
if not candidates:
    raise SystemExit(1)
print(candidates[0])")
execute_process(
    COMMAND "${Python_EXECUTABLE}" -c "${_cocotb_cpp_core_py}"
    OUTPUT_VARIABLE COCOTB_CPP_CORE_LIB
    RESULT_VARIABLE cocotb_cpp_core_status
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
if (NOT cocotb_cpp_core_status EQUAL 0)
    message(FATAL_ERROR "Could not discover cocotb_cpp_core shared library. Install cocotb-cpp first (pip install . or pip install -e .).")
endif()

nanobind_add_module(axil src/axil_bindings.cpp)
target_compile_features(axil PRIVATE cxx_std_20)
target_include_directories(axil
    PRIVATE
        "${COCOTB_CPP_SOURCE_DIR}"
        "${COCOTB_CPP_SOURCE_DIR}/src"
        "${COCOTB_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(axil PRIVATE Python::Python "${COCOTB_CPP_CORE_LIB}")
get_filename_component(_cocotb_cpp_core_dir "${COCOTB_CPP_CORE_LIB}" DIRECTORY)
if (DEFINED Python_LIBRARY_RELEASE)
    get_filename_component(_python_libdir "${Python_LIBRARY_RELEASE}" DIRECTORY)
    set_target_properties(axil PROPERTIES BUILD_RPATH "${_python_libdir};${_cocotb_cpp_core_dir}" INSTALL_RPATH "$ORIGIN;$ORIGIN/../cocotb_cpp;${_python_libdir}")
else()
    set_target_properties(axil PROPERTIES BUILD_RPATH "${_cocotb_cpp_core_dir}" INSTALL_RPATH "$ORIGIN;$ORIGIN/../cocotb_cpp")
endif()

install(TARGETS axil LIBRARY DESTINATION axil_ext)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/axil_ext/__init__.py" DESTINATION axil_ext)
