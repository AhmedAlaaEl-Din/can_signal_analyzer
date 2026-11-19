import QtQuick
import CanUi

// One trace line. Kept deliberately light: a few Text items, no layouts,
// so thousands of rows per second can stream through the pool of delegates.
// The selected row expands (when a database is loaded) to show its decoded
// signals.
Item {
    id: row

    required property int index
    required property string time
    required property int channel
    required property string frameId
    required property bool extended
    required property int dlc
    required property string payload
    required property real seq
    required property real ordinal
    required property bool fresh
    required property string delta
    required property string messageName

    property var columns: []
    property int cellSpacing: 20
    property bool selected: false
    property bool animateIn: true
    property bool showNames: false
    property var decoded: []        // set for the expanded (selected) row
    signal clicked()

    readonly property bool expanded: selected && decoded.length > 0
    readonly property int lineHeight: Theme.size.row
    height: lineHeight + (expanded ? detail.implicitHeight + Theme.space.sm : 0)

    function maybeFadeIn() {
        if (fresh && animateIn) {
            fadeIn.restart()
        } else {
            fadeIn.stop()
            content.opacity = 1
        }
    }
    Component.onCompleted: maybeFadeIn()
    ListView.onReused: maybeFadeIn()

    Rectangle {
        anchors.fill: parent
        color: row.selected ? Theme.selection
             : hover.hovered ? Theme.hover
             : (row.ordinal % 2 === 1 ? Theme.zebra : "transparent")
    }
    Rectangle {
        visible: row.selected
        width: 2
        height: parent.height
        color: Theme.accent
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: row.clicked() }

    Row {
        id: content
        x: Theme.space.lg
        height: row.lineHeight
        spacing: row.cellSpacing

        NumberAnimation {
            id: fadeIn
            target: content
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motion.slow
            easing.type: Easing.OutCubic
        }

        Text {
            width: row.columns[0].width
            height: parent.height
            text: row.time
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: Theme.textMuted
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.mono
        }
        Text {
            width: row.columns[1].width
            height: parent.height
            text: row.channel
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: Theme.textFaint
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.mono
        }
        Item {
            width: row.columns[2].width
            height: parent.height
            readonly property bool named: row.showNames && row.messageName.length > 0
            Text {
                id: idText
                height: parent.height
                width: Math.min(implicitWidth, parent.width - (parent.named ? 0 : 36))
                elide: Text.ElideRight
                text: parent.named ? row.messageName : row.frameId
                verticalAlignment: Text.AlignVCenter
                color: row.selected ? Theme.accentText : Theme.text
                font.family: parent.named ? Theme.fontSans : Theme.fontMono
                font.pixelSize: parent.named ? Theme.type.body : Theme.type.mono
                font.weight: parent.named ? Font.Medium : Font.DemiBold
            }
            ExtTag {
                visible: row.extended && !parent.named
                anchors.left: idText.right
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Text {
            width: row.columns[3].width
            height: parent.height
            text: row.delta
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: Theme.textMuted
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.mono
        }
        Text {
            width: row.columns[4].width
            height: parent.height
            text: row.dlc
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
            color: Theme.textMuted
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.mono
        }
        Text {
            height: parent.height
            text: row.payload
            verticalAlignment: Text.AlignVCenter
            color: Theme.text
            font.family: Theme.fontMono
            font.pixelSize: Theme.type.mono
            font.wordSpacing: 3
        }
    }

    // Decoded signals of the selected frame.
    DecodedGrid {
        id: detail
        visible: row.expanded
        x: Theme.space.lg + row.columns[0].width + row.columns[1].width + 2 * row.cellSpacing
        y: row.lineHeight
        width: row.width - x - Theme.space.lg
        signalList: row.expanded ? row.decoded : []
        title: row.showNames || row.messageName.length === 0 ? "" : row.messageName
    }
}
