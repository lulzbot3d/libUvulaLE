# CMakeList.txt : CMake project for libuvula, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set (PYTHON_EXECUTABLE "C:/dvl/Cura/build/generators/cura_venv/Scripts/python.exe")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("libuvula")

# Find Pybind11
find_package(pybind11 REQUIRED)

# Include Conan-generated files
#include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
#conan_basic_setup()

## Add your Python module
pybind11_add_module(libuvula libuvula.cpp)

add_library(UNWRAP ./src/unwrap.cpp)
target_link_libraries(UNWRAP PRIVATE pybind11::pybind11)
target_include_directories(UNWRAP PUBLIC ./include)

target_link_libraries(libuvula PRIVATE UNWRAP)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET libuvula PROPERTY CXX_STANDARD 20)
endif()
