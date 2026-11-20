import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import CanUi
import CanAnalyzer
import CanAnalyzer.Core

// One plot lane: value axis, faint grid, the scene-graph canvas, an
// interactive legend, threshold lines and lane controls. All lanes share the
// time axis owned by the page (Session.plot).
Item {
    id: lane
    required property int index
    required property var series
    required property real weight
    required property bool yLocked
    required property real yMin
    required property real yMax
    required property var thresholds

    property real axisWidth: 64
    property real plotWidth: width - axisWidth - 52
    property bool isLast: false
    property var timeTicks: []        // shared time-axis ticks from the page
    signal resizeBy(real dy)

    readonly property real topPad: 8
    readonly property real bottomPad: 6
    readonly property real plotHeight: height - topPad - bottomPad
    readonly property var store: Session.timeSeries
    readonly property var visibleSeries: series.filter(s => s.visible)

    // ---- y ranges ----------------------------------------------------------------
    // Series are grouped by unit. The first unit uses the left axis (and the
    // grid); a second unit gets its own scale on the right axis; any further
    // units are scaled to the lane on their own. Enumerations form their own
    // group with a range that covers all their states.
    function unitKey(s) { return s.isEnum ? "#enum" + s.signalId : s.unit }
    readonly property var unitKeys: {
        const keys = []
        for (const s of visibleSeries)
            if (keys.indexOf(unitKey(s)) < 0)
                keys.push(unitKey(s))
        return keys
    }
    readonly property string leftKey: unitKeys.length > 0 ? unitKeys[0] : ""
    readonly property string rightKey: unitKeys.length > 1 ? unitKeys[1] : ""
    readonly property var leftSeries: visibleSeries.filter(s => unitKey(s) === leftKey)
    readonly property var rightSeries: visibleSeries.filter(s => unitKey(s) === rightKey)

    function groupRange(key) {
        const members = visibleSeries.filter(s => unitKey(s) === key)
        let lo = 1e300, hi = -1e300
        for (const r of canvas.seriesRanges)
            if (members.some(m => m.signalId === r.signalId)) { lo = Math.min(lo, r.min); hi = Math.max(hi, r.max) }
        const en = members.find(m => m.isEnum)
        if (en) {
            for (const l of en.valueLabels) { lo = Math.min(lo, l.value); hi = Math.max(hi, l.value) }
            return lo <= hi ? [lo - 0.5, hi + 0.5] : [0, 1]
        }
        return lo <= hi ? Session.plot.paddedRange(lo, hi) : [0, 1]
    }
    readonly property var leftTarget: yLocked ? [yMin, yMax] : (canvas.seriesRanges, groupRange(leftKey))
    readonly property var rightTarget: rightKey.length > 0 ? (canvas.seriesRanges, groupRange(rightKey)) : [0, 1]

    // Displayed ranges ease towards their targets every frame. (A Behavior
    // does not work here: the target changes every frame while following
    // live, and a Behavior restarted every frame never advances.)
    property real shownMin: 0
    property real shownMax: 1
    property real rightMin: 0
    property real rightMax: 1
    property bool rangeInitialised: false
    FrameAnimation {
        running: lane.visible
        onTriggered: {
            const lt = lane.leftTarget, rt = lane.rightTarget
            if (!lane.rangeInitialised || lane.yLocked) {
                lane.shownMin = lt[0]; lane.shownMax = lt[1]
                lane.rightMin = rt[0]; lane.rightMax = rt[1]
                lane.rangeInitialised = canvas.hasData || lane.yLocked
                return
            }
            const k = 1 - Math.exp(-frameTime / 0.09)
            lane.shownMin += (lt[0] - lane.shownMin) * k
            lane.shownMax += (lt[1] - lane.shownMax) * k
            lane.rightMin += (rt[0] - lane.rightMin) * k
            lane.rightMax += (rt[1] - lane.rightMax) * k
        }
    }

    // Per-series ranges for everything not on the left axis.
    readonly property var rangesById: {
        const m = {}
        for (const s of visibleSeries) {
            const k = unitKey(s)
            if (k === leftKey)
                continue
            m[String(s.signalId)] = k === rightKey ? [rightMin, rightMax] : (canvas.seriesRanges, groupRange(k))
        }
        return m
    }

    function yOf(v) { return topPad + (shownMax - v) / (shownMax - shownMin) * plotHeight }
    function valueAtY(y) { return shownMax - (y - topPad) / plotHeight * (shownMax - shownMin) }

    function valueText(s, timeUs) {
        const v = lane.store.valueAt(s.signalId, timeUs)
        if (v === undefined || v === null)
            return "—"
        if (s.isEnum) {
            const l = s.valueLabels.find(x => Math.abs(x.value - v) < 1e-9)
            return l ? l.text : Session.plot.formatValue(v, s.decimals)
        }
        return Session.plot.formatValue(v, s.decimals) + (s.unit ? " " + s.unit : "")
    }

    // Ticks for a group: enumerations get every integer state (with its label
    // when the value table has one), numbers get "nice" ticks.
    function ticksFor(series, lo, hi) {
        const en = series.find(s => s.isEnum)
        if (en) {
            const out = []
            const first = Math.ceil(lo), last = Math.floor(hi)
            const step = last - first > 12 ? Math.ceil((last - first) / 12) : 1
            for (let v = first; v <= last; v += step) {
                const l = en.valueLabels.find(x => Math.abs(x.value - v) < 1e-9)
                out.push({ y: (hi - v) / (hi - lo) * plotHeight, label: l ? l.text : String(v) })
            }
            return out
        }
        return Session.plot.valueTicks(lo, hi, plotHeight)
    }
    // Labels within 14 px of the bottom edge would collide with the unit label.
    readonly property var ticks: ticksFor(leftSeries, shownMin, shownMax).filter(t => t.y <= plotHeight - 4 && t.y >= -2)
    readonly property var rightTicks: rightKey.length > 0
        ? ticksFor(rightSeries, rightMin, rightMax).filter(t => t.y <= plotHeight - 4 && t.y >= -2) : []

    // ---- axes and grid ---------------------------------------------------------------
    Repeater {
        model: lane.ticks
        Item {
            required property var modelData
            y: lane.topPad + modelData.y
            width: lane.width
            Rectangle {
                x: lane.axisWidth
                width: lane.plotWidth
                height: 1
                color: Theme.gridLine
            }
            Text {
                width: lane.axisWidth - 10
                y: -height / 2
                horizontalAlignment: Text.AlignRight
                visible: modelData.y < lane.plotHeight - 14 || lane.leftSeries.length === 0 || lane.leftSeries[0].isEnum
                text: modelData.label
                color: lane.rightKey.length > 0 && lane.leftSeries.length > 0
                       ? Theme.alpha(Theme.plotColor(lane.leftSeries[0].color), 0.95) : Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
                font.features: { "tnum": 1 }
                elide: Text.ElideLeft
            }
        }
    }
    Repeater { // right axis: labels only, tinted like its first series
        model: lane.rightTicks
        Text {
            required property var modelData
            x: lane.axisWidth + lane.plotWidth + 8
            y: lane.topPad + modelData.y - height / 2
            width: lane.width - x - 4
            visible: modelData.y < lane.plotHeight - 14 || lane.rightSeries[0].isEnum
            text: modelData.label
            elide: Text.ElideRight
            color: lane.rightSeries.length > 0 ? Theme.alpha(Theme.plotColor(lane.rightSeries[0].color), 0.95) : Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.caption
            font.features: { "tnum": 1 }
        }
    }
    // Units of both axes, at the bottom corners.
    Text {
        visible: text.length > 0
        x: 6
        y: lane.height - height - 4
        text: lane.leftSeries.length > 0 && !lane.leftSeries[0].isEnum ? lane.leftSeries[0].unit : ""
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.caption - 1
        font.weight: Font.DemiBold
    }
    Text {
        visible: text.length > 0
        x: lane.axisWidth + lane.plotWidth + 8
        y: lane.height - height - 4
        text: lane.rightSeries.length > 0 && !lane.rightSeries[0].isEnum ? lane.rightSeries[0].unit : ""
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.caption - 1
        font.weight: Font.DemiBold
    }
    // Vertical time grid (shared ticks).
    Repeater {
        model: lane.timeTicks
        Rectangle {
            required property var modelData
            x: lane.axisWidth + modelData.x
            y: lane.topPad
            width: 1
            height: lane.plotHeight
            color: Theme.gridLine
            visible: modelData.x >= 0 && modelData.x <= lane.plotWidth
        }
    }

    // ---- data ----------------------------------------------------------------
    PlotCanvas {
        id: canvas
        x: lane.axisWidth
        y: lane.topPad
        width: lane.plotWidth
        height: lane.plotHeight
        store: Session.timeSeries
        series: lane.series
        colors: Theme.plotPalette
        viewStartUs: Session.plot.viewStartUs
        viewEndUs: Session.plot.viewEndUs
        yMin: lane.shownMin
        yMax: lane.shownMax
        rangesById: lane.rangesById
        revision: Session.timeSeries.revision
        lineWidth: 1.75
        fillOpacity: 0.16 + 0.06 * Theme.t
        bandOpacity: 0.20 + 0.06 * Theme.t
    }

    // ---- thresholds ------------------------------------------------------------
    Repeater {
        model: lane.thresholds
        Item {
            id: th
            required property var modelData
            required property int index
            x: lane.axisWidth
            width: lane.plotWidth
            height: 14
            readonly property bool above: modelData > lane.shownMax
            readonly property bool below: modelData < lane.shownMin
            y: Math.max(lane.topPad, Math.min(lane.topPad + lane.plotHeight, lane.yOf(modelData))) - height / 2
            Shape {
                visible: !th.above && !th.below
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 1
                ShapePath {
                    strokeColor: Theme.warning
                    strokeWidth: 1.25
                    strokeStyle: ShapePath.DashLine
                    dashPattern: [4, 3]
                    startX: 0; startY: 0.5
                    PathLine { x: th.width; y: 0.5 }
                }
            }
            Rectangle { // value badge, also the drag handle
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 18
                width: thText.implicitWidth + 14
                radius: 9
                color: Theme.toneFill("warning")
                border.width: 1
                border.color: Theme.alpha(Theme.warning, 0.45)
                Text {
                    id: thText
                    anchors.centerIn: parent
                    text: (th.above ? "▲ " : th.below ? "▼ " : "") + Session.plot.formatValue(th.modelData, 1)
                    color: Theme.warning
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
                HoverHandler { cursorShape: Qt.SizeVerCursor }
                DragHandler {
                    target: null
                    yAxis.enabled: true
                    xAxis.enabled: false
                    property real startValue: 0
                    onActiveChanged: if (active) startValue = th.modelData
                    onTranslationChanged: {
                        const v = lane.valueAtY(lane.yOf(startValue) + translation.y)
                        Session.plotModel.setThreshold(lane.index, th.index, v)
                    }
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: Session.plotModel.removeThreshold(lane.index, th.index)
                }
            }
        }
    }

    // ---- legend ----------------------------------------------------------------
    Flow {
        id: legend
        x: lane.axisWidth + 8
        y: lane.topPad + 4
        width: lane.plotWidth - 180
        spacing: 6
        Repeater {
            model: lane.series
            Rectangle {
                id: chip
                required property var modelData
                readonly property color seriesColor: Theme.plotColor(modelData.color)
                height: 22
                width: chipRow.implicitWidth + 16
                radius: 11
                color: Theme.alpha(Theme.surface, 0.86)
                border.width: 1
                border.color: chipHover.hovered ? Theme.borderStrong : Theme.border
                opacity: modelData.visible ? 1 : 0.5
                Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }

                Row {
                    id: chipRow
                    anchors.centerIn: parent
                    spacing: 6
                    // Line sample: click toggles solid / dashed (second visual cue).
                    Item {
                        width: 16
                        height: 12
                        anchors.verticalCenter: parent.verticalCenter
                        Row {
                            anchors.centerIn: parent
                            spacing: chip.modelData.dashed ? 2 : 0
                            Repeater {
                                model: chip.modelData.dashed ? 3 : 1
                                Rectangle {
                                    width: chip.modelData.dashed ? 4 : 16
                                    height: 2.5
                                    radius: 1.25
                                    color: chip.seriesColor
                                }
                            }
                        }
                        TapHandler { onTapped: Session.plotModel.setDashed(lane.index, chip.modelData.signalId, !chip.modelData.dashed) }
                    }
                    Text {
                        text: chip.modelData.name
                        color: Theme.text
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.type.small
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                        font.strikeout: !chip.modelData.visible
                    }
                    Text {
                        // current value at the right edge of the view
                        text: Session.timeSeries.revision >= 0 ? lane.valueText(chip.modelData, Session.plot.viewEndUs) : ""
                        color: chip.modelData.visible ? Theme.textMuted : Theme.textFaint
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.type.small
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    IconButton {
                        visible: chipHover.hovered
                        width: visible ? 16 : 0
                        extent: 16
                        iconSize: 10
                        iconName: "close"
                        tip: qsTr("Remove from plot")
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: Session.plotModel.removeSignal(lane.index, chip.modelData.signalId)
                    }
                }
                HoverHandler { id: chipHover }
                TapHandler { onTapped: Session.plotModel.toggleVisible(lane.index, chip.modelData.signalId) }
            }
        }
    }

    // ---- lane controls (top right) ---------------------------------------------------
    Row {
        id: controls
        x: lane.axisWidth + lane.plotWidth - width - 6
        y: lane.topPad + 2
        spacing: 2
        opacity: laneHover.hovered || lane.yLocked ? 1 : 0.0
        Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
        IconButton {
            extent: 24; iconSize: 13
            iconName: lane.yLocked ? "lock" : "unlock"
            checked: lane.yLocked
            tip: lane.yLocked ? qsTr("Y axis locked; click to autoscale") : qsTr("Lock the Y axis at the current range")
            onClicked: Session.plotModel.setYLock(lane.index, !lane.yLocked, lane.shownMin, lane.shownMax)
        }
        IconButton {
            extent: 24; iconSize: 13
            iconName: "threshold"
            tip: qsTr("Add a threshold line (drag it; right-click removes)")
            onClicked: Session.plotModel.addThreshold(lane.index, (lane.shownMin + lane.shownMax) / 2)
        }
        IconButton {
            extent: 24; iconSize: 13
            iconName: "chevron-up"
            enabled: lane.index > 0
            tip: qsTr("Move lane up")
            onClicked: Session.plotModel.moveLane(lane.index, lane.index - 1)
        }
        IconButton {
            extent: 24; iconSize: 13
            iconName: "chevron-down"
            enabled: !lane.isLast
            tip: qsTr("Move lane down")
            onClicked: Session.plotModel.moveLane(lane.index, lane.index + 1)
        }
        IconButton {
            extent: 24; iconSize: 12
            iconName: "close"
            tip: qsTr("Remove lane")
            onClicked: Session.plotModel.removeLane(lane.index)
        }
    }
    property bool demoMode: false
    HoverHandler { id: laneHover; enabled: !lane.demoMode }

    // ---- empty lane / drop target ----------------------------------------------------
    Text {
        anchors.centerIn: canvas
        visible: lane.series.length === 0
        text: qsTr("Drop a signal here")
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
    }
    DropArea {
        anchors.fill: parent
        keys: ["can-signal"]
        onDropped: (drop) => {
            Session.plotModel.addSignal(drop.source.signalId, lane.index)
            drop.accept()
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: Theme.radius.md
            color: Theme.accentSubtle
            border.width: 1.5
            border.color: Theme.accent
            visible: parent.containsDrag
        }
    }

    // ---- separator + resize handle ----------------------------------------------------
    Rectangle {
        visible: !lane.isLast
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }
    Item {
        visible: !lane.isLast
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -4
        height: 8
        z: 5
        HoverHandler { cursorShape: Qt.SizeVerCursor }
        DragHandler {
            target: null
            xAxis.enabled: false
            property real last: 0
            onActiveChanged: last = 0
            onTranslationChanged: {
                lane.resizeBy(translation.y - last)
                last = translation.y
            }
        }
    }
}
