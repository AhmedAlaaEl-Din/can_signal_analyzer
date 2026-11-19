import QtQuick
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Slim bottom bar with live status chips.
Rectangle {
    id: bar
    implicitHeight: Theme.size.statusbar + 4
    color: Theme.surface

    readonly property string stateTone: Session.paused ? "warning" : Session.running ? "ok" : "neutral"
    readonly property string loadTone: Session.busLoad >= 0.8 ? "error"
                                     : Session.busLoad >= 0.5 ? "warning" : "neutral"

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl
        anchors.rightMargin: Theme.space.xl
        spacing: Theme.space.sm

        Chip {
            dot: true
            pulsing: Session.running && !Session.paused
            tone: Session.replayMode ? (Session.running ? "accent" : "neutral") : bar.stateTone
            label: Session.replayMode ? (Session.running ? qsTr("Replay") : Session.replay.atEnd ? qsTr("Replay ended") : qsTr("Replay paused"))
                 : Session.paused ? qsTr("Paused") : Session.running ? qsTr("Live") : qsTr("Stopped")
            value: Session.paused && Session.pendingWhilePaused > 0
                   ? qsTr("+%1 held").arg(Fmt.grouped(Session.pendingWhilePaused)) : ""
        }
        Chip {
            visible: Session.recorder.recording
            dot: true
            pulsing: true
            tone: "error"
            label: qsTr("Recording")
            value: Fmt.duration(Session.recorder.durationUs) + " · " + Fmt.bytes(Session.recorder.bytesWritten)
        }
        Chip {
            label: qsTr("Rate")
            value: Fmt.grouped(Session.framesPerSecond) + " fr/s"
            minValueWidth: 64
        }
        Chip {
            label: qsTr("Bus load")
            value: Fmt.percent(Session.busLoad)
            tone: bar.loadTone
            minValueWidth: 44
        }
        Chip {
            label: qsTr("Total")
            value: Fmt.grouped(Session.totalFrames)
            minValueWidth: 56
        }
        Chip {
            label: qsTr("Buffer")
            value: Fmt.percent(Session.frameModel.bufferedCount / Session.frameModel.capacity, 0)
            minValueWidth: 32
        }

        Item { Layout.fillWidth: true }

        Text {
            text: Session.sourceDescription
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
            elide: Text.ElideRight
            Layout.maximumWidth: 260
        }
        Divider { vertical: true; Layout.preferredHeight: 14; Layout.leftMargin: Theme.space.xs; Layout.rightMargin: Theme.space.xs }
        Row {
            spacing: Theme.space.xs
            Kbd { text: "F5" }
            Text { text: qsTr("run"); color: Theme.textFaint; font.pixelSize: Theme.type.small; font.family: Theme.fontSans; anchors.verticalCenter: parent.verticalCenter }
            Item { width: Theme.space.xs; height: 1 }
            Kbd { text: "Space" }
            Text { text: qsTr("pause"); color: Theme.textFaint; font.pixelSize: Theme.type.small; font.family: Theme.fontSans; anchors.verticalCenter: parent.verticalCenter }
            Item { width: Theme.space.xs; height: 1 }
            Kbd { text: "Ctrl+F" }
            Text { text: qsTr("find"); color: Theme.textFaint; font.pixelSize: Theme.type.small; font.family: Theme.fontSans; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
