# QML application: design system (CanUi), custom Controls style (CanStyle)
# and the screens (CanAnalyzer module).
add_subdirectory(ui)
add_subdirectory(style)

qt_add_executable(can_analyzer WIN32 MACOSX_BUNDLE
    main.cpp
    demodirector.h demodirector.cpp
    plotbench.h plotbench.cpp
)

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
        qml/format.js
    SOURCES
        sparkline.h sparkline.cpp
        plotcanvas.h plotcanvas.cpp
)

target_compile_definitions(can_analyzer PRIVATE CAN_ANALYZER_VERSION="${PROJECT_VERSION}")

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
    endif()
endif()
