import QtQuick
import QtQuick.Controls
import CanUi
import CanAnalyzer.Core
import "format.js" as Fmt

// Floating action at the bottom of a table. While paused it explains that the
// view is frozen and how many frames are waiting; otherwise (when `detached`)
// it offers to jump back to the live end.
Button {
    id: pill
    property bool detached: false
    property string detachedText: qsTr("Jump to live")
    property string detachedIcon: "arrow-down"
    readonly property bool paused: Session.paused
    readonly property bool shown: paused || detached
    signal jumpRequested()

    variant: "primary"
    iconName: paused ? "play" : detachedIcon
    text: paused
          ? (Session.pendingWhilePaused > 0
             ? qsTr("Paused · %1 new frames waiting · Resume").arg(Fmt.grouped(Session.pendingWhilePaused))
             : qsTr("Paused · Resume"))
          : detachedText
    height: 30
    opacity: shown ? 1 : 0
    visible: opacity > 0.01
    scale: shown ? 1 : 0.92
    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    Behavior on scale { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    onClicked: {
        if (paused)
            Session.paused = false
        pill.jumpRequested()
    }
    ToolTip.visible: hovered
    ToolTip.text: paused ? qsTr("The view is frozen; capture continues  (Space)")
                         : qsTr("Resume auto-scroll  (End)")
    ToolTip.delay: 600
}
