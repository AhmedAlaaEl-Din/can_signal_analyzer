import QtQuick

// Small "EXT" marker next to 29-bit identifiers.
Rectangle {
    implicitWidth: label.implicitWidth + 8
    implicitHeight: 15
    radius: 3
    color: "transparent"
    border.width: 1
    border.color: Theme.borderStrong
    Text {
        id: label
        anchors.centerIn: parent
        text: "EXT"
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.4
    }
}
