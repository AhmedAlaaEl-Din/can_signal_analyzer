import QtQuick

// Keycap used to show keyboard shortcuts.
Rectangle {
    property alias text: label.text
    implicitWidth: Math.max(22, label.implicitWidth + 10)
    implicitHeight: 20
    radius: Theme.radius.sm
    color: Theme.raised
    border.width: 1
    border.color: Theme.borderStrong
    Rectangle { // bottom edge gives the key a little depth
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        height: 1
        color: Theme.borderStrong
    }
    Text {
        id: label
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -0.5
        color: Theme.textMuted
        font.family: Theme.fontMono
        font.pixelSize: Theme.type.caption
    }
}
