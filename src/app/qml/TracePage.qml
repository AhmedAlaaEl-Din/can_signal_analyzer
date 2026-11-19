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
    readonly property bool showNames: ApplicationWindow.window ? ApplicationWindow.window.showNames : false
    readonly property var columns: [
        { title: qsTr("TIME (s)"), width: 104, align: "right" },
        { title: qsTr("CH"), width: 28, align: "right" },
        { title: showNames ? qsTr("MESSAGE") : qsTr("ID"), width: showNames ? 168 : 104, align: "left" },
        { title: qsTr("Δt (ms)"), width: 72, align: "right" },
        { title: qsTr("DLC"), width: 32, align: "right" },
        { title: qsTr("DATA"), width: 0, align: "left" }
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
    signal demoRequested()
    signal openLogRequested()

    // ---- find (Ctrl+F, F3 / Shift+F3) ----
    property bool findOpen: false
    property int matches: 0
    function openFind() {
        findOpen = true
        findField.forceActiveFocus()
        findField.selectAll()
    }
    function closeFind() {
        findOpen = false
        findField.text = ""
        Session.searchText = ""
        list.forceActiveFocus()
    }
    function setSearch(text) {
        findOpen = true
        findField.text = text
        Session.searchText = text
        page.matches = Session.matchCount()
    }
    // Jump to the next / previous match from the selected row (or the view).
    function findNext(forward) {
        if (!Session.searchValid)
            return false
        let from = page.selectedSeq >= 0 ? Session.frameModel.rowForSeq(page.selectedSeq) : -1
        if (from < 0)
            from = forward ? -1 : list.count
        const row = Session.findNext(from, forward)
        page.matches = Session.matchCount()
        if (row < 0)
            return false
        list.follow = false
        list._programmatic = true
        list.positionViewAtIndex(row, ListView.Center)
        list._programmatic = false
        list.rememberAnchor()
        const item = list.itemAtIndex(row)
        page.selectedSeq = item ? item.seq : -1
        return true
    }
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
        list.rememberAnchor()
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

        // ------------------------------------------------------------ find bar
        Rectangle {
            id: findBar
            Layout.fillWidth: true
            Layout.preferredHeight: page.findOpen ? 44 : 0
            visible: Layout.preferredHeight > 1
            clip: true
            radius: Theme.radius.md
            color: Theme.surface
            border.width: 1
            border.color: Session.searchError.length > 0 ? Theme.alpha(Theme.error, 0.5) : Theme.border
            Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space.sm
                anchors.rightMargin: Theme.space.sm
                spacing: Theme.space.sm
                TextField {
                    id: findField
                    iconName: "search"
                    monospace: true
                    Layout.preferredWidth: Math.min(340, page.width * 0.34)
                    placeholderText: qsTr("Find  1A4 · 12 ?? FF · VehicleSpeed > 100")
                    error: Session.searchError.length > 0
                    onTextEdited: findDebounce.restart()
                    Timer {
                        id: findDebounce
                        interval: 180
                        onTriggered: { Session.searchText = findField.text; page.matches = Session.matchCount() }
                    }
                    Keys.onReturnPressed: (e) => { Session.searchText = text; page.findNext(!(e.modifiers & Qt.ShiftModifier)) }
                    Keys.onEnterPressed: (e) => { Session.searchText = text; page.findNext(!(e.modifiers & Qt.ShiftModifier)) }
                    Keys.onEscapePressed: page.closeFind()
                }
                IconButton {
                    iconName: "arrow-up"
                    enabled: Session.searchValid
                    tip: qsTr("Previous match  (Shift+F3)")
                    onClicked: page.findNext(false)
                }
                IconButton {
                    iconName: "arrow-down"
                    enabled: Session.searchValid
                    tip: qsTr("Next match  (F3)")
                    onClicked: page.findNext(true)
                }
                // Interpretation of the query (or why it is not valid).
                Chip {
                    visible: Session.searchValid || Session.searchError.length > 0
                    tone: Session.searchValid ? "accent" : "error"
                    iconName: Session.searchValid ? "search" : "alert"
                    label: Session.searchValid ? Session.searchDescription : Session.searchError
                    Layout.maximumWidth: page.width * 0.4
                }
                Text {
                    visible: Session.searchValid
                    text: page.matches === 1 ? qsTr("1 match in the buffer") : qsTr("%1 matches in the buffer").arg(Fmt.grouped(page.matches))
                    color: page.matches > 0 ? Theme.textMuted : Theme.warning
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    font.features: { "tnum": 1 }
                }
                Item { Layout.fillWidth: true }
                Text {
                    visible: page.width > 1100 && !Session.searchValid && Session.searchError.length === 0
                    text: qsTr("ID or message · data bytes with ?? · Signal > value")
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                }
                IconButton {
                    iconName: "close"
                    tip: qsTr("Close find  (Esc)")
                    onClicked: page.closeFind()
                }
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

                // Keep the last rows readable above the floating pill.
                bottomMargin: pill.shown ? 52 : 0
                onBottomMarginChanged: if (follow) Qt.callLater(scrollToEnd)

                function atEnd() {
                    return contentY + height >= contentHeight + originY + bottomMargin - 2
                }
                function scrollToEnd() {
                    _programmatic = true
                    positionViewAtEnd()
                    // positionViewAtEnd() ignores bottomMargin; include it so the
                    // newest row stays visible above the floating pill.
                    if (bottomMargin > 0)
                        contentY = Math.max(contentY, contentHeight + originY + bottomMargin - height)
                    _programmatic = false
                }

                onCountChanged: if (follow) Qt.callLater(scrollToEnd)

                // Detached view anchoring: remember the frame at the top of the
                // view (by sequence number) and, after the ring buffer evicts
                // older rows, put that same frame back at the top.
                property real anchorSeq: -1
                function rememberAnchor() {
                    const idx = indexAt(20, contentY + 2)
                    const item = idx >= 0 ? itemAtIndex(idx) : null
                    anchorSeq = item ? item.seq : -1
                }
                function restoreAnchor() {
                    if (follow || anchorSeq < 0)
                        return
                    const row = Math.max(0, Session.frameModel.rowForSeq(anchorSeq))
                    _programmatic = true
                    positionViewAtIndex(row, ListView.Beginning)
                    _programmatic = false
                    if (row === 0)
                        rememberAnchor() // anchor itself was evicted: keep the oldest row
                }
                Connections {
                    target: Session.frameModel
                    function onRowsEvicted(count) {
                        if (!list.follow)
                            Qt.callLater(list.restoreAnchor)
                    }
                    // Once the ring buffer is full every batch removes as many rows
                    // as it adds, so `count` stops changing: follow on inserts.
                    function onRowsInserted() {
                        if (list.follow)
                            Qt.callLater(list.scrollToEnd)
                    }
                }
                onFollowChanged: if (!follow) Qt.callLater(rememberAnchor)
                onContentYChanged: {
                    if (_programmatic)
                        return
                    if (!follow && atEnd())
                        follow = true
                    else if (follow && (dragging || flicking || vbar.pressed) && !atEnd())
                        follow = false
                    if (!follow)
                        rememberAnchor()
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
                    showNames: page.showNames
                    decoded: selected && Session.database.loaded ? Session.frameModel.decodedSignals(index) : []
                    onClicked: page.selectedSeq = (selected ? -1 : seq)
                }
            }

            // Empty states
            WelcomeState {
                anchors.centerIn: list
                width: list.width
                visible: list.count === 0 && !Session.running && !Session.filterActive && !Session.replayMode
                         && Session.totalFrames === 0
                onDemoRequested: page.demoRequested()
                onOpenLogRequested: page.openLogRequested()
            }
            EmptyState {
                anchors.centerIn: list
                width: list.width
                visible: list.count === 0 && (!Session.running || Session.filterActive)
                         && !(!Session.running && !Session.filterActive && !Session.replayMode && Session.totalFrames === 0)
                iconName: Session.filterActive ? "filter" : "inbox"
                title: Session.filterActive ? qsTr("No frames match the filter")
                                            : qsTr("No frames yet")
                message: Session.filterActive
                         ? qsTr("Nothing in the buffer passes the current ID, length and format criteria.")
                         : qsTr("Start the simulator to stream CAN traffic into the trace.")
                shortcut: Session.filterActive ? "Esc" : "F5"
                shortcutLabel: Session.filterActive ? qsTr("to reset the filter") : qsTr("to start")
            }

            // Floating pill: "paused, N waiting" or "jump to live"
            FloatingPill {
                id: pill
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Theme.space.lg
                detached: !list.follow && list.count > 0
                onJumpRequested: page.jumpToLive()
            }
        }
    }
}
