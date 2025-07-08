cmake_minimum_required(VERSION 3.23)
project(uvula)

find_package(standardprojectsettings REQUIRED)
option(EXTENSIVE_WARNINGS "Build with all warnings" ON)

option(WITH_PYTHON_BINDINGS "Build with Python bindings: `pyUvula`" ON)
if(WITH_PYTHON_BINDINGS)
    set(PYUVULA_VERSION "1.0.0" CACHE STRING "Version of the pyuvula python bindings" )
    message(STATUS "Configuring pyUvula version: ${PYUVULA_VERSION}")
endif ()

if(NOT DEFINED UVULA_VERSION)
    message(FATAL_ERROR "UVULA_VERSION is not defined!")
endif()
message(STATUS "Configuring Uvula version: ${UVULA_VERSION}")

set(UVULA_SRC
        src/unwrap.cpp
        src/xatlas.cpp
)
add_library(uvula STATIC ${UVULA_SRC})

target_link_libraries(uvula
        PUBLIC)

target_include_directories(uvula
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(uvula
    PUBLIC
    UVULA_VERSION="${UVULA_VERSION}"
)

use_threads(uvula)
enable_sanitizers(uvula)
if (${EXTENSIVE_WARNINGS})
    set_project_warnings(uvula)
endif ()

# --- Setup Python bindings ---
if (WITH_PYTHON_BINDINGS)
    add_subdirectory(pyUvula)
endif ()
