import QtQuick
import QtQuick.Layouts

// Title + optional supporting text, with a slot on the right for actions.
Item {
    id: header
    property string title: ""
    property string subtitle: ""
    default property alias actions: actionRow.data

    implicitHeight: Math.max(textColumn.implicitHeight, actionRow.implicitHeight)
    implicitWidth: textColumn.implicitWidth + actionRow.implicitWidth + Theme.space.lg

    Column {
        id: textColumn
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2
        Text {
            text: header.title
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.title
            font.weight: Font.DemiBold
        }
        Text {
            visible: header.subtitle.length > 0
            text: header.subtitle
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
        }
    }
    RowLayout {
        id: actionRow
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space.sm
    }
}
