import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

// About: version, build, Qt version and the license notices.
Popup {
    id: dialog
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent ? parent.width - 64 : 520)
    modal: true
    dim: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle { color: Theme.alpha("#000000", 0.28 + 0.17 * Theme.t) }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.fast } }
    background: Rectangle {
        radius: Theme.radius.lg
        color: Theme.popup
        border.width: 1
        border.color: Theme.borderStrong
    }

    component InfoRow: RowLayout {
        property string label
        property string value
        Layout.fillWidth: true
        spacing: Theme.space.md
        Text {
            Layout.preferredWidth: 96
            text: parent.label
            color: Theme.textFaint
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
        }
        Text {
            Layout.fillWidth: true
            text: parent.value
            color: Theme.text
            font.family: Theme.fontSans
            font.pixelSize: Theme.type.small
            font.features: { "tnum": 1 }
            wrapMode: Text.WordWrap
        }
    }
    component LinkButton: Text {
        id: link
        property url target
        property string label
        text: label
        color: target.toString().length > 0 ? (linkHover.hovered ? Theme.accentHover : Theme.accentText) : Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
        font.underline: target.toString().length > 0 && linkHover.hovered
        HoverHandler { id: linkHover; cursorShape: link.target.toString().length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor }
        TapHandler { enabled: link.target.toString().length > 0; onTapped: Qt.openUrlExternally(link.target) }
    }

    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.xl
            Layout.bottomMargin: Theme.space.lg
            spacing: Theme.space.lg
            Image {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                source: "qrc:/qt/qml/CanAnalyzer/icons/app.svg"
                sourceSize: Qt.size(128, 128)
                smooth: true
            }
            ColumnLayout {
                spacing: 2
                Layout.fillWidth: true
                Text {
                    text: AppInfo.name
                    color: Theme.text
                    font.family: Theme.fontDisplay
                    font.pixelSize: Theme.type.headline + 2
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Version %1").arg(AppInfo.version)
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.body
                }
                Text {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    text: qsTr("CAN signal analyzer for simulated and recorded CAN traffic: trace, DBC decoding, plots, logging and replay.")
                    wrapMode: Text.WordWrap
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                }
            }
            IconButton {
                Layout.alignment: Qt.AlignTop
                iconName: "close"
                tip: qsTr("Close  (Esc)")
                onClicked: dialog.close()
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.xl
            Layout.topMargin: Theme.space.lg
            Layout.bottomMargin: Theme.space.lg
            spacing: Theme.space.sm
            InfoRow { label: qsTr("Build"); value: AppInfo.buildInfo }
            InfoRow {
                label: qsTr("Qt")
                value: AppInfo.qtVersion === AppInfo.qtBuildVersion
                       ? qsTr("%1 (dynamically linked)").arg(AppInfo.qtVersion)
                       : qsTr("%1 loaded, built against %2").arg(AppInfo.qtVersion).arg(AppInfo.qtBuildVersion)
            }
            InfoRow { label: qsTr("License"); value: qsTr("MIT for CAN Analyzer itself") }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            implicitHeight: noticeCol.implicitHeight + 2 * Theme.space.md
            radius: Theme.radius.md
            color: Theme.alpha(Theme.text, 0.035)
            border.width: 1
            border.color: Theme.border
            ColumnLayout {
                id: noticeCol
                x: Theme.space.md
                y: Theme.space.md
                width: parent.width - 2 * Theme.space.md
                spacing: Theme.space.sm
                Text {
                    Layout.fillWidth: true
                    text: qsTr("This application uses the Qt framework under the GNU Lesser General Public License version 3 (LGPLv3). Qt is linked dynamically and was not modified; you can replace the Qt libraries with your own build. The license texts, the Qt source location and the replacement steps are in the THIRD_PARTY folder.")
                    wrapMode: Text.WordWrap
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    lineHeight: 1.15
                }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.space.lg
                    LinkButton { label: qsTr("LGPLv3 license text"); target: AppInfo.qtLicenseFile }
                    LinkButton { label: qsTr("Third-party notices"); target: AppInfo.licenseFolder }
                    LinkButton { label: qsTr("MIT license"); target: AppInfo.appLicenseFile }
                }
                Text {
                    visible: AppInfo.qtLicenseFile.toString().length === 0
                    Layout.fillWidth: true
                    text: qsTr("The license files are shipped next to can_analyzer.exe in the release package.")
                    wrapMode: Text.WordWrap
                    color: Theme.textFaint
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.space.xl
            Layout.topMargin: Theme.space.lg
            Text {
                Layout.fillWidth: true
                text: qsTr("Simulated data only · invented demo vehicle")
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.caption
            }
            Button { variant: "primary"; text: qsTr("Close"); onClicked: dialog.close() }
        }
    }
}
