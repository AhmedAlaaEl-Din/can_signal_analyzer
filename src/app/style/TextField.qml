import QtQuick
import QtQuick.Templates as T
import CanUi

// Input with optional leading icon, monospace option and error state.
T.TextField {
    id: control
    property string iconName: ""
    property bool monospace: false
    property bool error: false

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            placeholderText ? placeholder.implicitWidth + leftPadding + rightPadding : 0)
    implicitHeight: Theme.size.control
    leftPadding: iconName.length > 0 ? 32 : Theme.space.md - 2
    rightPadding: Theme.space.md - 2
    topPadding: 0
    bottomPadding: 0
    verticalAlignment: TextInput.AlignVCenter
    hoverEnabled: true

    font.family: monospace ? Theme.fontMono : Theme.fontSans
    font.pixelSize: Theme.type.body
    color: Theme.text
    placeholderTextColor: Theme.textFaint
    selectionColor: Theme.selection
    selectedTextColor: Theme.text
    selectByMouse: true

    Text {
        id: placeholder
        x: control.leftPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        height: control.height
        text: control.placeholderText
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.body
        color: control.placeholderTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        visible: !control.length && !control.preeditText
        renderType: control.renderType
    }

    background: Rectangle {
        implicitWidth: 180
        implicitHeight: Theme.size.control
        radius: Theme.radius.md
        color: Theme.sunken
        border.width: control.activeFocus ? 1.5 : 1
        border.color: control.error ? Theme.error
                    : control.activeFocus ? Theme.accent
                    : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }

        Icon {
            visible: control.iconName.length > 0
            name: control.iconName
            size: 15
            x: 10
            anchors.verticalCenter: parent.verticalCenter
            color: control.activeFocus ? Theme.accentText : Theme.textFaint
        }
        Rectangle { // soft focus halo
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 3
            border.color: control.error ? Theme.alpha(Theme.error, 0.18) : Theme.alpha(Theme.accent, 0.18)
            opacity: control.activeFocus ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motion.fast } }
        }
    }
}
