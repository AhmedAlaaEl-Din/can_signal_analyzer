import QtQuick
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

// Friendly, dismissible explanation of why a DBC file could not be loaded.
// Shows the first problems with their line numbers; the previous database
// (if any) stays active.
Rectangle {
    id: banner
    readonly property var db: Session.database
    readonly property bool shown: db.errorLines.length > 0

    visible: implicitHeight > 0.5
    implicitHeight: shown ? body.implicitHeight + 2 * Theme.space.md : 0
    clip: true
    radius: Theme.radius.lg
    color: Theme.toneFill("error")
    border.width: shown ? 1 : 0
    border.color: Theme.alpha(Theme.error, 0.35)
    Behavior on implicitHeight { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    RowLayout {
        id: body
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space.md
        anchors.leftMargin: Theme.space.lg
        spacing: Theme.space.md

        Icon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            name: "alert"
            size: 18
            color: Theme.error
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4
            Text {
                text: banner.db.errorTitle
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.body
                font.weight: Font.DemiBold
            }
            Repeater {
                model: banner.db.errorLines
                Text {
                    required property string modelData
                    Layout.fillWidth: true
                    text: modelData
                    wrapMode: Text.WordWrap
                    color: Theme.textMuted
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.type.small
                }
            }
            Text {
                text: banner.db.loaded ? qsTr("The previously loaded database “%1” is still active.").arg(banner.db.name)
                                       : qsTr("Fix the file and open it again, or load the demo database.")
                color: Theme.textMuted
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                topPadding: 2
            }
        }
        IconButton {
            Layout.alignment: Qt.AlignTop
            iconName: "close"
            tip: qsTr("Dismiss")
            onClicked: banner.db.dismissError()
        }
    }
}
