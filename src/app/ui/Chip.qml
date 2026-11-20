import QtQuick

// Compact status pill: optional dot or icon, a label and a value.
// Values use tabular figures so changing numbers never jitter.
Rectangle {
    id: chip
    property string label: ""
    property string value: ""
    property string tone: "neutral"   // neutral | accent | ok | warning | error
    property string iconName: ""
    property bool dot: false
    property bool pulsing: false
    property int minValueWidth: 0

    implicitHeight: 22
    implicitWidth: row.implicitWidth + 2 * Theme.space.sm + 2
    radius: Theme.radius.pill
    color: Theme.toneFill(tone)
    border.width: 1
    border.color: Theme.alpha(Theme.toneColor(tone), tone === "neutral" ? 0.0 : 0.22)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space.xs + 2

        Item {
            visible: chip.dot
            width: 8
            height: 8
            anchors.verticalCenter: parent.verticalCenter
            Rectangle {
                anchors.centerIn: parent
                width: 7
                height: 7
                radius: 3.5
                color: Theme.toneColor(chip.tone)
            }
            Rectangle {
                id: halo
                anchors.centerIn: parent
                width: 7
                height: 7
                radius: width / 2
                color: "transparent"
                border.width: 1.5
                border.color: Theme.toneColor(chip.tone)
                visible: chip.pulsing
                opacity: 0
                ParallelAnimation {
                    running: chip.pulsing && chip.visible
                    loops: Animation.Infinite
                    NumberAnimation { target: halo; property: "scale"; from: 1; to: 2.3; duration: 1400; easing.type: Easing.OutCubic }
                    NumberAnimation { target: halo; property: "opacity"; from: 0.7; to: 0; duration: 1400; easing.type: Easing.OutCubic }
                }
            }
        }
        Icon {
            visible: chip.iconName.length > 0
            name: chip.iconName
            size: 13
            color: Theme.toneColor(chip.tone)
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: chip.label.length > 0
            text: chip.label
            color: chip.tone === "neutral" ? Theme.textMuted : Theme.toneColor(chip.tone)
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: chip.value.length > 0
            text: chip.value
            width: Math.max(implicitWidth, chip.minValueWidth)
            horizontalAlignment: Text.AlignRight
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
