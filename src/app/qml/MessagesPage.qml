import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Per-ID overview: count, measured period, last data with changed bytes
// briefly highlighted. Sortable by clicking a column header.
Item {
    id: page
    property bool active: false
    property int hoverRow: -1   // demo helper

    readonly property int cellSpacing: 22
    // `key` values match MessageOverviewModel::Column
    readonly property var columns: [
        { title: qsTr("ID"), width: 104, align: "left", key: 0 },
        { title: qsTr("CH"), width: 28, align: "right", key: 1 },
        { title: qsTr("DLC"), width: 32, align: "right", key: 2 },
        { title: qsTr("COUNT"), width: 84, align: "right", key: 3 },
        { title: qsTr("PERIOD (ms)"), width: 92, align: "right", key: 4 },
        { title: qsTr("LAST SEEN (s)"), width: 104, align: "right", key: 5 },
        { title: qsTr("DATA"), width: 0, align: "left", key: 6 }
    ]

    opacity: active ? 1 : 0
    visible: opacity > 0.01
    enabled: active
    property real rise: active ? 0 : 10
    transform: Translate { y: page.rise }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    Behavior on rise { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }

    Card {
        anchors.fill: parent
        elevation: 1

        TableHeader {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            columns: page.columns
            cellSpacing: page.cellSpacing
            sortable: true
            sortKey: Session.overviewModel.sortColumn
            sortAscending: Session.overviewModel.sortAscending
            onSortRequested: (key) => Session.overviewModel.sortBy(key)
        }

        ListView {
            id: list
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 1
            anchors.topMargin: 0
            anchors.bottomMargin: 6
            clip: true
            model: Session.overviewModel
            boundsBehavior: Flickable.StopAtBounds
            bottomMargin: pill.shown ? 52 : 0
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            // Smoothly slide rows when the sort order changes.
            displaced: Transition {
                NumberAnimation { properties: "y"; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }
            move: Transition {
                NumberAnimation { properties: "y"; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }
            add: Transition {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.slow; easing.type: Theme.motion.easing }
            }

            delegate: Item {
                id: row
                required property int index
                required property string frameId
                required property bool extended
                required property int channel
                required property int dlc
                required property real frameCount
                required property string period
                required property string lastSeen
                required property var bytes
                required property int changedMask
                required property real changeSerial

                width: list.width
                height: Theme.size.row + 6

                Rectangle {
                    anchors.fill: parent
                    color: hover.hovered || page.hoverRow === row.index ? Theme.hover
                         : (row.index % 2 === 1 ? Theme.zebra : "transparent")
                }
                HoverHandler { id: hover }

                Row {
                    x: Theme.space.lg
                    height: parent.height
                    spacing: page.cellSpacing

                    Item {
                        width: page.columns[0].width
                        height: parent.height
                        Text {
                            id: idText
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            text: row.frameId
                            color: Theme.text
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.type.mono
                            font.weight: Font.DemiBold
                        }
                        ExtTag {
                            visible: row.extended
                            anchors.left: idText.right
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                    Text {
                        width: page.columns[1].width; height: parent.height
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                        text: row.channel
                        color: Theme.textFaint
                        font.family: Theme.fontMono; font.pixelSize: Theme.type.mono
                    }
                    Text {
                        width: page.columns[2].width; height: parent.height
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                        text: row.dlc
                        color: Theme.textMuted
                        font.family: Theme.fontMono; font.pixelSize: Theme.type.mono
                    }
                    Text {
                        width: page.columns[3].width; height: parent.height
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                        text: Fmt.grouped(row.frameCount)
                        color: Theme.text
                        font.family: Theme.fontMono; font.pixelSize: Theme.type.mono
                    }
                    Text {
                        width: page.columns[4].width; height: parent.height
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                        text: row.period
                        color: Theme.text
                        font.family: Theme.fontMono; font.pixelSize: Theme.type.mono
                    }
                    Text {
                        width: page.columns[5].width; height: parent.height
                        horizontalAlignment: Text.AlignRight; verticalAlignment: Text.AlignVCenter
                        text: row.lastSeen
                        color: Theme.textMuted
                        font.family: Theme.fontMono; font.pixelSize: Theme.type.mono
                    }
                    // Data bytes, each in its own cell so changes can flash.
                    Row {
                        height: parent.height
                        spacing: 4
                        Repeater {
                            model: 8
                            delegate: Item {
                                id: byteCell
                                required property int index
                                readonly property bool present: index < row.bytes.length
                                onPresentChanged: if (!present) { flashAnim.stop(); flash.opacity = 0 }
                                width: 22
                                height: parent.height

                                Rectangle {
                                    id: flash
                                    anchors.centerIn: parent
                                    width: 24
                                    height: 20
                                    radius: Theme.radius.sm
                                    color: Theme.highlightChange
                                    opacity: 0
                                    visible: byteCell.present
                                }
                                Text {
                                    anchors.centerIn: parent
                                    text: byteCell.present ? row.bytes[byteCell.index] : ""
                                    color: Theme.text
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.type.mono
                                }
                                NumberAnimation {
                                    id: flashAnim
                                    target: flash
                                    property: "opacity"
                                    from: 1
                                    to: 0
                                    duration: Theme.motion.highlight
                                    easing.type: Easing.OutQuad
                                }
                                Connections {
                                    target: row
                                    function onChangeSerialChanged() {
                                        if (byteCell.present && (row.changedMask & (1 << byteCell.index)))
                                            flashAnim.restart()
                                    }
                                    // Rows are re-assigned when the sort order changes:
                                    // never carry a flash over to another message.
                                    function onFrameIdChanged() {
                                        flashAnim.stop()
                                        flash.opacity = 0
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        FloatingPill {
            id: pill
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space.lg
        }

        EmptyState {
            anchors.centerIn: list
            width: list.width
            visible: list.count === 0
            iconName: "grid"
            title: qsTr("No messages seen yet")
            message: qsTr("Each identifier on the bus gets one row here, with its count, measured period and latest payload.")
            shortcut: Session.running ? "" : "F5"
            shortcutLabel: qsTr("to start")
        }
    }
}
