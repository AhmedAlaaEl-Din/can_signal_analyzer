import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Log summary: key figures and a per-ID table, exportable as CSV.
Popup {
    id: dialog
    property var summary: ({})
    readonly property bool hasData: summary && summary.ok === true
    readonly property var rows: hasData ? summary.rows : []
    readonly property real maxShare: rows.reduce((m, r) => Math.max(m, r.share), 0.0001)
    signal exportRequested()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(900, parent ? parent.width - 64 : 900)
    height: Math.min(640, parent ? parent.height - 64 : 640)
    modal: true
    dim: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.28 + 0.17 * Theme.t) }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.fast }
    }
    background: Rectangle {
        radius: Theme.radius.lg
        color: Theme.popup
        border.width: 1
        border.color: Theme.borderStrong
    }

    component Stat: Rectangle {
        property string label
        property string value
        property string sub: ""
        property string tone: "neutral"
        Layout.fillWidth: true
        implicitHeight: 70
        radius: Theme.radius.md
        color: Theme.alpha(Theme.text, 0.035)
        border.width: 1
        border.color: Theme.border
        Column {
            x: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3
            Text {
                text: parent.parent.label
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
                font.weight: Font.DemiBold
                font.letterSpacing: Theme.type.captionSpacing
            }
            Text {
                text: parent.parent.value
                color: parent.parent.tone === "neutral" ? Theme.text : Theme.toneColor(parent.parent.tone)
                font.family: Theme.fontDisplay
                font.pixelSize: 20
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
            Text {
                visible: text.length > 0
                text: parent.parent.sub
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
                font.features: { "tnum": 1 }
            }
        }
    }

    readonly property var columns: [
        { title: qsTr("ID"), w: 86, align: Text.AlignLeft },
        { title: qsTr("MESSAGE"), w: 0, align: Text.AlignLeft },
        { title: qsTr("FRAMES"), w: 84, align: Text.AlignRight },
        { title: qsTr("SHARE"), w: 120, align: Text.AlignLeft },
        { title: qsTr("RATE"), w: 72, align: Text.AlignRight },
        { title: qsTr("PERIOD (MIN–MAX)"), w: 150, align: Text.AlignRight },
        { title: qsTr("DLC"), w: 40, align: Text.AlignRight },
        { title: qsTr("LOAD"), w: 64, align: Text.AlignRight },
    ]
    readonly property real fixedWidth: columns.reduce((s, c) => s + c.w, 0) + (columns.length - 1) * 14
    function colWidth(i, total) { return columns[i].w > 0 ? columns[i].w : Math.max(80, total - fixedWidth) }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- header
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.lg
            Layout.bottomMargin: Theme.space.md
            spacing: Theme.space.md
            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: Theme.radius.md
                color: Theme.accentSubtle
                Icon { anchors.centerIn: parent; name: "bar-chart"; size: 18; color: Theme.accentText }
            }
            ColumnLayout {
                spacing: 1
                Layout.fillWidth: true
                Text {
                    text: qsTr("Log summary")
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.type.title + 2
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    text: dialog.hasData
                          ? [dialog.summary.fileName, Fmt.bytes(dialog.summary.fileSize),
                             dialog.summary.startTime ? qsTr("recorded %1").arg(dialog.summary.startTime) : ""]
                                .filter(x => x).join("  ·  ")
                          : (dialog.summary.error || qsTr("Computing…"))
                    color: dialog.summary.error ? Theme.error : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                }
            }
            IconButton { iconName: "close"; tip: qsTr("Close  (Esc)"); onClicked: dialog.close() }
        }

        // ---- key figures
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.lg
            spacing: Theme.space.sm
            visible: dialog.hasData
            Stat {
                label: qsTr("DURATION")
                value: dialog.hasData ? Fmt.clock(dialog.summary.durationUs) : ""
                sub: dialog.hasData ? qsTr("%1 s → %2 s").arg((dialog.summary.firstUs / 1e6).toFixed(3)).arg((dialog.summary.lastUs / 1e6).toFixed(3)) : ""
            }
            Stat {
                label: qsTr("FRAMES")
                value: dialog.hasData ? Fmt.grouped(dialog.summary.frames) : ""
                sub: dialog.hasData ? qsTr("%1 fr/s average").arg(Fmt.grouped(dialog.summary.averageRate)) : ""
            }
            Stat {
                label: qsTr("MESSAGE IDS")
                value: dialog.hasData ? dialog.summary.idCount : ""
                sub: dialog.hasData ? qsTr("%1 kbit/s bus").arg(dialog.summary.bitrate / 1000) : ""
            }
            Stat {
                label: qsTr("BUS LOAD")
                value: dialog.hasData ? Fmt.percent(dialog.summary.averageLoad) : ""
                sub: dialog.hasData ? qsTr("peak %1 (1 s window)").arg(Fmt.percent(dialog.summary.peakLoad)) : ""
                tone: dialog.hasData && dialog.summary.peakLoad >= 0.8 ? "error"
                      : dialog.hasData && dialog.summary.peakLoad >= 0.5 ? "warning" : "neutral"
            }
        }
        Text {
            visible: dialog.hasData && dialog.summary.warning.length > 0
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.lg
            Layout.topMargin: Theme.space.sm
            text: dialog.hasData ? dialog.summary.warning : ""
            wrapMode: Text.WordWrap
            color: Theme.warning
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
        }

        // ---- per-ID table
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.space.lg
            Layout.bottomMargin: Theme.space.md
            radius: Theme.radius.md
            color: "transparent"
            border.width: 1
            border.color: Theme.border
            clip: true

            Rectangle {
                id: head
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 1
                y: 1
                height: Theme.size.header
                color: Theme.alpha(Theme.text, 0.03)
                Row {
                    x: Theme.space.md
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 14
                    Repeater {
                        model: dialog.columns
                        Text {
                            required property var modelData
                            required property int index
                            width: dialog.colWidth(index, head.width - 2 * Theme.space.md)
                            horizontalAlignment: modelData.align
                            text: modelData.title
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.type.caption
                            font.weight: Font.DemiBold
                            font.letterSpacing: Theme.type.captionSpacing
                            elide: Text.ElideRight
                        }
                    }
                }
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
            }

            ListView {
                id: table
                anchors.top: head.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 1
                anchors.topMargin: 0
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: dialog.rows
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: Rectangle {
                    id: rowItem
                    required property var modelData
                    required property int index
                    readonly property real inner: table.width - 2 * Theme.space.md
                    width: table.width
                    height: Theme.size.row + 4
                    color: index % 2 === 1 ? Theme.zebra : "transparent"
                    Row {
                        x: Theme.space.md
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 14
                        Text { width: dialog.colWidth(0, rowItem.inner); text: rowItem.modelData.id; color: Theme.text; font.family: Theme.fontMono; font.pixelSize: Theme.type.small }
                        Text { width: dialog.colWidth(1, rowItem.inner); text: rowItem.modelData.name || "—"; color: rowItem.modelData.name ? Theme.text : Theme.textFaint; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; elide: Text.ElideRight }
                        Text { width: dialog.colWidth(2, rowItem.inner); horizontalAlignment: Text.AlignRight; text: Fmt.grouped(rowItem.modelData.count); color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; font.features: { "tnum": 1 } }
                        Item {
                            width: dialog.colWidth(3, rowItem.inner)
                            height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(2, (parent.width - 44) * rowItem.modelData.share / dialog.maxShare)
                                height: 6
                                radius: 3
                                color: Theme.alpha(Theme.accent, 0.75)
                            }
                            Text {
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: (rowItem.modelData.share * 100).toFixed(1) + "%"
                                color: Theme.textMuted
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.caption
                                font.features: { "tnum": 1 }
                            }
                        }
                        Text { width: dialog.colWidth(4, rowItem.inner); horizontalAlignment: Text.AlignRight; text: rowItem.modelData.rate.toFixed(1) + " Hz"; color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; font.features: { "tnum": 1 } }
                        Text {
                            width: dialog.colWidth(5, rowItem.inner)
                            horizontalAlignment: Text.AlignRight
                            text: rowItem.modelData.periodMs.toFixed(1) + " ms  (" + rowItem.modelData.minPeriodMs.toFixed(1) + "–" + rowItem.modelData.maxPeriodMs.toFixed(1) + ")"
                            color: Theme.text
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.type.small
                            font.features: { "tnum": 1 }
                        }
                        Text { width: dialog.colWidth(6, rowItem.inner); horizontalAlignment: Text.AlignRight; text: rowItem.modelData.dlc; color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.type.small }
                        Text { width: dialog.colWidth(7, rowItem.inner); horizontalAlignment: Text.AlignRight; text: Fmt.percent(rowItem.modelData.load); color: Theme.text; font.family: Theme.fontSans; font.pixelSize: Theme.type.small; font.features: { "tnum": 1 } }
                    }
                }
            }
            BusyIndicatorText {
                anchors.centerIn: parent
                visible: Session.summaryBusy
            }
        }

        // ---- footer
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            spacing: Theme.space.sm
            Text {
                Layout.fillWidth: true
                text: qsTr("Periods are measured between consecutive frames of the same ID. Bus load uses the nominal frame length without stuff bits.")
                wrapMode: Text.WordWrap
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
            }
            Button {
                variant: "secondary"
                iconName: "table"
                text: qsTr("Export CSV…")
                enabled: dialog.hasData
                onClicked: dialog.exportRequested()
            }
            Button {
                variant: "primary"
                text: qsTr("Close")
                onClicked: dialog.close()
            }
        }
    }

    component BusyIndicatorText: Text {
        text: qsTr("Reading the log…")
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
    }
}
