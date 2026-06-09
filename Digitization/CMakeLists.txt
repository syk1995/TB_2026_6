cmake_minimum_required(VERSION 3.16)
project(TB2026Digitization LANGUAGES CXX)

add_library(cell_shaping INTERFACE)
target_include_directories(cell_shaping INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(cell_shaping INTERFACE cxx_std_17)

add_executable(test_cell_shaping examples/test_cell_shaping.cpp)
target_link_libraries(test_cell_shaping PRIVATE cell_shaping)

set(PYBIND11_FINDPYTHON ON)
find_package(Python3 COMPONENTS Interpreter Development.Module QUIET)
find_package(pybind11 CONFIG QUIET)

if(Python3_FOUND AND pybind11_FOUND)
  pybind11_add_module(cell_shaping_cpp python/cell_shaping_pybind.cpp)
  target_link_libraries(cell_shaping_cpp PRIVATE cell_shaping)
  set_target_properties(cell_shaping_cpp PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python
  )
endif()
