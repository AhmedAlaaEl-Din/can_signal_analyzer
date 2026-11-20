cmake_minimum_required(VERSION 3.21)

# The version is defined here only; version.h, the Windows version resource,
# the About dialog and the package name are generated from it.
project(CanAnalyzer
    VERSION 1.0.0
    DESCRIPTION "Qt 6 / QML CAN signal analyzer (simulated bus)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Svg Test)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

# All QML modules land in one tree whose layout matches their URIs (tooling friendly).
# Not "qml": windeployqt (deploy target) copies Qt's own QML modules into <build>/qml.
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qmlmodules)

# Shared warning settings for every target in the project.
add_library(can_warnings INTERFACE)
if(MSVC)
    # C4702 (unreachable code) fires inside Qt headers during optimisation; it cannot
    # be scoped with /external, so it is disabled project-wide.
    target_compile_options(can_warnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus /wd4702)
else()
    target_compile_options(can_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
endif()

# Qt's bin directory (derived from the imported target, nothing machine
# specific is hard-coded): build-time tools and tests run with it on PATH.
get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core LOCATION)
endif()
get_filename_component(CAN_QT_BIN_DIR "${_qt_core_location}" DIRECTORY)

enable_testing()

add_subdirectory(src/core)
add_subdirectory(tools)
add_subdirectory(src/app)
add_subdirectory(tests)
add_subdirectory(bench)
