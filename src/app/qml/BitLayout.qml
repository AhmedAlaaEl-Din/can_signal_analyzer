import QtQuick
import CanUi

// 8 x 8 grid of the payload bits (rows = bytes 0..7, columns = bit 7..0).
// Cells the signal occupies are tinted with the accent; the most significant
// bit is solid, the least significant one outlined. Each cell shows the live
// bit value; the right column shows the byte in hex.
Item {
    id: layout
    property var bits: []          // data-bit indices, MSB first
    property var bytes: []         // 8 ints, -1 = byte not present (beyond DLC / not seen)
    property int dlc: 8
    readonly property int cell: 26
    readonly property int gap: 3
    readonly property int labelWidth: 30
    readonly property int hexWidth: 34

    implicitWidth: labelWidth + 8 * cell + 7 * gap + Theme.space.sm + hexWidth
    implicitHeight: header.height + 8 * (cell - 4) + 7 * gap + Theme.space.xs + legend.height + Theme.space.sm

    function roleOf(bit) {
        const i = bits.indexOf(bit)
        if (i < 0) return "none"
        if (i === 0) return "msb"
        if (i === bits.length - 1) return "lsb"
        return "mid"
    }
    function bitValue(bit) {
        const b = bytes.length > Math.floor(bit / 8) ? bytes[Math.floor(bit / 8)] : -1
        if (b === undefined || b < 0) return ""
        return ((b >> (bit % 8)) & 1).toString()
    }
    function hexOf(byteIndex) {
        const b = bytes.length > byteIndex ? bytes[byteIndex] : -1
        if (b === undefined || b < 0) return "··"
        return (b < 16 ? "0" : "") + b.toString(16).toUpperCase()
    }

    // Column header: bit numbers inside a byte
    Row {
        id: header
        x: layout.labelWidth
        spacing: layout.gap
        height: 16
        Repeater {
            model: 8
            Text {
                required property int index
                width: layout.cell
                horizontalAlignment: Text.AlignHCenter
                text: 7 - index
                color: Theme.textFaint
                font.family: Theme.fontMono
                font.pixelSize: Theme.type.caption
            }
        }
    }

    Column {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: Theme.space.xs
        spacing: layout.gap

        Repeater {
            model: 8
            Row {
                id: byteRow
                required property int index
                readonly property bool present: index < layout.dlc
                spacing: layout.gap
                height: layout.cell - 4

                Text {
                    width: layout.labelWidth - layout.gap
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: "B" + byteRow.index
                    color: byteRow.present ? Theme.textMuted : Theme.alpha(Theme.textFaint, 0.5)
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.type.caption
                }
                Repeater {
                    model: 8
                    Rectangle {
                        id: bitCell
                        required property int index
                        readonly property int bit: byteRow.index * 8 + (7 - index)
                        readonly property string role: layout.roleOf(bit)
                        width: layout.cell
                        height: byteRow.height
                        radius: Theme.radius.sm
                        opacity: byteRow.present ? 1 : 0.35
                        color: role === "msb" ? Theme.accent
                             : role === "none" ? Theme.alpha(Theme.textFaint, 0.08)
                             : Theme.alpha(Theme.accent, 0.22 + 0.08 * Theme.t)
                        border.width: role === "lsb" ? 1.5 : 0
                        border.color: Theme.accent
                        Behavior on color { ColorAnimation { duration: Theme.motion.normal } }

                        Text {
                            anchors.centerIn: parent
                            text: layout.bitValue(bitCell.bit)
                            color: bitCell.role === "msb" ? Theme.textOnAccent
                                 : bitCell.role === "none" ? Theme.textFaint : Theme.text
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.type.small
                            font.weight: bitCell.role === "none" ? Font.Normal : Font.DemiBold
                        }
                    }
                }
                Text {
                    width: layout.hexWidth
                    height: parent.height
                    leftPadding: Theme.space.sm
                    verticalAlignment: Text.AlignVCenter
                    text: layout.hexOf(byteRow.index)
                    color: byteRow.present ? Theme.text : Theme.textFaint
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.type.small
                }
            }
        }
    }

    // Legend
    Row {
        id: legend
        anchors.top: grid.bottom
        anchors.topMargin: Theme.space.sm
        x: layout.labelWidth
        spacing: Theme.space.md
        height: 16
        Repeater {
            model: [
                { label: qsTr("MSB"), kind: "msb" },
                { label: qsTr("signal bits"), kind: "mid" },
                { label: qsTr("LSB"), kind: "lsb" }
            ]
            Row {
                required property var modelData
                spacing: Theme.space.xs + 1
                Rectangle {
                    width: 12; height: 12; radius: 3
                    anchors.verticalCenter: parent.verticalCenter
                    color: modelData.kind === "msb" ? Theme.accent : Theme.alpha(Theme.accent, 0.22 + 0.08 * Theme.t)
                    border.width: modelData.kind === "lsb" ? 1.5 : 0
                    border.color: Theme.accent
                }
                Text {
                    text: modelData.label
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.caption
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
