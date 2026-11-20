import QtQuick
import QtQuick.Templates as T
import CanUi

// Variants: "primary" (accent fill), "secondary" (outlined), "ghost" (text only).
T.Button {
    id: control
    property string variant: "secondary"
    property string iconName: ""
    property int iconSize: 14

    readonly property bool _primary: variant === "primary"
    readonly property color _fg: !enabled ? Theme.textFaint
                               : _primary ? Theme.textOnAccent
                               : variant === "ghost" && !hovered ? Theme.textMuted
                               : Theme.text

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    leftPadding: text.length > 0 ? Theme.space.md + (iconName.length > 0 ? -2 : 2) : Theme.space.sm
    rightPadding: text.length > 0 ? Theme.space.md + 2 : Theme.space.sm
    topPadding: 0
    bottomPadding: 0
    spacing: Theme.space.sm - 1
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body
    font.weight: Font.DemiBold

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
                color: control._fg
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
        implicitWidth: 72
        implicitHeight: Theme.size.control
        radius: Theme.radius.md
        color: control._primary
               ? (control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent)
               : (control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent")
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: control.hovered ? Theme.borderStrong : Theme.border
        opacity: control.enabled ? 1 : 0.6
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        FocusRing { target: control }
    }
}
