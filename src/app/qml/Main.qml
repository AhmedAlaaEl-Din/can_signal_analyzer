import QtQuick
import QtQuick.Controls
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

    header: AppToolBar {
        id: toolBar
    }

    footer: StatusBar {
        id: statusBar
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
            spacing: Theme.space.lg

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
            }
            Item { Layout.fillWidth: true }
            Text {
                text: window.currentTab === 0 ? qsTr("Live frame trace, newest at the bottom")
                                              : qsTr("One row per identifier · click a header to sort")
                color: Theme.textFaint
                font.family: Theme.fontSans
                font.pixelSize: Theme.type.small
                elide: Text.ElideRight
                Layout.maximumWidth: 360
                Layout.alignment: Qt.AlignVCenter
            }
          }
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
        }
    }

    // ---------------------------------------------------------------- keys
    Shortcut { sequences: ["F5"]; onActivated: Session.toggleRunning() }
    Shortcut { sequences: ["F6", "Space"]; onActivated: Session.togglePause() }
    Shortcut { sequence: "Ctrl+L"; onActivated: Session.clear() }
    Shortcut { sequence: "Ctrl+T"; onActivated: Theme.toggle() }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.currentTab = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.currentTab = 1 }
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: { window.currentTab = 0; tracePage.focusFilter() }
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
                tracePage.forceActiveFocus()
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
        default: console.warn("unknown demo action", name)
        }
    }
}
