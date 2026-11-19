import QtQuick

// Friendly placeholder for empty tables: icon in a soft circle, title, hint,
// and an optional keyboard hint (for example "F5").
Item {
    id: empty
    property string iconName: "inbox"
    property string title: ""
    property string message: ""
    property string shortcut: ""
    property string shortcutLabel: ""

    implicitWidth: column.implicitWidth
    implicitHeight: column.implicitHeight

    Column {
        id: column
        anchors.centerIn: parent
        spacing: Theme.space.md
        width: Math.max(120, Math.min(380, empty.width - 2 * Theme.space.lg))

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56
            height: 56
            radius: 28
            color: Theme.accentSubtle
            Icon {
                anchors.centerIn: parent
                name: empty.iconName
                size: 26
                color: Theme.accentText
            }
        }
        Item { width: 1; height: Theme.space.xxs }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: empty.title
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.type.title + 1
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: empty.message
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.body
            lineHeight: 1.25
            wrapMode: Text.WordWrap
            visible: text.length > 0
        }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.space.sm
            visible: empty.shortcut.length > 0
            Kbd { text: empty.shortcut; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: empty.shortcutLabel
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
