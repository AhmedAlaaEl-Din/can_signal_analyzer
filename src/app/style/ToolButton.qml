import QtQuick
import QtQuick.Templates as T
import CanUi

// Low-emphasis toolbar action: icon + optional label, no border.
// When checkable and checked it takes a subtle accent tint.
T.ToolButton {
    id: control
    property string iconName: ""
    property int iconSize: 15
    property color iconColor: _fg

    readonly property color _fg: !enabled ? Theme.textFaint
                               : checked ? Theme.accentText
                               : hovered ? Theme.text : Theme.textMuted

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    leftPadding: text.length > 0 ? Theme.space.sm + 2 : Theme.space.sm
    rightPadding: text.length > 0 ? Theme.space.md : Theme.space.sm
    topPadding: 0
    bottomPadding: 0
    spacing: Theme.space.sm - 1
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body
    font.weight: Font.Medium

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: control.spacing
            Icon {
                visible: control.iconName.length > 0
                name: control.iconName
                size: control.iconSize
                color: control.iconColor
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                visible: control.text.length > 0
                text: control.text
                font: control.font
                color: control._fg
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Rectangle {
        implicitWidth: Theme.size.control
        implicitHeight: Theme.size.control
        radius: Theme.radius.md
        color: control.down ? Theme.pressed
             : control.checked ? Theme.accentSubtle
             : control.hovered ? Theme.hover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        FocusRing { target: control }
    }
}
