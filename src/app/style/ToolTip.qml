import QtQuick
import QtQuick.Templates as T
import CanUi

T.ToolTip {
    id: control

    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: parent ? parent.height + 6 : 0
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    margins: 6
    leftPadding: Theme.space.sm + 1
    rightPadding: Theme.space.sm + 1
    topPadding: 5
    bottomPadding: 5
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.fast }
    }

    contentItem: Text {
        text: control.text
        color: Theme.dark ? Theme.text : "#F5F6F8"
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        radius: Theme.radius.md
        color: Theme.dark ? Theme.popup : "#20242D"
        border.width: Theme.dark ? 1 : 0
        border.color: Theme.borderStrong
    }
}
