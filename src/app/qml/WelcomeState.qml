import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

// First screen of a fresh launch (nothing running, nothing loaded): one click
// to a useful view instead of a chain of empty states.
Item {
    id: root
    signal demoRequested()
    signal openLogRequested()
    implicitHeight: col.implicitHeight

    ColumnLayout {
        id: col
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 48, 520)
        spacing: Theme.space.md

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            source: "qrc:/qt/qml/CanAnalyzer/icons/app.svg"
            sourceSize: Qt.size(112, 112)
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.xs
            text: qsTr("Welcome to CAN Analyzer")
            color: Theme.text
            font.family: Theme.fontDisplay
            font.pixelSize: Theme.type.headline
            font.weight: Font.DemiBold
        }
        Text {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Watch a simulated vehicle bus decoded live, or replay a recorded log.")
            wrapMode: Text.WordWrap
            color: Theme.textMuted
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.body
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.sm
            spacing: Theme.space.sm
            Button {
                variant: "primary"
                iconName: "play"
                text: qsTr("Start with the demo")
                onClicked: root.demoRequested()
            }
            Button {
                variant: "secondary"
                iconName: "log-file"
                text: qsTr("Open a log…")
                onClicked: root.openLogRequested()
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space.xs
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("The demo loads the demo database, starts the simulator and opens four plot lanes.")
            wrapMode: Text.WordWrap
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
        }
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Theme.space.xs
            spacing: Theme.space.xs
            Kbd { text: "F5" }
            Text { text: qsTr("simulator only"); color: Theme.textFaint; font.pixelSize: Theme.type.small; font.family: Theme.fontSans; anchors.verticalCenter: parent.verticalCenter }
            Item { width: Theme.space.sm; height: 1 }
            Kbd { text: "Ctrl+Shift+O" }
            Text { text: qsTr("open a log"); color: Theme.textFaint; font.pixelSize: Theme.type.small; font.family: Theme.fontSans; anchors.verticalCenter: parent.verticalCenter }
        }
    }
}
