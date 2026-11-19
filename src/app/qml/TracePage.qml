import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Live trace: filter bar + scrolling frame table that follows the live end
// until the user scrolls up.
Item {
    id: page
    property bool active: true
    property real selectedSeq: -1

    readonly property int cellSpacing: 22
    readonly property var columns: [
        { title: qsTr("Time (s)"), width: 104, align: "right" },
        { title: qsTr("Ch"), width: 28, align: "right" },
        { title: qsTr("ID"), width: 104, align: "left" },
        { title: qsTr("Δt (ms)"), width: 72, align: "right" },
        { title: qsTr("DLC"), width: 32, align: "right" },
        { title: qsTr("Data"), width: 0, align: "left" }
    ]

    // Page transition: fade + short rise.
    opacity: active ? 1 : 0
    visible: opacity > 0.01
    enabled: active
    property real rise: active ? 0 : 10
    transform: Translate { y: page.rise }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    Behavior on rise { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }

    function focusFilter() { idField.forceActiveFocus(); idField.selectAll() }
    function setIdFilterText(text) {
        idField.text = text
        Session.idFilter = text
    }
    function jumpToLive() {
        list.follow = true
        list.scrollToEnd()
    }
    function scrollUpBy(rows) {
        list.follow = false
        list.contentY = Math.max(list.originY, list.contentY - rows * Theme.size.row)
    }
    function selectVisibleRow(offset) {
        const idx = list.indexAt(20, list.contentY + offset * Theme.size.row + Theme.size.row / 2)
        if (idx >= 0) {
            const item = list.itemAtIndex(idx)
            if (item)
                page.selectedSeq = item.seq
        }
    }

    // New rows fade in only while they arrive slowly enough to be noticed.
    // At high rates every visible row would be mid-fade all the time, which
    // just makes the data look washed out, so the stream is shown directly.
    readonly property real rowRate: Session.framesPerSecond * list.count / Math.max(1, Session.frameModel.bufferedCount)
    readonly property bool fadeNewRows: list.follow && !Session.paused
                                        && rowRate < Math.max(8, list.height / Theme.size.row) * 1.5

    Connections {
        target: Session
        function onCleared() { page.selectedSeq = -1; list.follow = true }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        // ------------------------------------------------------- filter bar
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            TextField {
                id: idField
                iconName: "filter"
                monospace: true
                placeholderText: qsTr("Filter IDs  e.g. 123, 1A0-1AF")
                Layout.preferredWidth: 280
                error: Session.idFilterError.length > 0
                onTextEdited: idDebounce.restart()
                Timer {
                    id: idDebounce
                    interval: 160
                    onTriggered: Session.idFilter = idField.text
                }
                Keys.onEscapePressed: { text = ""; Session.idFilter = ""; page.forceActiveFocus() }
            }
            TextField {
                id: dlcField
                monospace: true
                placeholderText: qsTr("DLC  e.g. 8 or 0-4")
                Layout.preferredWidth: 150
                error: Session.dlcFilterError.length > 0
                onTextEdited: dlcDebounce.restart()
                Timer {
                    id: dlcDebounce
                    interval: 160
                    onTriggered: Session.dlcFilter = dlcField.text
                }
                Keys.onEscapePressed: { text = ""; Session.dlcFilter = ""; page.forceActiveFocus() }
            }
            ComboBox {
                id: formatBox
                Layout.preferredWidth: 164
                model: [qsTr("All frames"), qsTr("Standard (11-bit)"), qsTr("Extended (29-bit)")]
                currentIndex: Session.formatFilter
                onActivated: (index) => Session.formatFilter = index
            }
            Button {
                variant: "ghost"
                iconName: "close"
                text: qsTr("Reset")
                visible: Session.filterActive || idField.text.length > 0 || dlcField.text.length > 0
                onClicked: {
                    idField.text = ""
                    dlcField.text = ""
                    Session.clearFilter()
                }
            }

            Item { Layout.fillWidth: true }

            // Right side: validation message or row counter.
            Row {
                spacing: Theme.space.xs + 2
                visible: Session.idFilterError.length > 0 || Session.dlcFilterError.length > 0
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: Theme.error
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: Session.idFilterError.length > 0 ? Session.idFilterError : Session.dlcFilterError
                    color: Theme.error
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            Text {
                visible: Session.idFilterError.length === 0 && Session.dlcFilterError.length === 0
                text: Session.filterActive
                      ? qsTr("%1 of %2 frames").arg(Fmt.grouped(list.count)).arg(Fmt.grouped(Session.frameModel.bufferedCount))
                      : qsTr("%1 frames").arg(Fmt.grouped(list.count))
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                font.features: { "tnum": 1 }
            }
        }

        // ------------------------------------------------------------ table
        Card {
            Layout.fillWidth: true
            Layout.fillHeight: true
            elevation: 1

            TableHeader {
                id: tableHeader
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.leftMargin: 1
                anchors.rightMargin: 1
                columns: page.columns
                cellSpacing: page.cellSpacing
            }

            ListView {
                id: list
                anchors.top: tableHeader.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 1
                anchors.topMargin: 0
                anchors.bottomMargin: 6
                clip: true
                model: Session.frameModel
                reuseItems: true
                boundsBehavior: Flickable.StopAtBounds
                boundsMovement: Flickable.StopAtBounds
                pixelAligned: true
                activeFocusOnTab: true

                property bool follow: true
                property bool _programmatic: false

                function atEnd() {
                    return contentY + height >= contentHeight + originY - 2
                }
                function scrollToEnd() {
                    _programmatic = true
                    positionViewAtEnd()
                    _programmatic = false
                }

                onCountChanged: if (follow) Qt.callLater(scrollToEnd)
                onContentYChanged: {
                    if (_programmatic)
                        return
                    if (!follow && atEnd())
                        follow = true
                    else if (follow && (dragging || flicking || vbar.pressed) && !atEnd())
                        follow = false
                }
                onHeightChanged: if (follow) Qt.callLater(scrollToEnd)

                WheelHandler {
                    blocking: false
                    onWheel: (event) => {
                        if (event.angleDelta.y > 0)
                            list.follow = false
                    }
                }
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_PageUp || event.key === Qt.Key_Up || event.key === Qt.Key_Home)
                        list.follow = false
                    if (event.key === Qt.Key_Home) {
                        list.positionViewAtBeginning()
                        event.accepted = true
                    }
                }

                ScrollBar.vertical: ScrollBar {
                    id: vbar
                    policy: ScrollBar.AsNeeded
                }

                delegate: TraceRow {
                    width: list.width
                    columns: page.columns
                    cellSpacing: page.cellSpacing
                    selected: seq === page.selectedSeq
                    animateIn: page.fadeNewRows
                    onClicked: page.selectedSeq = (selected ? -1 : seq)
                }
            }

            // Empty states
            EmptyState {
                anchors.centerIn: list
                width: list.width
                visible: list.count === 0 && (!Session.running || Session.filterActive)
                iconName: Session.filterActive ? "filter" : "inbox"
                title: Session.filterActive ? qsTr("No frames match the filter")
                                            : qsTr("No frames yet")
                message: Session.filterActive
                         ? qsTr("Nothing in the buffer passes the current ID, length and format criteria.")
                         : qsTr("Start the simulator to stream CAN traffic into the trace.")
                shortcut: Session.filterActive ? "Esc" : "F5"
                shortcutLabel: Session.filterActive ? qsTr("to reset the filter") : qsTr("to start")
            }

            // Floating "jump to live" pill
            Button {
                id: livePill
                variant: "primary"
                iconName: "arrow-down"
                text: qsTr("Jump to live")
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.space.lg
                height: 30
                opacity: !list.follow && list.count > 0 ? 1 : 0
                visible: opacity > 0.01
                scale: !list.follow ? 1 : 0.92
                Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                onClicked: page.jumpToLive()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Resume auto-scroll  (End)")
                ToolTip.delay: 600
            }
        }
    }
}
