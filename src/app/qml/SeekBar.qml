import QtQuick
import QtQuick.Controls
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Replay seek bar: played part, markers (flags), a draggable handle and a
// hover time preview. Dragging previews the time; the seek happens on release
// (a seek rebuilds every view, so it is not done for every mouse move).
Item {
    id: bar
    readonly property var replay: Session.replay
    readonly property real span: Math.max(1, replay.endUs - replay.startUs)
    property bool dragging: false
    property real dragUs: 0
    readonly property real shownUs: dragging ? dragUs : replay.positionUs
    property real hoverUs: -1
    property bool demoHover: false   // demo: show the hover preview at hoverUs

    implicitHeight: 32

    function xOf(us) { return (us - replay.startUs) / span * track.width }
    function usAt(x) { return replay.startUs + Math.max(0, Math.min(1, x / track.width)) * span }

    Item {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 7
        anchors.rightMargin: 7
        anchors.verticalCenter: parent.verticalCenter
        height: 6

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: Theme.alpha(Theme.text, 0.10)
        }
        Rectangle {
            width: Math.max(0, Math.min(track.width, bar.xOf(bar.shownUs)))
            height: parent.height
            radius: 3
            color: Theme.accent
        }
    }

    // Markers: a thin line through the track and a flag above it.
    Repeater {
        model: Session.markers
        Item {
            id: mk
            required property var timeUs
            required property string name
            required property int index
            x: track.x + bar.xOf(timeUs) - width / 2
            width: 12
            height: bar.height
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 3
                width: 2
                height: parent.height - 6
                radius: 1
                color: Theme.warning
            }
            Rectangle {
                x: parent.width / 2
                y: 2
                width: 7
                height: 6
                color: Theme.warning
            }
            HoverHandler { id: mkHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Session.seekReplay(mk.timeUs) }
            ToolTip.visible: mkHover.hovered
            ToolTip.text: mk.name + "  ·  " + Fmt.clock(mk.timeUs - bar.replay.startUs)
            ToolTip.delay: 200
        }
    }

    // Handle
    Rectangle {
        id: handle
        x: track.x + Math.max(0, Math.min(track.width, bar.xOf(bar.shownUs))) - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: bar.dragging || area.containsMouse ? 16 : 14
        height: width
        radius: width / 2
        color: Theme.surface
        border.width: 2
        border.color: Theme.accent
        Behavior on width { NumberAnimation { duration: Theme.motion.fast } }
    }

    // Hover / drag preview bubble
    Rectangle {
        id: bubble
        readonly property real us: bar.dragging ? bar.dragUs : bar.hoverUs
        visible: (bar.dragging || area.containsMouse || bar.demoHover) && us >= 0
        x: Math.max(0, Math.min(bar.width - width, track.x + bar.xOf(us) - width / 2))
        y: -height - 4
        width: bubbleText.implicitWidth + 14
        height: 22
        radius: Theme.radius.sm
        color: Theme.popup
        border.width: 1
        border.color: Theme.borderStrong
        Text {
            id: bubbleText
            anchors.centerIn: parent
            text: Fmt.clock(bubble.us - bar.replay.startUs)
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.caption
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: !bar.demoHover
        z: -1
        cursorShape: Qt.PointingHandCursor
        onPositionChanged: (mouse) => {
            bar.hoverUs = bar.usAt(mouse.x - track.x)
            if (pressed)
                bar.dragUs = bar.hoverUs
        }
        onExited: if (!bar.demoHover) bar.hoverUs = -1
        onPressed: (mouse) => {
            bar.dragging = true
            bar.dragUs = bar.usAt(mouse.x - track.x)
        }
        onReleased: {
            if (bar.dragging)
                Session.seekReplay(bar.dragUs)
            bar.dragging = false
        }
        onCanceled: bar.dragging = false
    }
}
