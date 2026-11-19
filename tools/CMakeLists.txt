# Helper executables.

# can_logtool: generate the sample log, inspect logs, large-log benchmark.
qt_add_executable(can_logtool logtool/main.cpp)
set_target_properties(can_logtool PROPERTIES WIN32_EXECUTABLE OFF)
target_compile_definitions(can_logtool PRIVATE CAN_ANALYZER_VERSION="${PROJECT_VERSION}")
target_link_libraries(can_logtool PRIVATE can_core can_warnings)
if(WIN32)
    target_link_libraries(can_logtool PRIVATE psapi)
endif()

# can_icongen: renders src/app/icons/app.svg into the Windows .ico at build time.
qt_add_executable(can_icongen icongen/main.cpp)
set_target_properties(can_icongen PROPERTIES WIN32_EXECUTABLE OFF)
target_link_libraries(can_icongen PRIVATE Qt6::Gui Qt6::Svg can_warnings)

set(CAN_APP_ICO "${CMAKE_BINARY_DIR}/generated/can_analyzer.ico" CACHE INTERNAL "")
add_custom_command(
    OUTPUT "${CAN_APP_ICO}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/generated"
    COMMAND "${CMAKE_COMMAND}" -E env --modify "PATH=path_list_prepend:${CAN_QT_BIN_DIR}" --
            "$<TARGET_FILE:can_icongen>" "${CMAKE_SOURCE_DIR}/src/app/icons/app.svg" "${CAN_APP_ICO}"
    DEPENDS can_icongen "${CMAKE_SOURCE_DIR}/src/app/icons/app.svg"
    COMMENT "Rendering the application icon"
    VERBATIM)
add_custom_target(app_icon DEPENDS "${CAN_APP_ICO}")
