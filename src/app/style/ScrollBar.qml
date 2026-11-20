import QtQuick
import QtQuick.Templates as T
import CanUi

// Slim overlay scrollbar: 6 px thumb that widens on hover/drag.
T.ScrollBar {
    id: control

    readonly property bool _active: control.hovered || control.pressed

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)
    padding: 3
    minimumSize: 0.06
    hoverEnabled: true
    visible: control.policy !== T.ScrollBar.AlwaysOff
    interactive: true

    contentItem: Rectangle {
        implicitWidth: control.horizontal ? 100 : (control._active ? 8 : 6)
        implicitHeight: control.horizontal ? (control._active ? 8 : 6) : 100
        radius: Math.min(width, height) / 2
        color: control.pressed ? Theme.textMuted
             : control._active ? Theme.alpha(Theme.textMuted, 0.75)
             : Theme.alpha(Theme.textFaint, 0.5)
        opacity: control.size < 1.0 ? 1 : 0
        Behavior on implicitWidth { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on implicitHeight { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
    }

    background: Rectangle {
        implicitWidth: control.horizontal ? 100 : 14
        implicitHeight: control.horizontal ? 14 : 100
        color: control._active && control.size < 1.0 ? Theme.alpha(Theme.textFaint, 0.08) : "transparent"
        radius: 7
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
    }
}
