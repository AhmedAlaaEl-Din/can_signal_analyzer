import QtQuick
import CanUi

// Short confirmation or error message at the bottom of the window.
Rectangle {
    id: toast
    property string text: ""
    property string tone: "ok"
    property bool shown: false

    function show(message, messageTone) {
        text = message
        tone = messageTone || "ok"
        shown = true
        hideTimer.interval = tone === "error" ? 5200 : 2800
        hideTimer.restart()
    }

    width: Math.min(toastRow.implicitWidth + 2 * Theme.space.lg, parent ? parent.width - 48 : 600)
    height: 36
    radius: 18
    color: Theme.popup
    border.width: 1
    border.color: Theme.alpha(Theme.toneColor(tone), 0.55)
    opacity: shown ? 1 : 0
    visible: opacity > 0.01
    scale: shown ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }
    Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    Row {
        id: toastRow
        anchors.centerIn: parent
        spacing: Theme.space.sm
        Icon {
            name: toast.tone === "error" ? "alert" : "pulse"
            size: 14
            color: Theme.toneColor(toast.tone)
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: toast.text
            color: toast.tone === "error" ? Theme.error : Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
            elide: Text.ElideMiddle
            width: Math.min(implicitWidth, (toast.parent ? toast.parent.width : 600) - 110)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    TapHandler { onTapped: toast.shown = false }
    Timer { id: hideTimer; interval: 2800; onTriggered: toast.shown = false }
}
