import QtQuick
import CanUi

// Compact grid of decoded signals: name, value + unit, enumeration text.
// Used by the expanded Trace row and the expanded Messages row.
Item {
    id: grid
    property var signalList: []   // [{ name, value, unit, enumText, outOfRange }]
    property string title: ""
    readonly property int cellWidth: 232
    readonly property int columnsCount: Math.max(1, Math.floor((width + Theme.space.md) / (cellWidth + Theme.space.md)))

    implicitHeight: (title.length > 0 ? titleText.implicitHeight + 4 : 0)
                    + Math.ceil(signalList.length / columnsCount) * 24 + 6

    Text {
        id: titleText
        visible: grid.title.length > 0
        text: grid.title
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
        font.weight: Font.DemiBold
    }

    Grid {
        y: grid.title.length > 0 ? titleText.implicitHeight + 4 : 2
        columns: grid.columnsCount
        columnSpacing: Theme.space.md
        rowSpacing: 0
        Repeater {
            model: grid.signalList
            Item {
                required property var modelData
                width: grid.cellWidth
                height: 24
                Rectangle { // quiet guide line under each cell
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 2
                    height: 1
                    color: Theme.alpha(Theme.border, 0.45)
                }
                Text {
                    id: n
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: 112
                    elide: Text.ElideRight
                    text: modelData.name
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                }
                Text {
                    anchors.left: n.right
                    anchors.leftMargin: Theme.space.sm
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    text: modelData.value + (modelData.unit ? " " + modelData.unit : "")
                          + (modelData.enumText ? "  " + modelData.enumText : "")
                    color: modelData.outOfRange ? Theme.error : Theme.text
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.type.small
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
