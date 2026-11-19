import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Transport for replay mode: play/pause, jump to start/end, single step,
// position and duration, seek bar with markers, speed, loop, markers,
// summary and CSV export.
Rectangle {
    id: bar
    readonly property var replay: Session.replay
    property alias seekBar: seek
    signal summaryRequested()
    signal exportRequested()

    implicitHeight: 52
    radius: Theme.radius.lg
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    readonly property var speeds: [0.25, 0.5, 1, 2, 4, 8, 0]
    function speedLabel(v) { return v === 0 ? qsTr("Max") : (v < 1 ? v.toString().replace(/^0/, "") : v) + "×" }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.sm
        anchors.rightMargin: Theme.space.sm
        spacing: Theme.space.xs

        IconButton {
            iconName: "skip-start"
            tip: qsTr("Jump to the start  (Ctrl+Home)")
            onClicked: Session.seekReplay(bar.replay.startUs)
        }
        // Play / pause: the primary action of the bar.
        RoundButton {
            id: playButton
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            hoverEnabled: true
            onClicked: Session.togglePause()
            background: Rectangle {
                radius: width / 2
                color: playButton.down ? Theme.accentPressed : playButton.hovered ? Theme.accentHover : Theme.accent
            }
            contentItem: Item {
                Icon {
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: bar.replay.playing ? 0 : 1
                    name: bar.replay.playing ? "pause" : "play"
                    size: 16
                    color: Theme.textOnAccent
                }
            }
            ToolTip.visible: hovered
            ToolTip.text: bar.replay.playing ? qsTr("Pause  (Space)") : qsTr("Play  (Space)")
            ToolTip.delay: 600
        }
        IconButton {
            iconName: "step"
            tip: qsTr("Step one frame  (.)")
            onClicked: bar.replay.stepFrame()
        }
        IconButton {
            iconName: "skip-end"
            tip: qsTr("Jump to the end  (Ctrl+End)")
            onClicked: Session.seekReplay(bar.replay.endUs)
        }

        // Position / duration
        Text {
            Layout.leftMargin: Theme.space.sm
            text: Fmt.clock((seek.dragging ? seek.dragUs : bar.replay.positionUs) - bar.replay.startUs)
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
        }
        Text {
            text: "/ " + Fmt.clock(bar.replay.durationUs)
            color: Theme.textFaint
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.small
            Layout.rightMargin: Theme.space.sm
        }

        SeekBar {
            id: seek
            Layout.fillWidth: true
            Layout.minimumWidth: 120
        }

        ComboBox {
            id: speedBox
            prefix: qsTr("Speed")
            Layout.preferredWidth: 122
            Layout.leftMargin: Theme.space.sm
            model: bar.speeds.map(v => bar.speedLabel(v))
            currentIndex: Math.max(0, bar.speeds.indexOf(bar.replay.speed))
            onActivated: (i) => bar.replay.speed = bar.speeds[i]
            ToolTip.visible: hovered && !popup.visible
            ToolTip.text: qsTr("Playback speed; Max plays as fast as the views keep up")
            ToolTip.delay: 600
        }
        IconButton {
            iconName: "repeat"
            checkable: true
            checked: bar.replay.loop
            iconColor: checked ? Theme.accentText : Theme.textMuted
            tip: bar.replay.loop ? qsTr("Loop is on") : qsTr("Loop at the end")
            onClicked: bar.replay.loop = !bar.replay.loop
        }

        Divider { vertical: true; Layout.preferredHeight: 22; Layout.leftMargin: Theme.space.xs; Layout.rightMargin: Theme.space.xs }

        IconButton {
            iconName: "flag"
            tip: qsTr("Add a marker at the current time  (M)")
            onClicked: Session.addMarker("")
        }
        IconButton {
            iconName: "chevron-left"
            enabled: Session.markers.count > 0
            tip: qsTr("Previous marker  ([)")
            onClicked: Session.jumpToMarker(false)
        }
        IconButton {
            iconName: "chevron-right"
            enabled: Session.markers.count > 0
            tip: qsTr("Next marker  (])")
            onClicked: Session.jumpToMarker(true)
        }

        Divider { vertical: true; Layout.preferredHeight: 22; Layout.leftMargin: Theme.space.xs; Layout.rightMargin: Theme.space.xs }

        IconButton {
            iconName: "bar-chart"
            tip: qsTr("Log summary: per-ID statistics and bus load")
            onClicked: bar.summaryRequested()
        }
        IconButton {
            iconName: "table"
            tip: qsTr("Export the whole log as CSV")
            onClicked: bar.exportRequested()
        }
    }
}
