cmake_minimum_required(VERSION 3.20)
project(cocotb_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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

function(cocotb_cpp_target_defaults target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    target_include_directories(${target_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${COCOTB_INCLUDE_DIR}
    )
    target_link_libraries(${target_name} PRIVATE Python::Python)
    set(_rpath_origin "$ORIGIN")
    if (DEFINED Python_LIBRARY_RELEASE)
        get_filename_component(_python_libdir "${Python_LIBRARY_RELEASE}" DIRECTORY)
        set_target_properties(${target_name} PROPERTIES BUILD_RPATH "${_python_libdir}" INSTALL_RPATH "${_rpath_origin};${_python_libdir}")
    else()
        set_target_properties(${target_name} PROPERTIES INSTALL_RPATH "${_rpath_origin}")
    endif()
endfunction()

add_library(cocotb_cpp_core SHARED src/cocotb_core.cpp)
set_target_properties(cocotb_cpp_core PROPERTIES PREFIX "" OUTPUT_NAME "cocotb_cpp_core")
cocotb_cpp_target_defaults(cocotb_cpp_core)
install(TARGETS cocotb_cpp_core LIBRARY DESTINATION cocotb_cpp)

nanobind_add_module(_native src/cocotb_cpp_bindings.cpp)
cocotb_cpp_target_defaults(_native)
target_link_libraries(_native PRIVATE cocotb_cpp_core)
install(TARGETS _native LIBRARY DESTINATION cocotb_cpp)

add_library(cocotb_cpp_entry SHARED src/cocotb_python_entry.cpp)
set_target_properties(cocotb_cpp_entry PROPERTIES PREFIX "" OUTPUT_NAME "cocotb_cpp_entry")
cocotb_cpp_target_defaults(cocotb_cpp_entry)
target_link_libraries(cocotb_cpp_entry PRIVATE cocotb_cpp_core)
install(TARGETS cocotb_cpp_entry LIBRARY DESTINATION cocotb_cpp)

install(FILES cocotb_cpp/__init__.py cocotb_cpp/triggers.py DESTINATION cocotb_cpp)
