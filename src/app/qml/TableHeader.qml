import QtQuick
import CanUi

// Sticky column header shared by the Trace and Messages tables.
// `columns`: [{ title, width (<=0 = fill), align: "left"|"right", key (sort id) }]
Item {
    id: header
    property var columns: []
    property int sortKey: -1
    property bool sortAscending: true
    property bool sortable: false
    property int cellSpacing: 20
    property int leftInset: Theme.space.lg
    signal sortRequested(int key)

    implicitHeight: Theme.size.header

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    Row {
        id: row
        x: header.leftInset
        height: parent.height
        spacing: header.cellSpacing

        Repeater {
            model: header.columns
            delegate: Item {
                id: cell
                required property var modelData
                readonly property bool fill: modelData.width <= 0
                readonly property bool active: header.sortable && modelData.key === header.sortKey
                width: fill ? Math.max(80, header.width - row.x - fixedWidth() - header.leftInset) : modelData.width
                height: row.height

                function fixedWidth() {
                    let w = 0
                    for (let i = 0; i < header.columns.length; ++i)
                        if (header.columns[i].width > 0)
                            w += header.columns[i].width + header.cellSpacing
                    return w
                }

                Row {
                    id: labelRow
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: cell.modelData.align === "right" ? parent.right : undefined
                    anchors.left: cell.modelData.align === "right" ? undefined : parent.left
                    layoutDirection: cell.modelData.align === "right" ? Qt.RightToLeft : Qt.LeftToRight

                    Text {
                        text: cell.modelData.title.toUpperCase()
                        color: cell.active ? Theme.text : (hover.hovered && header.sortable ? Theme.textMuted : Theme.textFaint)
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.type.caption
                        font.weight: Font.DemiBold
                        font.letterSpacing: Theme.type.captionSpacing
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Icon {
                        visible: header.sortable
                        opacity: cell.active ? 1 : (hover.hovered ? 0.5 : 0)
                        name: cell.active && !header.sortAscending ? "chevron-down" : "chevron-up"
                        size: 12
                        color: cell.active ? Theme.accentText : Theme.textFaint
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
                    }
                }

                HoverHandler {
                    id: hover
                    enabled: header.sortable
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    enabled: header.sortable
                    onTapped: header.sortRequested(cell.modelData.key)
                }
            }
        }
    }
}
