import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import CanUi
import CanAnalyzer
import CanAnalyzer.Core

// Live plots: a signal picker on the left, stacked lanes sharing one time
// axis, crosshair with a value tooltip, measurement cursors A/B with deltas,
// follow-live with zoom/pan, thresholds and PNG/CSV export.
Item {
    id: page
    property bool active: false
    readonly property var plot: Session.plot
    readonly property var lanesModel: Session.plotModel
    readonly property bool hasDb: Session.database.loaded
    readonly property real axisWidth: 64
    readonly property real rightPad: 52   // room for a right-hand axis
    readonly property real plotWidth: Math.max(10, plotCard.width - 2 - axisWidth - rightPad)
    readonly property var timeTicks: (plot.viewStartUs, plot.viewEndUs, plotWidth, plot.timeTicks(plotWidth))
    property real hoverX: -1        // crosshair position inside the plot region (-1 = none)
    property bool dragging: false   // a signal is being dragged from the picker
    property bool demoMode: false   // scripted capture: ignore the real mouse

    opacity: active ? 1 : 0
    visible: opacity > 0.01
    enabled: active
    property real rise: active ? 0 : 10
    transform: Translate { y: page.rise }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    Behavior on rise { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }

    // Smooth scrolling: advance the shared time axis every rendered frame.
    FrameAnimation {
        running: page.active
        onTriggered: Session.plot.advance(frameTime)
    }

    // ---------------------------------------------------------------- helpers
    function addSignalByName(message, signal, laneIndex) {
        const id = Session.signalModel.signalIdByName(message, signal)
        if (id >= 0)
            lanesModel.addSignal(id, laneIndex)
    }
    function zoom(factor) { plot.zoomAt(page.hoverX >= 0 ? page.hoverX / page.plotWidth : 1.0, factor) }
    function toggleCursors() { if (plot.hasCursors) plot.clearCursors(); else plot.placeCursors() }
    function showToast(text, tone) { toast.text = text; toast.tone = tone || "ok"; toast.shown = true; toastTimer.restart() }
    function allVisibleSeries() {
        const out = []
        for (let i = 0; i < lanes.count; ++i) {
            const l = lanes.itemAtIndex(i)
            if (l)
                for (const s of l.visibleSeries)
                    out.push({ series: s, lane: l })
        }
        return out
    }
    function exportCsv(url) {
        const ids = [], names = []
        for (const e of allVisibleSeries())
            if (ids.indexOf(e.series.signalId) < 0) { ids.push(e.series.signalId); names.push(e.series.name) }
        if (Session.plot.exportCsv(url, ids, names))
            showToast(qsTr("Saved %1 (%2 signals)").arg(Session.plot.localPath(url).split(/[\\/]/).pop()).arg(ids.length))
        else
            showToast(Session.plot.lastError, "error")
    }
    function exportPng(url) {
        const path = Session.plot.localPath(url)
        plotCard.grabToImage(function(result) {
            if (result.saveToFile(path))
                page.showToast(qsTr("Saved %1").arg(path.split(/[\\/]/).pop()))
            else
                page.showToast(qsTr("Could not save the image to that location."), "error")
        })
    }

    // Signal picker data (all signals of the database).
    readonly property var pickerItems: {
        const n = Session.signalModel.signalCount   // re-evaluates when the database changes
        const items = []
        for (let i = 0; i < n; ++i)
            items.push(lanesModel.signalInfo(i))
        return items.filter(x => x.name !== undefined)
    }
    Connections {
        target: Session.database
        function onChanged() { Session.plot.clearCursors() }
    }

    FileDialog {
        id: pngDialog
        title: qsTr("Export plot as PNG")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "png"
        nameFilters: [qsTr("PNG images (*.png)")]
        onAccepted: page.exportPng(selectedFile)
    }
    FileDialog {
        id: csvDialog
        title: qsTr("Export visible range as CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV files (*.csv)")]
        onAccepted: page.exportCsv(selectedFile)
    }

    // ---------------------------------------------------------------- keys
    Shortcut { enabled: page.active; sequence: "F"; onActivated: Session.plot.follow() }
    Shortcut { enabled: page.active; sequences: ["+", "="]; onActivated: page.zoom(0.7) }
    Shortcut { enabled: page.active; sequence: "-"; onActivated: page.zoom(1.4) }
    Shortcut { enabled: page.active; sequence: "0"; onActivated: Session.plot.resetView() }
    Shortcut { enabled: page.active; sequence: "C"; onActivated: page.toggleCursors() }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        // =========================================================== picker
        Card {
            id: picker
            Layout.fillHeight: true
            Layout.preferredWidth: page.width >= 1100 ? 248 : 208
            elevation: 1
            visible: page.hasDb

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space.md
                spacing: Theme.space.sm
                Text {
                    text: qsTr("SIGNALS")
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.type.captionSpacing
                }
                TextField {
                    id: pickerSearch
                    Layout.fillWidth: true
                    iconName: "search"
                    placeholderText: qsTr("Find a signal")
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Click + or drag onto a lane")
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                    wrapMode: Text.WordWrap
                }
                ListView {
                    id: pickerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    model: {
                        const q = pickerSearch.text.trim().toLowerCase()
                        return q.length === 0 ? page.pickerItems
                             : page.pickerItems.filter(s => s.name.toLowerCase().indexOf(q) >= 0 || s.message.toLowerCase().indexOf(q) >= 0)
                    }
                    delegate: Item {
                        id: pickRow
                        required property var modelData
                        readonly property int signalId: modelData.signalId
                        readonly property bool plotted: Session.plotModel.seriesCount >= 0 && Session.plotModel.containsSignal(modelData.signalId)
                        width: pickerList.width
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radius.sm
                            color: pickHover.hovered ? Theme.hover : "transparent"
                        }
                        Rectangle {
                            x: 4
                            anchors.verticalCenter: parent.verticalCenter
                            width: 6; height: 6; radius: 3
                            color: pickRow.plotted ? Theme.accent : "transparent"
                            border.width: pickRow.plotted ? 0 : 1
                            border.color: Theme.borderStrong
                        }
                        Column {
                            x: 16
                            width: parent.width - 16 - 30
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                width: parent.width
                                text: pickRow.modelData.name
                                elide: Text.ElideRight
                                color: Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.small
                                font.weight: Font.Medium
                            }
                            Text {
                                width: parent.width
                                text: pickRow.modelData.message + (pickRow.modelData.unit ? "  ·  " + pickRow.modelData.unit : "")
                                elide: Text.ElideRight
                                color: Theme.textFaint
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.caption
                            }
                        }
                        IconButton {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            extent: 26
                            iconSize: 13
                            iconName: "plus"
                            tip: qsTr("Plot in a new lane")
                            onClicked: Session.plotModel.addSignal(pickRow.signalId, -1)
                        }
                        HoverHandler { id: pickHover; cursorShape: Qt.OpenHandCursor }
                        // Drag a ghost chip onto a lane.
                        Rectangle {
                            id: ghost
                            width: ghostText.implicitWidth + 20
                            height: 24
                            radius: 12
                            color: Theme.accent
                            visible: Drag.active
                            Drag.active: pickDrag.active
                            Drag.keys: ["can-signal"]
                            Drag.source: pickRow
                            Drag.hotSpot.x: 10
                            Drag.hotSpot.y: 12
                            Text {
                                id: ghostText
                                anchors.centerIn: parent
                                text: pickRow.modelData.name
                                color: Theme.textOnAccent
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.small
                                font.weight: Font.DemiBold
                            }
                        }
                        DragHandler {
                            id: pickDrag
                            target: ghost
                            onActiveChanged: {
                                page.dragging = active
                                if (!active) {
                                    ghost.Drag.drop()
                                    ghost.x = 0
                                    ghost.y = 0
                                }
                            }
                        }
                    }
                }
            }
        }

        // =========================================================== main
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space.md

            // ------------------------------------------------- toolbar
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space.sm
                enabled: page.hasDb

                ToolButton {
                    iconName: "live"
                    text: qsTr("Live")
                    checkable: true
                    checked: Session.plot.followLive
                    onClicked: Session.plot.follow()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Follow the newest data  (F)")
                    ToolTip.delay: 600
                }
                SegmentedToggle {
                    options: ["5 s", "10 s", "30 s", "60 s"]
                    readonly property var values: [5, 10, 30, 60]
                    currentIndex: Math.max(0, values.indexOf(Session.plot.windowSeconds))
                    onActivated: (i) => { Session.plot.windowSeconds = values[i]; Session.plot.follow() }
                    tip: qsTr("Time window while following live")
                }
                Divider { vertical: true; Layout.preferredHeight: 20; Layout.leftMargin: Theme.space.xs; Layout.rightMargin: Theme.space.xs }
                IconButton { iconName: "zoom-in"; tip: qsTr("Zoom in  (+, or mouse wheel)"); onClicked: page.zoom(0.7) }
                IconButton { iconName: "zoom-out"; tip: qsTr("Zoom out  (−)"); onClicked: page.zoom(1.4) }
                IconButton { iconName: "reset"; tip: qsTr("Reset view and follow live  (0, or double-click)"); onClicked: Session.plot.resetView() }
                Divider { vertical: true; Layout.preferredHeight: 20; Layout.leftMargin: Theme.space.xs; Layout.rightMargin: Theme.space.xs }
                ToolButton {
                    iconName: "cursors"
                    text: qsTr("Cursors")
                    checkable: true
                    checked: Session.plot.hasCursors
                    onClicked: page.toggleCursors()
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Measurement cursors A and B  (C)")
                    ToolTip.delay: 600
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: Session.plot.formatTime(Session.plot.viewStartUs) + "  –  " + Session.plot.formatTime(Session.plot.viewEndUs)
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    font.features: { "tnum": 1 }
                    visible: page.width > 1050
                }
                Button {
                    iconName: "plus"
                    text: qsTr("Lane")
                    onClicked: Session.plotModel.addLane()
                }
                IconButton { iconName: "image"; tip: qsTr("Export the plot as PNG"); enabled: lanes.count > 0; onClicked: pngDialog.open() }
                IconButton { iconName: "table"; tip: qsTr("Export the visible range as CSV"); enabled: lanes.count > 0; onClicked: csvDialog.open() }
            }

            // ------------------------------------------------- measurement bar
            Rectangle {
                id: measureBar
                Layout.fillWidth: true
                Layout.preferredHeight: Session.plot.hasCursors ? measureFlow.implicitHeight + 2 * Theme.space.sm : 0
                visible: Layout.preferredHeight > 1
                clip: true
                radius: Theme.radius.md
                color: Theme.surface
                border.width: 1
                border.color: Theme.border
                Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                readonly property var delta: (Session.plot.cursorAUs, Session.plot.cursorBUs, Session.plot.cursorDelta())
                Flow {
                    id: measureFlow
                    x: Theme.space.md
                    y: Theme.space.sm
                    width: parent.width - 2 * Theme.space.md
                    spacing: Theme.space.md
                    component Readout: Row {
                        property string label
                        property string value
                        property color tint: Theme.textMuted
                        spacing: 5
                        Text { text: parent.label; color: parent.tint; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
                        Text { text: parent.value; color: Theme.text; font.family: Theme.fontMono; font.pixelSize: Theme.type.small; anchors.verticalCenter: parent.verticalCenter }
                    }
                    Readout { label: "A"; value: Session.plot.formatTime(Session.plot.cursorAUs); tint: Theme.accentText }
                    Readout { label: "B"; value: Session.plot.formatTime(Session.plot.cursorBUs); tint: Theme.plotColor(1) }
                    Readout { label: "Δt"; value: measureBar.delta.dtText || "" }
                    Readout { label: "f"; value: measureBar.delta.freqText || "" }
                    Rectangle { width: 1; height: 16; color: Theme.border }
                    Repeater {
                        model: Session.plot.hasCursors ? page.allVisibleSeries().filter(e => !e.series.isEnum) : []
                        Row {
                            required property var modelData
                            spacing: 6
                            readonly property var va: Session.timeSeries.valueAt(modelData.series.signalId, Session.plot.cursorAUs)
                            readonly property var vb: Session.timeSeries.valueAt(modelData.series.signalId, Session.plot.cursorBUs)
                            Rectangle { width: 8; height: 8; radius: 4; color: Theme.plotColor(modelData.series.color); anchors.verticalCenter: parent.verticalCenter }
                            Text {
                                text: modelData.series.name
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.small
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "Δ " + (parent.va !== undefined && parent.vb !== undefined
                                      ? Session.plot.formatValue(parent.vb - parent.va, modelData.series.decimals)
                                        + (modelData.series.unit ? " " + modelData.series.unit : "")
                                      : "—")
                                color: Theme.text
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.type.small
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------- plot card
            Card {
                id: plotCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                elevation: 1
                readonly property real timeAxisHeight: 30

                // Lanes plus zoom / pan / hover handling over the plot region.
                Item {
                    id: lanesArea
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: timeAxis.top
                    anchors.margins: 1
                    anchors.bottomMargin: 0
                    readonly property bool overPlot: hover.hovered && hover.point.position.x >= page.axisWidth
                                                     && hover.point.position.x <= page.axisWidth + page.plotWidth

                    HoverHandler {
                        id: hover
                        enabled: lanes.count > 0 && !page.demoMode
                        onPointChanged: page.hoverX = lanesArea.overPlot ? point.position.x - page.axisWidth : -1
                        onHoveredChanged: if (!hovered) page.hoverX = -1
                    }
                    WheelHandler {
                        enabled: lanes.count > 0
                        onWheel: (e) => Session.plot.zoomAt(Math.max(0, Math.min(1, (e.x - page.axisWidth) / page.plotWidth)),
                                                            e.angleDelta.y > 0 ? 0.8 : 1.25)
                    }
                    DragHandler {
                        enabled: lanes.count > 0
                        target: null
                        property real last: 0
                        onActiveChanged: last = 0
                        onTranslationChanged: {
                            Session.plot.panByFraction(-(translation.x - last) / page.plotWidth)
                            last = translation.x
                        }
                    }
                    TapHandler {
                        enabled: lanes.count > 0
                        onDoubleTapped: Session.plot.resetView()
                    }

                    ListView {
                        id: lanes
                        anchors.fill: parent
                        interactive: false
                        clip: true
                        model: page.hasDb ? page.lanesModel : null
                        readonly property real available: height
                        delegate: PlotLane {
                            width: lanes.width
                            demoMode: page.demoMode
                            height: Math.max(84, lanes.available * weight / Math.max(0.0001, Session.plotModel.totalWeight))
                            axisWidth: page.axisWidth
                            plotWidth: page.plotWidth
                            timeTicks: page.timeTicks
                            isLast: index === lanes.count - 1
                            onResizeBy: (dy) => {
                                const total = Session.plotModel.totalWeight
                                const dw = dy / lanes.available * total
                                const next = lanes.itemAtIndex(index + 1)
                                if (!next)
                                    return
                                Session.plotModel.setWeight(index, weight + dw)
                                Session.plotModel.setWeight(index + 1, next.weight - dw)
                            }
                        }
                        add: Transition {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.slow; easing.type: Theme.motion.easing }
                        }
                        remove: Transition {
                            NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
                        }
                        displaced: Transition {
                            NumberAnimation { properties: "y"; duration: Theme.motion.slow; easing.type: Theme.motion.easing }
                        }
                        move: Transition {
                            NumberAnimation { properties: "y"; duration: Theme.motion.slow; easing.type: Theme.motion.easing }
                        }
                    }
                }

                // Shared time axis.
                Item {
                    id: timeAxis
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: plotCard.timeAxisHeight
                    visible: lanes.count > 0
                    Rectangle { anchors.left: parent.left; anchors.right: parent.right; height: 1; color: Theme.border }
                    Repeater {
                        model: page.timeTicks
                        Text {
                            required property var modelData
                            x: page.axisWidth + modelData.x - width / 2
                            y: 8
                            visible: modelData.x >= 0 && modelData.x <= page.plotWidth
                            text: modelData.label
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.type.caption
                            font.features: { "tnum": 1 }
                        }
                    }
                }

                // ---- overlay: crosshair, cursors, readouts ----
                Item {
                    id: overlay
                    x: page.axisWidth + 1
                    y: 1
                    width: page.plotWidth
                    height: lanesArea.height
                    visible: lanes.count > 0

                    // Crosshair
                    Rectangle {
                        visible: page.hoverX >= 0
                        x: page.hoverX
                        width: 1
                        height: parent.height
                        color: Theme.alpha(Theme.text, 0.35)
                    }

                    // Measurement cursors A and B
                    Repeater {
                        model: Session.plot.hasCursors ? [0, 1] : []
                        Item {
                            id: cur
                            required property int modelData
                            readonly property real timeUs: modelData === 0 ? Session.plot.cursorAUs : Session.plot.cursorBUs
                            readonly property color tint: modelData === 0 ? Theme.accent : Theme.plotColor(1)
                            // Mention the view range so the binding follows scrolling and zooming.
                            x: (Session.plot.viewStartUs, Session.plot.viewEndUs, Session.plot.xOfTime(timeUs, page.plotWidth)) - width / 2
                            width: 14
                            height: parent.height
                            visible: x + width / 2 >= 0 && x + width / 2 <= page.plotWidth
                            Rectangle {
                                anchors.horizontalCenter: parent.horizontalCenter
                                width: 1.5
                                height: parent.height
                                color: cur.tint
                            }
                            Rectangle { // handle with the cursor letter
                                anchors.horizontalCenter: parent.horizontalCenter
                                y: 2
                                width: 20
                                height: 18
                                radius: 5
                                color: cur.tint
                                Text {
                                    anchors.centerIn: parent
                                    text: cur.modelData === 0 ? "A" : "B"
                                    color: Theme.textOnAccent
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.type.caption
                                    font.weight: Font.Bold
                                }
                            }
                            HoverHandler { cursorShape: Qt.SizeHorCursor }
                            DragHandler {
                                target: null
                                yAxis.enabled: false
                                property real startX: 0
                                onActiveChanged: if (active) startX = cur.x + cur.width / 2
                                onTranslationChanged: Session.plot.setCursor(cur.modelData, Session.plot.timeAtX(startX + translation.x, page.plotWidth))
                            }
                        }
                    }

                    // Crosshair tooltip
                    Rectangle {
                        id: tip
                        visible: page.hoverX >= 0
                        readonly property real timeUs: (Session.plot.viewStartUs, Session.plot.viewEndUs,
                                                        Session.plot.timeAtX(page.hoverX, page.plotWidth))
                        x: page.hoverX + 14 + width > parent.width ? page.hoverX - 14 - width : page.hoverX + 14
                        y: hover.hovered ? Math.min(parent.height - height - 8, Math.max(8, hover.point.position.y - height / 2))
                                         : Math.max(8, parent.height * 0.3 - height / 2)
                        width: tipCol.implicitWidth + 2 * Theme.space.md
                        height: tipCol.implicitHeight + 2 * Theme.space.sm
                        radius: Theme.radius.md
                        color: Theme.alpha(Theme.popup, 0.96)
                        border.width: 1
                        border.color: Theme.borderStrong
                        Column {
                            id: tipCol
                            x: Theme.space.md
                            y: Theme.space.sm
                            spacing: 3
                            Text { text: Session.plot.formatTime(tip.timeUs); color: Theme.text; font.family: Theme.fontMono; font.pixelSize: Theme.type.small; font.weight: Font.DemiBold }
                            Repeater {
                                model: tip.visible ? page.allVisibleSeries() : []
                                Row {
                                    required property var modelData
                                    spacing: 6
                                    Rectangle { width: 8; height: 8; radius: 4; color: Theme.plotColor(modelData.series.color); anchors.verticalCenter: parent.verticalCenter }
                                    Text { text: modelData.series.name; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; width: 120; elide: Text.ElideRight }
                                    Text { text: modelData.lane.valueText(modelData.series, tip.timeUs); color: Theme.text; font.family: Theme.fontMono; font.pixelSize: Theme.type.small }
                                }
                            }
                        }
                    }
                }

                // Follow-live / paused pill
                FloatingPill {
                    anchors.horizontalCenter: overlay.horizontalCenter
                    anchors.bottom: timeAxis.top
                    anchors.bottomMargin: Theme.space.md
                    visible: lanes.count > 0 && opacity > 0.01
                    detached: !Session.plot.followLive
                    detachedText: qsTr("Follow live")
                    detachedIcon: "live"
                    onJumpRequested: Session.plot.follow()
                }

                // ---- empty states ----
                Column {
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: Theme.space.lg
                    visible: !page.hasDb
                    EmptyState {
                        width: parent.width
                        iconName: "plot"
                        title: qsTr("Plots need a signal database")
                        message: qsTr("Load the demo database to plot decoded signals over time.")
                    }
                    Button {
                        anchors.horizontalCenter: parent.horizontalCenter
                        variant: "primary"
                        iconName: "database"
                        text: qsTr("Load demo database")
                        onClicked: Session.database.loadDemo()
                    }
                }
                EmptyState {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: page.hasDb && lanes.count === 0
                    iconName: "plot"
                    title: qsTr("No signals plotted yet")
                    message: qsTr("Click + next to a signal on the left, drag it here, or use Plot in the Signals tab. Several signals can share a lane.")
                }
                DropArea {
                    anchors.fill: parent
                    keys: ["can-signal"]
                    enabled: lanes.count === 0
                    onDropped: (drop) => { Session.plotModel.addSignal(drop.source.signalId, -1); drop.accept() }
                }
            }
            // New-lane drop zone while dragging.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: page.dragging && lanes.count > 0 ? 40 : 0
                visible: Layout.preferredHeight > 1
                Behavior on Layout.preferredHeight { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                radius: Theme.radius.md
                color: newLaneDrop.containsDrag ? Theme.accentSubtle : "transparent"
                border.width: 1.5
                border.color: newLaneDrop.containsDrag ? Theme.accent : Theme.borderStrong
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Drop here for a new lane")
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                }
                DropArea {
                    id: newLaneDrop
                    anchors.fill: parent
                    keys: ["can-signal"]
                    onDropped: (drop) => { Session.plotModel.addSignal(drop.source.signalId, -1); drop.accept() }
                }
            }
        }
    }

    // Toast for exports
    Rectangle {
        id: toast
        property string text: ""
        property string tone: "ok"
        property bool shown: false
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.xl
        width: toastText.implicitWidth + 2 * Theme.space.lg
        height: 34
        radius: 17
        color: Theme.popup
        border.width: 1
        border.color: Theme.alpha(Theme.toneColor(tone), 0.5)
        opacity: shown ? 1 : 0
        visible: opacity > 0.01
        Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
        Text {
            id: toastText
            anchors.centerIn: parent
            text: toast.text
            color: toast.tone === "error" ? Theme.error : Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
        }
        Timer { id: toastTimer; interval: 2800; onTriggered: toast.shown = false }
    }
}
