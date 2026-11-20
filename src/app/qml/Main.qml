import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import CanUi
import CanAnalyzer.Core

ApplicationWindow {
    id: window

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: qsTr("CAN Analyzer")
    color: Theme.bg

    font.family: Theme.fontSans
    font.pixelSize: Theme.type.body

    property int currentTab: 0
    // Show message names instead of hex IDs (only meaningful with a database).
    property bool namesPreferred: true
    readonly property bool showNames: namesPreferred && Session.database.loaded

    function openDatabaseDialog() { dbDialog.open() }

    header: AppToolBar {
        id: toolBar
    }

    footer: StatusBar {
        id: statusBar
    }

    FileDialog {
        id: dbDialog
        title: qsTr("Open signal database")
        nameFilters: [qsTr("CAN databases (*.dbc)"), qsTr("All files (*)")]
        onAccepted: Session.database.loadFile(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space.xl
        anchors.rightMargin: Theme.space.xl
        anchors.topMargin: Theme.space.sm
        anchors.bottomMargin: Theme.space.lg
        spacing: Theme.space.md

        Item {
            Layout.fillWidth: true
            implicitHeight: tabs.implicitHeight
            Divider {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
            }
            RowLayout {
                anchors.fill: parent
                spacing: Theme.space.md

                TabBar {
                    id: tabs
                    currentIndex: window.currentTab
                    onCurrentIndexChanged: window.currentTab = currentIndex
                    TabButton {
                        text: qsTr("Trace")
                        iconName: "list"
                        width: implicitWidth
                    }
                    TabButton {
                        text: qsTr("Messages")
                        iconName: "grid"
                        badge: Session.overviewModel.count > 0 ? Session.overviewModel.count.toString() : ""
                        width: implicitWidth
                    }
                    TabButton {
                        text: qsTr("Signals")
                        iconName: "signal"
                        badge: Session.database.loaded ? Session.signalModel.signalCount.toString() : ""
                        width: implicitWidth
                    }
                }
                Item { Layout.fillWidth: true }
                DatabaseBar {
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        DatabaseErrorBanner {
            Layout.fillWidth: true
        }

        Item {
            id: pages
            Layout.fillWidth: true
            Layout.fillHeight: true

            TracePage {
                id: tracePage
                anchors.fill: parent
                active: window.currentTab === 0
            }
            MessagesPage {
                id: messagesPage
                anchors.fill: parent
                active: window.currentTab === 1
            }
            SignalsPage {
                id: signalsPage
                anchors.fill: parent
                active: window.currentTab === 2
            }
        }
    }

    // ---------------------------------------------------------------- keys
    Shortcut { sequences: ["F5"]; onActivated: Session.toggleRunning() }
    Shortcut { sequences: ["F6", "Space"]; onActivated: Session.togglePause() }
    Shortcut { sequence: "Ctrl+L"; onActivated: Session.clear() }
    Shortcut { sequence: "Ctrl+T"; onActivated: Theme.toggle() }
    Shortcut { sequence: "Ctrl+O"; onActivated: window.openDatabaseDialog() }
    Shortcut { sequence: "Ctrl+N"; enabled: Session.database.loaded; onActivated: window.namesPreferred = !window.namesPreferred }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.currentTab = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.currentTab = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.currentTab = 2 }
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: {
            if (window.currentTab === 2) {
                signalsPage.focusSearch()
            } else {
                window.currentTab = 0
                tracePage.focusFilter()
            }
        }
    }
    Shortcut {
        sequence: "End"
        enabled: window.currentTab === 0
        onActivated: tracePage.jumpToLive()
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (window.activeFocusItem && window.activeFocusItem.hasOwnProperty("text"))
                pages.forceActiveFocus()
            else if (window.currentTab === 2 && signalsPage.selectedSignal >= 0)
                signalsPage.selectedSignal = -1
            else
                Session.clearFilter()
        }
    }

    // ------------------------------------------------------- demo scripting
    // Called from C++ (DemoDirector) to drive a scripted scene.
    function demoAction(name, arg) {
        switch (name) {
        case "theme": Theme.dark = (arg === "dark"); break
        case "tab": window.currentTab = Number(arg); break
        case "idFilter": tracePage.setIdFilterText(String(arg)); break
        case "clearFilter": tracePage.setIdFilterText(""); Session.clearFilter(); break
        case "scrollUp": tracePage.scrollUpBy(Number(arg)); break
        case "jumpToLive": tracePage.jumpToLive(); break
        case "selectRow": tracePage.selectVisibleRow(Number(arg)); break
        case "sort": Session.overviewModel.sortBy(Number(arg)); break
        case "hoverMessagesRow": messagesPage.hoverRow = Number(arg); break
        case "names": window.namesPreferred = (arg === "on"); break
        case "loadDemoDb": Session.database.loadDemo(); break
        case "loadDbFile": Session.database.loadFile(arg); break
        case "search": signalsPage.setSearch(String(arg)); break
        case "pin": {
            const parts = String(arg).split(".")
            const id = Session.signalModel.signalIdByName(parts[0], parts[1])
            if (id >= 0 && !Session.signalModel.isPinned(id))
                Session.signalModel.togglePin(id)
            break
        }
        case "selectSignal": {
            const p = String(arg).split(".")
            signalsPage.select(Session.signalModel.signalIdByName(p[0], p[1]))
            break
        }
        case "closeSignal": signalsPage.selectedSignal = -1; break
        case "expandMessage": messagesPage.toggleExpanded(String(arg)); break
        default: console.warn("unknown demo action", name)
        }
    }
}
