import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer
import CanAnalyzer.Core

// Right-hand panel with everything about one signal: live value, 10 s chart,
// definition (start bit, length, byte order, scaling, ranges), comment, value
// table and the bit layout over the 8 data bytes.
Card {
    id: panel
    property int signalId: -1
    property var details: ({})
    property var live: ({})
    signal closeRequested()

    elevation: 1

    function refresh() {
        details = signalId >= 0 ? Session.signalModel.signalDetails(signalId) : ({})
        live = signalId >= 0 ? Session.signalModel.liveValue(signalId) : ({})
    }
    onSignalIdChanged: refresh()
    Connections {
        target: Session.signalModel
        function onValuesChanged() { if (panel.signalId >= 0) panel.live = Session.signalModel.liveValue(panel.signalId) }
        function onStructureChanged() { if (panel.signalId >= 0) panel.details = Session.signalModel.signalDetails(panel.signalId) }
    }

    component SectionTitle: Text {
        color: Theme.textFaint
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.caption
        font.weight: Font.DemiBold
        font.letterSpacing: Theme.type.captionSpacing
        topPadding: Theme.space.sm
    }
    component PropLabel: Text {
        color: Theme.textMuted
        font.family: Theme.fontSans
        font.pixelSize: Theme.type.small
        Layout.preferredWidth: 104
        Layout.alignment: Qt.AlignTop
    }
    component PropValue: Text {
        color: Theme.text
        font.family: Theme.fontMono
        font.pixelSize: Theme.type.small
        Layout.fillWidth: true
        wrapMode: Text.WrapAnywhere
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 1
        contentHeight: content.implicitHeight + 2 * Theme.space.lg
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        ColumnLayout {
            id: content
            x: Theme.space.lg + 2
            y: Theme.space.lg
            width: flick.width - 2 * (Theme.space.lg + 2)
            spacing: Theme.space.sm

            // ---- title
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space.xs
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        Layout.fillWidth: true
                        text: panel.details.name || ""
                        elide: Text.ElideRight
                        color: Theme.text
                        font.family: Theme.fontDisplay
                        font.pixelSize: Theme.type.title + 3
                        font.weight: Font.DemiBold
                    }
                    Text {
                        Layout.fillWidth: true
                        text: (panel.details.message || "") + "  ·  0x" + (panel.details.frameId || "")
                              + (panel.details.cycleTimeMs > 0 ? qsTr("  ·  every %1 ms").arg(panel.details.cycleTimeMs) : "")
                        elide: Text.ElideRight
                        color: Theme.textMuted
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.type.small
                    }
                }
                IconButton {
                    Layout.alignment: Qt.AlignTop
                    iconName: panel.details.pinned ? "pin-filled" : "pin"
                    checked: !!panel.details.pinned
                    tip: panel.details.pinned ? qsTr("Unpin") : qsTr("Pin to the top")
                    onClicked: Session.signalModel.togglePin(panel.signalId)
                }
                IconButton {
                    Layout.alignment: Qt.AlignTop
                    iconName: "close"
                    tip: qsTr("Close details  (Esc)")
                    onClicked: panel.closeRequested()
                }
            }

            // ---- live value
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xs
                implicitHeight: liveColumn.implicitHeight + 2 * Theme.space.md
                radius: Theme.radius.md
                color: Theme.sunken
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: liveColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: Theme.space.md
                    spacing: Theme.space.xs

                    RowLayout {
                        spacing: Theme.space.sm
                        Text {
                            text: panel.live.value || "—"
                            color: panel.live.outOfRange ? Theme.error : Theme.text
                            font.family: Theme.fontMono
                            font.pixelSize: 26
                            font.weight: Font.DemiBold
                        }
                        Text {
                            Layout.alignment: Qt.AlignBaseline
                            text: panel.details.unit || ""
                            color: Theme.textMuted
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.type.title
                        }
                        Item { Layout.fillWidth: true }
                        Chip {
                            visible: !!panel.live.enumText
                            tone: "accent"
                            label: panel.live.enumText || ""
                        }
                        Chip {
                            visible: !!panel.live.outOfRange
                            tone: "error"
                            iconName: "alert"
                            label: qsTr("out of range")
                        }
                    }
                    Sparkline {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        model: Session.signalModel
                        signalId: panel.signalId
                        revision: Session.signalModel.timeUs
                        timeUs: Session.signalModel.timeUs
                        stepped: (panel.details.valueTable || []).length > 0
                        color: panel.live.outOfRange ? Theme.error : Theme.accent
                        areaColor: Theme.alpha(Theme.accent, 0.18 + 0.06 * Theme.t)
                        gridColor: Theme.border
                        lineWidth: 1.75
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: qsTr("last 10 s")
                            color: Theme.textFaint
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.type.caption
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: panel.live.hasValue ? qsTr("min %1  ·  max %2  ·  raw %3").arg(panel.live.min).arg(panel.live.max).arg(panel.live.raw) : qsTr("waiting for frames")
                            color: Theme.textMuted
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.type.caption
                        }
                    }
                }
            }

            // ---- bit layout
            SectionTitle { text: qsTr("BIT LAYOUT  ·  %1 BYTES").arg(panel.details.dlc !== undefined ? panel.details.dlc : 8) }
            BitLayout {
                Layout.topMargin: 2
                bits: panel.details.bits || []
                bytes: panel.live.bytes || []
                dlc: panel.details.dlc !== undefined ? panel.details.dlc : 8
            }

            // ---- definition
            SectionTitle { text: qsTr("DEFINITION") }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space.md
                rowSpacing: 5
                PropLabel { text: qsTr("Start bit") }
                PropValue { text: panel.details.startBit !== undefined ? panel.details.startBit : "" }
                PropLabel { text: qsTr("Length") }
                PropValue { text: panel.details.length !== undefined ? qsTr("%1 bit").arg(panel.details.length) : "" }
                PropLabel { text: qsTr("Byte order") }
                PropValue { text: panel.details.byteOrder || "" }
                PropLabel { text: qsTr("Value type") }
                PropValue { text: panel.details.signed ? qsTr("signed") : qsTr("unsigned") }
                PropLabel { text: qsTr("Scaling") }
                PropValue { text: qsTr("raw × %1 + %2").arg(panel.details.factor || "1").arg(panel.details.offset || "0") }
                PropLabel { text: qsTr("Range") }
                PropValue {
                    text: panel.details.hasRange ? (panel.details.minimum + " … " + panel.details.maximum + " " + (panel.details.unit || "")) : qsTr("not specified")
                }
                PropLabel { text: qsTr("Multiplexing"); visible: !!panel.details.multiplexing }
                PropValue { text: panel.details.multiplexing || ""; visible: !!panel.details.multiplexing }
                PropLabel { text: qsTr("Sender") }
                PropValue { text: panel.details.transmitter || "—"; font.family: Theme.fontSans }
                PropLabel { text: qsTr("Receivers") }
                PropValue { text: panel.details.receivers || "—"; font.family: Theme.fontSans; wrapMode: Text.WordWrap }
            }

            // ---- comment
            SectionTitle { text: qsTr("COMMENT"); visible: !!panel.details.comment }
            Text {
                visible: !!panel.details.comment
                Layout.fillWidth: true
                text: (panel.details.comment || "").replace(/\s*\n\s*/g, " ")
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                lineHeight: 1.2
            }

            // ---- value table
            SectionTitle { text: qsTr("VALUES"); visible: (panel.details.valueTable || []).length > 0 }
            Flow {
                visible: (panel.details.valueTable || []).length > 0
                Layout.fillWidth: true
                spacing: Theme.space.xs + 2
                Repeater {
                    model: panel.details.valueTable || []
                    Rectangle {
                        required property var modelData
                        readonly property bool current: panel.live.hasValue && Number(panel.live.raw) === Number(modelData.value)
                        height: 22
                        width: valueRow.implicitWidth + 14
                        radius: Theme.radius.pill
                        color: current ? Theme.accentSubtle : Theme.alpha(Theme.textFaint, 0.08)
                        border.width: current ? 1 : 0
                        border.color: Theme.alpha(Theme.accent, 0.5)
                        Row {
                            id: valueRow
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: modelData.value
                                color: Theme.textFaint
                                font.family: Theme.fontMono
                                font.pixelSize: Theme.type.caption
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: modelData.text
                                color: parent.parent.current ? Theme.accentText : Theme.text
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.type.small
                                font.weight: parent.parent.current ? Font.DemiBold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
            }

        }
    }
}
