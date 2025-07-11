set(ENV{LD_LIBRARY_PATH} "${CMAKE_LIBRARY_PATH}:${LD_LIBRARY_PATH}")  # Needed to ensure that CMake finds the Conan CPython library
find_package(Python COMPONENTS Interpreter Development)
find_package(pybind11 REQUIRED)

pybind11_add_module(pyUvula pyUvula.cpp)
target_link_libraries(pyUvula PUBLIC libuvula pybind11::pybind11 stdc++)
target_compile_definitions(pyUvula PRIVATE PYUVULA_VERSION="${PYUVULA_VERSION}")
if (NOT MSVC AND NOT ${CMAKE_BUILD_TYPE} MATCHES Debug|RelWithDebInfo)
    pybind11_strip(pyUvula)
endif ()
