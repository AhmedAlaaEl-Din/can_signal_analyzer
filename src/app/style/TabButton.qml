import QtQuick
import QtQuick.Templates as T
import CanUi

// Tab label with optional icon and a count badge.
T.TabButton {
    id: control
    property string iconName: ""
    property string badge: ""

    readonly property color _fg: checked ? Theme.text : hovered ? Theme.text : Theme.textMuted

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 40
    leftPadding: Theme.space.md
    rightPadding: Theme.space.md
    topPadding: 0
    bottomPadding: 2
    spacing: Theme.space.sm - 1
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body + 1
    font.weight: checked ? Font.DemiBold : Font.Medium

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
                size: 15
                color: control.checked ? Theme.accentText : control._fg
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: control.text
                font: control.font
                color: control._fg
                anchors.verticalCenter: parent.verticalCenter
                Behavior on color { ColorAnimation { duration: Theme.motion.normal } }
            }
            Rectangle {
                visible: control.badge.length > 0
                anchors.verticalCenter: parent.verticalCenter
                height: 18
                width: Math.max(22, badgeText.implicitWidth + 12)
                radius: 9
                color: control.checked ? Theme.accentSubtle : Theme.alpha(Theme.textMuted, 0.12)
                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: control.badge
                    color: control.checked ? Theme.accentText : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
            }
        }
    }

    background: Rectangle {
        implicitHeight: 40
        radius: Theme.radius.md
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.bottomMargin: 6
        color: control.down ? Theme.pressed : control.hovered && !control.checked ? Theme.hover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
        FocusRing { target: control }
    }
}
