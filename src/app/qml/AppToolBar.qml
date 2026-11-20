import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

// Top bar: identity on the left, capture controls in the middle, source
// settings and theme switch on the right.
Rectangle {
    id: bar
    implicitHeight: Theme.size.toolbar
    color: Theme.surface

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.border
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl
        anchors.rightMargin: Theme.space.lg
        spacing: Theme.space.sm

        // Identity
        Rectangle {
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            radius: 8
            color: Theme.accent
            Icon {
                anchors.centerIn: parent
                name: "pulse"
                size: 18
                color: Theme.textOnAccent
            }
        }
        Column {
            Layout.leftMargin: Theme.space.xs
            spacing: 0
            Text {
                text: qsTr("CAN Analyzer")
                color: Theme.text
                font.family: Theme.fontDisplay
                font.pixelSize: Theme.type.title
                font.weight: Font.DemiBold
            }
            Text {
                text: qsTr("Simulated bus · %1 kbit/s").arg(Session.bitrate / 1000)
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
            }
        }

        Divider {
            vertical: true
            Layout.preferredHeight: 28
            Layout.leftMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.md
        }

        // Capture controls
        Button {
            id: runButton
            variant: Session.running ? "secondary" : "primary"
            iconName: Session.running ? "stop" : "play"
            text: Session.running ? qsTr("Stop") : qsTr("Start")
            Layout.preferredWidth: 92
            onClicked: Session.toggleRunning()
            ToolTip.visible: hovered
            ToolTip.text: Session.running ? qsTr("Stop the simulator  (F5)") : qsTr("Start the simulator  (F5)")
            ToolTip.delay: 600
        }
        ToolButton {
            iconName: Session.paused ? "play" : "pause"
            text: Session.paused ? qsTr("Resume") : qsTr("Pause")
            checkable: true
            checked: Session.paused
            Layout.preferredWidth: 96
            onClicked: Session.togglePause()
            ToolTip.visible: hovered
            ToolTip.text: Session.paused ? qsTr("Resume the live view; nothing was lost  (Space)")
                                         : qsTr("Freeze the view, keep capturing  (Space)")
            ToolTip.delay: 600
        }
        ToolButton {
            iconName: "trash"
            text: qsTr("Clear")
            onClicked: Session.clear()
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Clear trace and statistics  (Ctrl+L)")
            ToolTip.delay: 600
        }

        Item { Layout.fillWidth: true }

        // Source settings
        ComboBox {
            id: rateBox
            prefix: qsTr("Rate")
            Layout.preferredWidth: 168
            textRole: "label"
            valueRole: "rate"
            model: [
                { label: qsTr("Nominal ≈%1/s").arg(Math.round(Session.nominalRate)), rate: 0 },
                { label: qsTr("500 frames/s"), rate: 500 },
                { label: qsTr("1000 frames/s"), rate: 1000 },
                { label: qsTr("2000 frames/s"), rate: 2000 }
            ]
            Component.onCompleted: currentIndex = indexOfValue(Session.targetRate)
            onActivated: Session.targetRate = currentValue
            Connections {
                target: Session
                function onTargetRateChanged() { rateBox.currentIndex = rateBox.indexOfValue(Session.targetRate) }
            }
            ToolTip.visible: hovered && !popup.visible
            ToolTip.text: qsTr("Total simulated frame rate")
            ToolTip.delay: 600
        }
        Switch {
            text: qsTr("Bursts")
            checked: Session.burstsEnabled
            onToggled: Session.burstsEnabled = checked
            Layout.leftMargin: Theme.space.sm
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Inject a 16-frame burst every 2.5 s")
            ToolTip.delay: 600
        }

        Divider {
            vertical: true
            Layout.preferredHeight: 28
            Layout.leftMargin: Theme.space.md
            Layout.rightMargin: Theme.space.xs
        }

        IconButton {
            id: themeButton
            iconName: Theme.dark ? "sun" : "moon"
            tip: Theme.dark ? qsTr("Switch to light theme  (Ctrl+T)") : qsTr("Switch to dark theme  (Ctrl+T)")
            onClicked: Theme.toggle()
            // small spin when the icon swaps
            RotationAnimation on rotation {
                id: spin
                running: false
                from: -40; to: 0
                duration: Theme.motion.slow
                easing.type: Easing.OutBack
            }
            Connections {
                target: Theme
                function onDarkChanged() { spin.restart() }
            }
        }
    }
}
