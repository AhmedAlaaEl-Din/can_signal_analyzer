import QtQuick
import QtQuick.Templates as T

// Square, icon-only button with hover/press state layer, focus ring and tooltip.
T.AbstractButton {
    id: control
    property string iconName: ""
    property string tip: ""
    property color iconColor: Theme.textMuted
    property int iconSize: Theme.size.icon
    property int extent: Theme.size.control

    implicitWidth: extent
    implicitHeight: extent
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: tip

    T.ToolTip.visible: hovered && tip.length > 0
    T.ToolTip.text: tip
    T.ToolTip.delay: 600

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.checked ? Theme.accentText
                 : control.hovered ? Theme.text : control.iconColor
            opacity: control.enabled ? 1 : 0.4
        }
    }
    background: Rectangle {
        radius: Theme.radius.md
        color: control.down ? Theme.pressed
             : control.checked ? Theme.accentSubtle
             : control.hovered ? Theme.hover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
        FocusRing { target: control }
    }
}
