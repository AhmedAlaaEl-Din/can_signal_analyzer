cmake_minimum_required(VERSION 3.21)

project(CanAnalyzer
    VERSION 0.1.0
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

enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/app)
add_subdirectory(tests)
add_subdirectory(bench)
