import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

// Right side of the tab row: database state (load / loaded / unload) and the
// Names | Hex display switch.
RowLayout {
    id: bar
    spacing: Theme.space.sm
    readonly property var db: Session.database

    // ---- nothing loaded
    Button {
        visible: !bar.db.loaded
        variant: "secondary"
        iconName: "database"
        text: qsTr("Load demo database")
        onClicked: ApplicationWindow.window.startDemo()
    }
    ToolButton {
        visible: !bar.db.loaded
        iconName: "folder"
        text: qsTr("Open DBC…")
        onClicked: ApplicationWindow.window.openDatabaseDialog()
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Open a signal database  (Ctrl+O)")
        ToolTip.delay: 600
    }

    // ---- loaded: pill with name and counts
    Rectangle {
        visible: bar.db.loaded
        implicitHeight: 30
        implicitWidth: pillRow.implicitWidth + Theme.space.sm + 4
        radius: Theme.radius.md
        color: Theme.alpha(Theme.accent, 0.08 + 0.04 * Theme.t)
        border.width: 1
        border.color: Theme.alpha(Theme.accent, 0.25)

        RowLayout {
            id: pillRow
            anchors.verticalCenter: parent.verticalCenter
            x: Theme.space.sm + 2
            spacing: Theme.space.sm - 2
            Icon {
                name: "database"
                size: 14
                color: Theme.accentText
            }
            Text {
                text: bar.db.name
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                Layout.maximumWidth: 180
            }
            Text {
                text: qsTr("%1 msgs · %2 signals").arg(bar.db.messageCount).arg(bar.db.signalCount)
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                font.features: { "tnum": 1 }
            }
            IconButton {
                extent: 24
                iconSize: 12
                iconName: "close"
                tip: qsTr("Unload the database")
                onClicked: bar.db.unload()
            }
        }
    }
    IconButton {
        visible: bar.db.loaded
        iconName: "folder"
        tip: qsTr("Open another DBC file  (Ctrl+O)")
        onClicked: ApplicationWindow.window.openDatabaseDialog()
    }

    Divider {
        vertical: true
        Layout.preferredHeight: 20
        Layout.leftMargin: Theme.space.xs
        Layout.rightMargin: Theme.space.xs
    }
    SegmentedToggle {
        options: [qsTr("Names"), qsTr("Hex")]
        enabled: bar.db.loaded
        currentIndex: ApplicationWindow.window.showNames ? 0 : 1
        onActivated: (index) => ApplicationWindow.window.namesPreferred = (index === 0)
        tip: bar.db.loaded ? qsTr("Show message names or raw IDs  (Ctrl+N)")
                           : qsTr("Load a database to show message names")
    }
}
