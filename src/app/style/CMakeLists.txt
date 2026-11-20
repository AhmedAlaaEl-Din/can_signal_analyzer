# CanStyle: a Qt Quick Controls style. Selected at startup with
# QQuickStyle::setStyle("CanStyle"); controls not provided here fall back to Basic.
qt_add_library(canstyle STATIC)

qt_add_qml_module(canstyle
    URI CanStyle
    VERSION 1.0
    QML_FILES
        Button.qml
        ToolButton.qml
        TextField.qml
        ComboBox.qml
        TabBar.qml
        TabButton.qml
        ScrollBar.qml
        Switch.qml
        ToolTip.qml
    IMPORTS
        QtQuick.Controls.Basic
)

target_link_libraries(canstyle PRIVATE Qt6::Quick Qt6::QuickControls2 can_warnings)
