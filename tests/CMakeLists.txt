cmake_minimum_required(VERSION 3.20)
project(cocotb_cpp_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (NOT DEFINED COCOTB_CPP_SOURCE_DIR)
    get_filename_component(COCOTB_CPP_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

find_package(Python REQUIRED COMPONENTS Interpreter)

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

function(add_cocotb_cpp_test_lib target source output_name)
    add_library(${target} SHARED "${source}" "${COCOTB_CPP_SOURCE_DIR}/src/cocotb_core.cpp")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    target_include_directories(${target}
        PRIVATE
            "${COCOTB_CPP_SOURCE_DIR}"
            "${COCOTB_CPP_SOURCE_DIR}/src"
            "${COCOTB_CPP_SOURCE_DIR}/examples/axil_ext/src"
            "${COCOTB_INCLUDE_DIR}"
    )
    set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "${output_name}")
endfunction()

add_cocotb_cpp_test_lib(cocotb_test_dff "${CMAKE_CURRENT_SOURCE_DIR}/test_dff.cpp" "lib_dff")
add_cocotb_cpp_test_lib(cocotb_test_axil "${CMAKE_CURRENT_SOURCE_DIR}/test_axil.cpp" "lib_axil")
