import QtQuick
import QtQuick.Templates as T
import QtQuick.Effects
import CanUi

T.ComboBox {
    id: control
    property string prefix: ""   // muted label shown inside the field, e.g. "Rate"

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.size.control
    leftPadding: Theme.space.md - 2
    rightPadding: 30
    hoverEnabled: true
    focusPolicy: Qt.TabFocus

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body

    delegate: T.ItemDelegate {
        id: item
        required property var model
        required property int index
        width: ListView.view.width
        implicitHeight: 30
        leftPadding: Theme.space.md - 2
        rightPadding: Theme.space.md - 2
        highlighted: control.highlightedIndex === index
        hoverEnabled: true
        text: control.textRole ? model[control.textRole] : model.modelData
        contentItem: Row {
            spacing: Theme.space.sm
            Text {
                text: item.text
                color: control.currentIndex === item.index ? Theme.accentText : Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.body
                font.weight: control.currentIndex === item.index ? Font.DemiBold : Font.Normal
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        background: Rectangle {
            radius: Theme.radius.sm
            color: item.highlighted || item.hovered ? Theme.hover : "transparent"
        }
    }

    indicator: Icon {
        x: control.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 14
        color: control.hovered ? Theme.text : Theme.textMuted
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    contentItem: Row {
        spacing: Theme.space.sm - 2
        Text {
            visible: control.prefix.length > 0
            text: control.prefix
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.body
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            text: control.displayText
            color: control.enabled ? Theme.text : Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.body
            font.weight: Font.Medium
            font.features: { "tnum": 1 }
            elide: Text.ElideRight
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    background: Rectangle {
        implicitWidth: 140
        implicitHeight: Theme.size.control
        radius: Theme.radius.md
        color: control.down ? Theme.pressed : control.hovered ? Theme.hover : "transparent"
        border.width: 1
        border.color: control.popup.visible ? Theme.accent
                    : control.hovered ? Theme.borderStrong : Theme.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
        FocusRing { target: control }
    }

    popup: T.Popup {
        y: control.height + 4
        width: Math.max(control.width, 160)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 320)
        padding: 4

        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
                NumberAnimation { property: "y"; from: control.height - 2; to: control.height + 4; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
            }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.fast }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            spacing: 1
            boundsBehavior: Flickable.StopAtBounds
        }

        background: Item {
            Rectangle {
                id: popupShadowSource
                anchors.fill: popupBg
                radius: popupBg.radius
                color: "black"
                visible: false
            }
            MultiEffect {
                source: popupShadowSource
                anchors.fill: popupShadowSource
                shadowEnabled: true
                shadowColor: Theme.shadow
                shadowBlur: Theme.shadowBlur(3)
                shadowVerticalOffset: Theme.shadowOffset(3)
                autoPaddingEnabled: true
            }
            Rectangle {
                id: popupBg
                anchors.fill: parent
                radius: Theme.radius.md + 2
                color: Theme.popup
                border.width: 1
                border.color: Theme.border
            }
        }
    }
}
