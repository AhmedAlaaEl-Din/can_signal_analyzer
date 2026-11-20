# QML application: design system (CanUi), custom Controls style (CanStyle)
# and the screens (CanAnalyzer module).
add_subdirectory(ui)
add_subdirectory(style)

# Version header and Windows resources (icon + version info) from the single
# version in the top-level project() call.
if(CMAKE_BUILD_TYPE)
    set(CAN_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
else()
    set(CAN_BUILD_TYPE "Release")
endif()
set(CAN_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(MSVC)
    set(CAN_COMPILER "MSVC ${CMAKE_CXX_COMPILER_VERSION}")
endif()
configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)
set(CAN_APP_RESOURCES)
if(WIN32)
    configure_file(app.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app.rc" @ONLY)
    set(CAN_APP_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/app.rc" PROPERTIES OBJECT_DEPENDS "${CAN_APP_ICO}")
endif()

qt_add_executable(can_analyzer WIN32 MACOSX_BUNDLE
    main.cpp
    demodirector.h demodirector.cpp
    plotbench.h plotbench.cpp
    appservices.h appservices.cpp
    ${CAN_APP_RESOURCES}
)
if(WIN32)
    add_dependencies(can_analyzer app_icon)
endif()
target_include_directories(can_analyzer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

qt_add_qml_module(can_analyzer
    URI CanAnalyzer
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/AppToolBar.qml
        qml/StatusBar.qml
        qml/TableHeader.qml
        qml/TracePage.qml
        qml/TraceRow.qml
        qml/MessagesPage.qml
        qml/FloatingPill.qml
        qml/SignalsPage.qml
        qml/SignalDetail.qml
        qml/BitLayout.qml
        qml/DecodedGrid.qml
        qml/DatabaseBar.qml
        qml/DatabaseErrorBanner.qml
        qml/PlotsPage.qml
        qml/PlotLane.qml
        qml/ReplayBar.qml
        qml/SeekBar.qml
        qml/RecordButton.qml
        qml/LogSummaryDialog.qml
        qml/AboutDialog.qml
        qml/Toast.qml
        qml/WelcomeState.qml
        qml/format.js
    RESOURCES
        icons/app.svg
    SOURCES
        sparkline.h sparkline.cpp
        plotcanvas.h plotcanvas.cpp
)


target_link_libraries(can_analyzer PRIVATE
    can_core
    canuiplugin
    canstyleplugin
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    can_warnings
)

# License files next to the executable, so the About dialog links work in the
# build tree as they do in the release package.
add_custom_command(TARGET can_analyzer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:can_analyzer>/LICENSE.txt"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/packaging/THIRD_PARTY" "$<TARGET_FILE_DIR:can_analyzer>/THIRD_PARTY"
    VERBATIM)

# Optional: copy the Qt runtime next to the executable (windeployqt) so the app
# can be started from Explorer. Usage: cmake --build --preset <preset> --target deploy
if(WIN32)
    get_target_property(_qmake_location Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_location}" DIRECTORY)
    find_program(CAN_WINDEPLOYQT windeployqt HINTS "${_qt_bin_dir}" NO_DEFAULT_PATH)
    if(CAN_WINDEPLOYQT)
        add_custom_target(deploy
            COMMAND "${CAN_WINDEPLOYQT}" --qmldir "${CMAKE_CURRENT_SOURCE_DIR}" --no-translations
                    --no-system-d3d-compiler --no-opengl-sw "$<TARGET_FILE:can_analyzer>"
            DEPENDS can_analyzer
            COMMENT "Deploying the Qt runtime next to can_analyzer.exe"
            VERBATIM)

        # Portable release package: dist/CANSignalAnalyzer-<version>-win64.zip
        # Usage: cmake --build --preset <preset> --target package_zip
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
        set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
        include(InstallRequiredSystemLibraries)
        add_custom_target(package_zip
            COMMAND "${CMAKE_COMMAND}"
                    "-DSOURCE_DIR=${CMAKE_SOURCE_DIR}"
                    "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
                    "-DVERSION=${PROJECT_VERSION}"
                    "-DQT_VERSION=${Qt6_VERSION}"
                    "-DEXE=$<TARGET_FILE:can_analyzer>"
                    "-DLOGTOOL=$<TARGET_FILE:can_logtool>"
                    "-DWINDEPLOYQT=${CAN_WINDEPLOYQT}"
                    "-DRUNTIME_LIBS=${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}"
                    -P "${CMAKE_SOURCE_DIR}/cmake/package.cmake"
            DEPENDS can_analyzer can_logtool
            COMMENT "Building the portable Windows package"
            VERBATIM)
    endif()
endif()
