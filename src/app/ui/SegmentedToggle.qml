import QtQuick
import QtQuick.Templates as T

// Compact segmented control, e.g. [ Names | Hex ]. The selection pill slides.
Rectangle {
    id: root
    property var options: []          // strings
    property int currentIndex: 0
    property string tip: ""
    signal activated(int index)

    readonly property int segmentHeight: 26
    implicitHeight: segmentHeight + 6
    implicitWidth: row.implicitWidth + 6
    radius: Theme.radius.md + 1
    color: Theme.alpha(Theme.textFaint, 0.10)
    opacity: enabled ? 1 : 0.5

    Rectangle {
        id: pill
        y: 3
        height: root.segmentHeight
        radius: Theme.radius.md - 1
        color: Theme.surface
        border.width: 1
        border.color: Theme.border
        readonly property Item target: repeater.count > root.currentIndex ? repeater.itemAt(root.currentIndex) : null
        x: target ? target.x + 3 : 3
        width: target ? target.width : 0
        Behavior on x { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
        Behavior on width { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    Row {
        id: row
        x: 3
        y: 3
        Repeater {
            id: repeater
            model: root.options
            T.AbstractButton {
                id: seg
                required property int index
                required property string modelData
                width: label.implicitWidth + 2 * Theme.space.md
                height: root.segmentHeight
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                onClicked: { root.currentIndex = index; root.activated(index) }
                contentItem: Text {
                    id: label
                    text: seg.modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: seg.index === root.currentIndex ? Theme.text : seg.hovered ? Theme.text : Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    font.weight: seg.index === root.currentIndex ? Font.DemiBold : Font.Medium
                }
                background: Item {
                    FocusRing { target: seg }
                }
            }
        }
    }
    T.ToolTip.visible: hover.hovered && tip.length > 0
    T.ToolTip.text: tip
    T.ToolTip.delay: 600
    HoverHandler { id: hover }
}
