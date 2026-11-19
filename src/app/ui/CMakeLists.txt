# CanUi: design tokens (Theme singleton), icons and reusable components.
qt_add_library(canui STATIC)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

file(GLOB CAN_UI_ICONS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS icons/*.svg)

qt_add_qml_module(canui
    URI CanUi
    VERSION 1.0
    QML_FILES
        Theme.qml
        Icon.qml
        Card.qml
        Chip.qml
        IconButton.qml
        FocusRing.qml
        SectionHeader.qml
        EmptyState.qml
        Kbd.qml
        Divider.qml
        ExtTag.qml
    RESOURCES
        ${CAN_UI_ICONS}
)

target_link_libraries(canui PRIVATE Qt6::Quick Qt6::QuickControls2 can_warnings)
