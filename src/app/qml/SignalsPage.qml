import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CanUi
import CanAnalyzer
import CanAnalyzer.Core

// All signals of the loaded database, grouped by message, with live value,
// unit, enumeration text, a 10 s sparkline and min/max seen. Search, collapse,
// pin. Selecting a signal opens the detail panel on the right.
Item {
    id: page
    property bool active: false
    property int selectedSignal: -1
    readonly property var model: Session.signalModel
    readonly property bool hasDb: Session.database.loaded

    readonly property int cellSpacing: 18
    // Responsive columns: lower-priority columns drop out when the list is narrow
    // (small window or detail panel open).
    readonly property real listWidth: listCard.width
    readonly property bool showMinMax: listWidth >= 760
    readonly property bool showState: listWidth >= 600
    readonly property int sparkWidth: listWidth >= 680 ? 148 : 104
    readonly property var columns: {
        const c = [
            { title: "", width: 22, align: "left" },
            { title: qsTr("SIGNAL"), width: 0, align: "left" },
            { title: qsTr("VALUE"), width: 92, align: "right" },
            { title: qsTr("UNIT"), width: 50, align: "left" }
        ]
        if (showState)
            c.push({ title: qsTr("STATE"), width: 84, align: "left" })
        c.push({ title: qsTr("LAST 10 s"), width: sparkWidth, align: "left" })
        if (showMinMax)
            c.push({ title: qsTr("MIN – MAX SEEN"), width: 150, align: "right" })
        return c
    }
    // Width left for the signal name in a row of the given width.
    function nameWidth(rowWidth) {
        let fixed = 0
        for (let i = 0; i < columns.length; ++i)
            if (columns[i].width > 0)
                fixed += columns[i].width
        fixed += 4                                   // pin button is 4 px wider than its column
        const gaps = (columns.length - 1) * cellSpacing
        return Math.max(72, rowWidth - (Theme.space.lg - 4) - fixed - gaps - Theme.space.lg)
    }

    opacity: active ? 1 : 0
    visible: opacity > 0.01
    enabled: active
    property real rise: active ? 0 : 10
    transform: Translate { y: page.rise }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
    Behavior on rise { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }

    function setSearch(text) { search.text = text; page.model.filterText = text }
    function select(signalId) { page.selectedSignal = signalId; list.positionAtSignal(signalId) }
    function focusSearch() { search.forceActiveFocus(); search.selectAll() }

    Connections {
        target: Session.database
        function onChanged() { page.selectedSignal = -1; search.text = "" }
    }

    RowLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredWidth: 100     // let fillWidth decide; never force the panel off-screen
            Layout.minimumWidth: 300
            spacing: Theme.space.md

            // ------------------------------------------------- search row
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space.sm
                enabled: page.hasDb

                TextField {
                    id: search
                    iconName: "search"
                    placeholderText: qsTr("Search signals or messages")
                    Layout.fillWidth: true
                    Layout.maximumWidth: 300
                    Layout.minimumWidth: 140
                    onTextEdited: searchDebounce.restart()
                    Timer {
                        id: searchDebounce
                        interval: 120
                        onTriggered: page.model.filterText = search.text
                    }
                    Keys.onEscapePressed: { text = ""; page.model.filterText = ""; page.forceActiveFocus() }
                }
                ToolButton {
                    iconName: "chevron-up"
                    text: page.showState ? qsTr("Collapse all") : ""
                    onClicked: page.model.setAllCollapsed(true)
                    ToolTip.visible: hovered && text.length === 0
                    ToolTip.text: qsTr("Collapse all groups")
                    ToolTip.delay: 600
                }
                ToolButton {
                    iconName: "chevron-down"
                    text: page.showState ? qsTr("Expand all") : ""
                    onClicked: page.model.setAllCollapsed(false)
                    ToolTip.visible: hovered && text.length === 0
                    ToolTip.text: qsTr("Expand all groups")
                    ToolTip.delay: 600
                }
                Item { Layout.fillWidth: true; Layout.minimumWidth: 0 }
                Text {
                    visible: page.hasDb && page.listWidth >= 520
                    text: (page.model.filterText.length > 0
                           ? qsTr("%1 of %2 signals").arg(page.model.visibleSignalCount).arg(page.model.signalCount)
                           : qsTr("%1 signals").arg(page.model.signalCount))
                          + (page.model.pinnedCount > 0 ? qsTr(" · %1 pinned").arg(page.model.pinnedCount) : "")
                    color: Theme.textMuted
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.type.small
                    font.features: { "tnum": 1 }
                }
            }

            // ------------------------------------------------- list card
            Card {
                id: listCard
                Layout.fillWidth: true
                Layout.fillHeight: true
                elevation: 1

                TableHeader {
                    id: header
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 1
                    anchors.rightMargin: 1
                    columns: page.columns
                    cellSpacing: page.cellSpacing
                    visible: page.hasDb
                }

                ListView {
                    id: list
                    anchors.top: header.bottom
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.margins: 1
                    anchors.topMargin: 0
                    anchors.bottomMargin: 6
                    clip: true
                    model: page.hasDb ? page.model : null
                    boundsBehavior: Flickable.StopAtBounds
                    // No delegate reuse: the list is small and is reset on every
                    // search keystroke, which reuse does not like.
                    reuseItems: false
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    function positionAtSignal(id) {
                        const row = page.model.rowOfSignal(id)
                        if (row >= 0)
                            positionViewAtIndex(row, ListView.Contain)
                    }

                    delegate: Item {
                        id: row
                        required property int index
                        required property string kind
                        required property string name
                        required property string messageName
                        required property string frameId
                        required property var signalId
                        required property int messageIndex
                        required property var groupSize
                        required property bool collapsed
                        required property bool pinnedSection
                        required property bool pinned
                        required property bool hasValue
                        required property string value
                        required property string unit
                        required property string enumText
                        required property string minSeen
                        required property string maxSeen
                        required property bool outOfRange
                        required property var revision

                        readonly property bool isHeader: kind === "header"
                        readonly property bool selected: !isHeader && signalId === page.selectedSignal

                        width: list.width
                        height: isHeader ? 36 : 34

                        // ---- group header
                        Item {
                            anchors.fill: parent
                            visible: row.isHeader
                            Rectangle {
                                anchors.fill: parent
                                anchors.topMargin: row.index === 0 ? 0 : 4
                                color: headerHover.hovered && !row.pinnedSection ? Theme.hover : Theme.alpha(Theme.textFaint, 0.06)
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: Theme.border
                            }
                            Row {
                                x: Theme.space.lg - 2
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.verticalCenterOffset: row.index === 0 ? 0 : 2
                                spacing: Theme.space.sm
                                Icon {
                                    anchors.verticalCenter: parent.verticalCenter
                                    name: row.pinnedSection ? "pin-filled" : "chevron-down"
                                    size: 14
                                    color: row.pinnedSection ? Theme.accentText : Theme.textMuted
                                    rotation: row.pinnedSection ? 0 : (row.collapsed ? -90 : 0)
                                    Behavior on rotation { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: row.name
                                    color: Theme.text
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.type.body
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: row.frameId.length > 0
                                    text: "0x" + row.frameId
                                    color: Theme.textFaint
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.type.small
                                }
                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    height: 18
                                    width: Math.max(22, countText.implicitWidth + 12)
                                    radius: 9
                                    color: Theme.alpha(Theme.textMuted, 0.12)
                                    Text {
                                        id: countText
                                        anchors.centerIn: parent
                                        text: row.groupSize !== undefined ? row.groupSize : ""
                                        color: Theme.textMuted
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.type.caption
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }
                            HoverHandler { id: headerHover; enabled: row.isHeader && !row.pinnedSection; cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                enabled: row.isHeader && !row.pinnedSection
                                onTapped: page.model.toggleGroup(row.messageIndex)
                            }
                        }

                        // ---- signal row
                        Item {
                            anchors.fill: parent
                            visible: !row.isHeader

                            Rectangle {
                                anchors.fill: parent
                                color: row.selected ? Theme.selection : rowHover.hovered ? Theme.hover : "transparent"
                            }
                            Rectangle {
                                visible: row.selected
                                width: 2
                                height: parent.height
                                color: Theme.accent
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: Theme.space.lg
                                height: 1
                                color: Theme.alpha(Theme.border, 0.6)
                            }
                            HoverHandler { id: rowHover }
                            TapHandler {
                                onTapped: page.selectedSignal = (row.selected ? -1 : row.signalId)
                            }

                            Row {
                                x: Theme.space.lg - 4
                                height: parent.height
                                spacing: page.cellSpacing

                                IconButton {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: page.columns[0].width + 4
                                    height: 26
                                    extent: 26
                                    iconSize: 14
                                    iconName: row.pinned ? "pin-filled" : "pin"
                                    iconColor: row.pinned ? Theme.accentText
                                             : (rowHover.hovered || row.selected ? Theme.textMuted : Theme.alpha(Theme.textFaint, 0.55))
                                    tip: row.pinned ? qsTr("Unpin") : qsTr("Pin to the top")
                                    onClicked: page.model.togglePin(row.signalId)
                                }
                                // name (fills)
                                Item {
                                    width: page.nameWidth(row.width)
                                    height: parent.height
                                    Text {
                                        id: nameText
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Math.min(implicitWidth, parent.width)
                                        text: row.name
                                        elide: Text.ElideRight
                                        color: row.selected ? Theme.accentText : Theme.text
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.type.body
                                        font.weight: Font.Medium
                                    }
                                    Text {
                                        anchors.left: nameText.right
                                        anchors.leftMargin: Theme.space.sm
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        visible: row.pinnedSection
                                        text: row.messageName
                                        elide: Text.ElideRight
                                        color: Theme.textFaint
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.type.small
                                    }
                                }
                                Text {
                                    width: page.columns[2].width
                                    height: parent.height
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                    text: row.value
                                    color: row.outOfRange ? Theme.error : row.hasValue ? Theme.text : Theme.textFaint
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.type.mono + 1
                                    font.weight: Font.DemiBold
                                }
                                Text {
                                    width: page.columns[3].width
                                    height: parent.height
                                    verticalAlignment: Text.AlignVCenter
                                    text: row.unit
                                    elide: Text.ElideRight
                                    color: Theme.textMuted
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.type.small
                                }
                                Item {
                                    visible: page.showState
                                    width: 84
                                    height: parent.height
                                    Chip {
                                        visible: row.enumText.length > 0
                                        anchors.verticalCenter: parent.verticalCenter
                                        tone: "accent"
                                        label: row.enumText
                                        implicitHeight: 20
                                        width: Math.min(implicitWidth, parent.width)
                                    }
                                }
                                Sparkline {
                                    width: page.sparkWidth
                                    height: 22
                                    anchors.verticalCenter: parent.verticalCenter
                                    model: page.model
                                    signalId: row.isHeader ? -1 : row.signalId
                                    revision: row.revision !== undefined ? row.revision : 0
                                    timeUs: page.model.timeUs
                                    stepped: row.enumText.length > 0 || row.unit.length === 0
                                    color: row.outOfRange ? Theme.error : Theme.accent
                                    areaColor: Theme.alpha(row.outOfRange ? Theme.error : Theme.accent, 0.16 + 0.06 * Theme.t)
                                    lineWidth: 1.4
                                }
                                Text {
                                    visible: page.showMinMax
                                    width: 150
                                    height: parent.height
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                    text: row.hasValue ? row.minSeen + "  –  " + row.maxSeen : ""
                                    color: Theme.textMuted
                                    font.family: Theme.fontMono
                                    font.pixelSize: Theme.type.small
                                }
                            }
                        }
                    }
                }

                // No database loaded
                Column {
                    anchors.centerIn: parent
                    width: parent.width
                    spacing: Theme.space.lg
                    visible: !page.hasDb
                    EmptyState {
                        width: parent.width
                        iconName: "database"
                        title: qsTr("No signal database loaded")
                        message: qsTr("Load the demo database or open a DBC file to decode frames into named signals with units.")
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: Theme.space.sm
                        Button {
                            variant: "primary"
                            iconName: "database"
                            text: qsTr("Load demo database")
                            onClicked: Session.database.loadDemo()
                        }
                        Button {
                            iconName: "folder"
                            text: qsTr("Open DBC file…")
                            onClicked: ApplicationWindow.window.openDatabaseDialog()
                        }
                    }
                }
                // Search without matches
                EmptyState {
                    anchors.centerIn: parent
                    width: parent.width
                    visible: page.hasDb && list.count === 0
                    iconName: "search"
                    title: qsTr("No signals match “%1”").arg(page.model.filterText)
                    message: qsTr("Search looks at signal names, message names and units.")
                    shortcut: "Esc"
                    shortcutLabel: qsTr("to clear the search")
                }
            }
        }

        // ------------------------------------------------- detail panel
        SignalDetail {
            id: detail
            Layout.fillHeight: true
            Layout.preferredWidth: page.selectedSignal >= 0 ? Math.round(Math.min(384, Math.max(300, page.width * 0.4))) : 0
            visible: Layout.preferredWidth > 1
            clip: true
            signalId: page.selectedSignal
            onCloseRequested: page.selectedSignal = -1
            Behavior on Layout.preferredWidth { NumberAnimation { duration: Theme.motion.slow; easing.type: Theme.motion.easing } }
        }
    }
}
