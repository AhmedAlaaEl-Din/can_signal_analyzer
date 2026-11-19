import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Record / Stop control for the toolbar. Idle: a "Record" button. While
// recording: a red chip with the elapsed log time and the file size, and a
// Stop button.
RowLayout {
    id: root
    spacing: Theme.space.xs
    readonly property var rec: Session.recorder
    signal recordRequested()

    ToolButton {
        visible: !root.rec.recording
        iconName: "record"
        iconSize: 12
        text: qsTr("Record")
        enabled: !Session.replayMode
        onClicked: root.recordRequested()
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Record everything the bus delivers to a .canlog file  (Ctrl+R)")
        ToolTip.delay: 600
        iconColor: enabled ? Theme.error : Theme.textFaint // the record dot is always red
    }

    Rectangle {
        visible: root.rec.recording
        implicitHeight: 30
        implicitWidth: recRow.implicitWidth + Theme.space.sm + 6
        radius: Theme.radius.md
        color: Theme.alpha(Theme.error, 0.10 + 0.04 * Theme.t)
        border.width: 1
        border.color: Theme.alpha(Theme.error, 0.35)
        RowLayout {
            id: recRow
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.space.sm + 2
            spacing: Theme.space.sm
            Rectangle {
                width: 9; height: 9; radius: 4.5
                color: Theme.error
                SequentialAnimation on opacity {
                    running: root.rec.recording
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 700; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
                }
            }
            Text {
                text: "REC"
                color: Theme.error
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
                font.weight: Font.Bold
                font.letterSpacing: 0.8
            }
            Text {
                text: Fmt.duration(root.rec.durationUs)
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                font.features: { "tnum": 1 }
            }
            Text {
                text: Fmt.bytes(root.rec.bytesWritten)
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                font.features: { "tnum": 1 }
            }
            IconButton {
                extent: 22
                iconSize: 11
                iconName: "stop"
                iconColor: Theme.error
                tip: qsTr("Stop recording and close the file  (Ctrl+R)")
                onClicked: Session.stopRecording()
            }
        }
        ToolTip.visible: recHover.hovered
        ToolTip.text: qsTr("Recording to %1").arg(root.rec.fileName)
        ToolTip.delay: 400
        HoverHandler { id: recHover }
    }
}
