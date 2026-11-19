import QtQuick
import QtQuick.Templates as T
import CanUi

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Theme.size.control, implicitIndicatorHeight + topPadding + bottomPadding)
    padding: 0
    spacing: Theme.space.sm
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body

    indicator: Rectangle {
        implicitWidth: 32
        implicitHeight: 18
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: 9
        color: control.checked ? Theme.accent : Theme.alpha(Theme.textFaint, control.hovered ? 0.45 : 0.32)
        Behavior on color { ColorAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

        Rectangle {
            width: 14
            height: 14
            radius: 7
            y: 2
            x: control.checked ? parent.width - width - 2 : 2
            color: control.checked ? Theme.textOnAccent : "#FFFFFF"
            scale: control.pressed ? 0.9 : 1
            Behavior on x { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
            Behavior on scale { NumberAnimation { duration: Theme.motion.fast } }
        }
        FocusRing { target: control }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        font: control.font
        color: control.enabled ? Theme.textMuted : Theme.textFaint
        verticalAlignment: Text.AlignVCenter
    }
}
