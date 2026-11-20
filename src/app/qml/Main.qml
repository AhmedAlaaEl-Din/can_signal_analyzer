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
    // In the deterministic --demo capture the real mouse must not leak into the
    // frames (hover crosshair, hover-only controls).
    property bool demoMode: false   // set by the DemoDirector
    // Show message names instead of hex IDs (only meaningful with a database).
    property bool namesPreferred: true
    readonly property bool showNames: namesPreferred && Session.database.loaded

    function openDatabaseDialog() { dbDialog.open() }
    function openLogDialog() { logDialog.open() }
    function showToast(text, tone) { toast.show(text, tone) }
    // "Load demo": one click to a useful screen (database, plots, running).
    function startDemo() {
        if (Session.replayMode) {
            Session.database.loadDemo()
            if (Session.plotModel.count === 0)
                Session.plotModel.applyDemoLayout()
        } else {
            Session.loadDemoSetup()
        }
        window.currentTab = 3
    }
    function requestRecord() {
        if (Session.recorder.recording) {
            Session.stopRecording()
            return
        }
        recordDialog.currentFolder = Settings.value("logs/folder", "file:///" + Session.defaultLogFolder())
        recordDialog.selectedFile = recordDialog.currentFolder + "/" + Session.defaultLogName()
        recordDialog.open()
    }
    function openSummary() {
        summaryDialog.summary = ({})
        summaryDialog.open()
        Session.requestSummary()
    }
    function openFile(url) {
        const path = Session.localPath(url).toLowerCase()
        if (path.endsWith(".dbc")) {
            Session.database.loadFile(url)
        } else if (path.endsWith(".canlog") || path.endsWith(".csv")) {
            if (Session.openLog(url))
                Settings.setValue("logs/folder", String(url).replace(/\/[^\/]*$/, ""))
        } else {
            toast.show(qsTr("Drop a .canlog, .csv or .dbc file."), "error")
        }
    }

    header: AppToolBar {
        id: toolBar
        onRecordRequested: window.requestRecord()
        onOpenLogRequested: window.openLogDialog()
        onAboutRequested: aboutDialog.open()
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
    FileDialog {
        id: logDialog
        title: qsTr("Open a log for replay")
        nameFilters: [qsTr("CAN logs (*.canlog *.csv)"), qsTr("CAN Analyzer logs (*.canlog)"), qsTr("CSV frames (*.csv)")]
        onAccepted: window.openFile(selectedFile)
    }
    FileDialog {
        id: recordDialog
        title: qsTr("Record to")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "canlog"
        nameFilters: [qsTr("CAN Analyzer logs (*.canlog)")]
        onAccepted: {
            if (Session.startRecording(selectedFile))
                Settings.setValue("logs/folder", String(currentFolder))
        }
    }
    FileDialog {
        id: logCsvDialog
        title: qsTr("Export the log as CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV files (*.csv)")]
        onAccepted: Session.exportLogCsv(selectedFile)
    }
    FileDialog {
        id: summaryCsvDialog
        title: qsTr("Export the summary as CSV")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "csv"
        nameFilters: [qsTr("CSV files (*.csv)")]
        onAccepted: Session.exportSummaryCsv(selectedFile)
    }

    LogSummaryDialog {
        id: summaryDialog
        onExportRequested: summaryCsvDialog.open()
    }
    AboutDialog { id: aboutDialog }

    Connections {
        target: Session
        function onNotice(text, tone) { toast.show(text, tone) }
        function onSummaryReady(summary) { summaryDialog.summary = summary }
        function onReplayModeChanged() {
            // A replay starts in the Trace or Plots tab, wherever the user is.
            if (Session.replayMode && window.currentTab === 1)
                window.currentTab = 0
        }
    }

    // ---- persistence (theme, window, tab, database, filters, plot layout) ----
    function restoreSettings() {
        if (!Settings.enabled)
            return
        Theme.dark = Settings.value("ui/dark", "true") === "true" || Settings.value("ui/dark", true) === true
        const w = Number(Settings.value("window/width", 0)), h = Number(Settings.value("window/height", 0))
        if (w >= minimumWidth && h >= minimumHeight) {
            window.width = w
            window.height = h
        }
        if (Settings.value("window/maximized", "false") === "true")
            window.visibility = Window.Maximized
        window.namesPreferred = Settings.value("ui/names", "true") !== "false"
        const db = String(Settings.value("database/path", ""))
        if (db === "demo") {
            Session.database.loadDemo()
        } else if (db.length > 0) {
            if (Session.fileExists(db))
                Session.database.loadFile("file:///" + db)
            else
                toast.show(qsTr("The last database (%1) was not found; starting without it.").arg(db.split(/[\\/]/).pop()), "error")
        }
        const layout = String(Settings.value("plots/layout", ""))
        if (layout.length > 0 && Session.database.loaded)
            Session.plotModel.restoreLayout(layout)
        Session.plot.windowSeconds = Number(Settings.value("plots/window", 10))
        tracePage.setIdFilterText(String(Settings.value("trace/idFilter", "")))
        Session.dlcFilter = String(Settings.value("trace/dlcFilter", ""))
        Session.formatFilter = Number(Settings.value("trace/formatFilter", 0))
        window.currentTab = Math.max(0, Math.min(3, Number(Settings.value("ui/tab", 0))))
    }
    function saveSettings() {
        if (!Settings.enabled)
            return
        Settings.setValue("ui/dark", Theme.dark ? "true" : "false")
        Settings.setValue("window/maximized", window.visibility === Window.Maximized ? "true" : "false")
        if (window.visibility !== Window.Maximized) {
            Settings.setValue("window/width", window.width)
            Settings.setValue("window/height", window.height)
        }
        Settings.setValue("ui/names", window.namesPreferred ? "true" : "false")
        Settings.setValue("ui/tab", window.currentTab)
        Settings.setValue("database/path", !Session.database.loaded ? ""
                          : Session.database.isDemo ? "demo" : Session.database.path)
        Settings.setValue("plots/layout", Session.database.loaded ? Session.plotModel.saveLayout() : "")
        Settings.setValue("plots/window", Session.plot.windowSeconds)
        Settings.setValue("trace/idFilter", Session.idFilter)
        Settings.setValue("trace/dlcFilter", Session.dlcFilter)
        Settings.setValue("trace/formatFilter", Session.formatFilter)
        Settings.sync()
    }
    Component.onCompleted: restoreSettings()
    onClosing: saveSettings()

    // Drop a log or a database anywhere on the window.
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0) {
                window.openFile(drop.urls[0])
                drop.accept()
            }
        }
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
                    TabButton {
                        text: qsTr("Plots")
                        iconName: "plot"
                        badge: Session.plotModel.seriesCount > 0 ? Session.plotModel.seriesCount.toString() : ""
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
                onDemoRequested: window.startDemo()
                onOpenLogRequested: window.openLogDialog()
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
            PlotsPage {
                id: plotsPage
                anchors.fill: parent
                active: window.currentTab === 3
                demoMode: window.demoMode
            }
        }

        ReplayBar {
            id: replayBar
            Layout.fillWidth: true
            Layout.preferredHeight: Session.replayMode ? implicitHeight : 0
            visible: Session.replayMode
            onSummaryRequested: window.openSummary()
            onExportRequested: logCsvDialog.open()
        }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.space.xl + (Session.replayMode ? 56 : 0)
        z: 100
    }

    // ---------------------------------------------------------------- keys
    Shortcut { sequences: ["F5"]; onActivated: Session.toggleRunning() }
    Shortcut { sequences: ["F6", "Space"]; onActivated: Session.togglePause() }
    Shortcut { sequence: "Ctrl+R"; enabled: !Session.replayMode; onActivated: window.requestRecord() }
    Shortcut { sequence: "Ctrl+Shift+O"; onActivated: window.openLogDialog() }
    Shortcut { sequence: "F1"; onActivated: aboutDialog.open() }
    // Replay transport
    Shortcut { sequence: "."; enabled: Session.replayMode; onActivated: Session.replay.stepFrame() }
    Shortcut { sequence: "M"; enabled: Session.replayMode; onActivated: Session.addMarker("") }
    Shortcut { sequence: "]"; enabled: Session.replayMode; onActivated: Session.jumpToMarker(true) }
    Shortcut { sequence: "["; enabled: Session.replayMode; onActivated: Session.jumpToMarker(false) }
    Shortcut { sequence: "Ctrl+Home"; enabled: Session.replayMode; onActivated: Session.seekReplay(Session.replay.startUs) }
    Shortcut { sequence: "Ctrl+End"; enabled: Session.replayMode; onActivated: Session.seekReplay(Session.replay.endUs) }
    // Trace find
    Shortcut { sequence: "F3"; onActivated: { window.currentTab = 0; tracePage.findNext(true) } }
    Shortcut { sequence: "Shift+F3"; onActivated: { window.currentTab = 0; tracePage.findNext(false) } }
    Shortcut { sequence: "Ctrl+L"; onActivated: Session.clear() }
    Shortcut { sequence: "Ctrl+T"; onActivated: Theme.toggle() }
    Shortcut { sequence: "Ctrl+O"; onActivated: window.openDatabaseDialog() }
    Shortcut { sequence: "Ctrl+N"; enabled: Session.database.loaded; onActivated: window.namesPreferred = !window.namesPreferred }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.currentTab = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.currentTab = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.currentTab = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: window.currentTab = 3 }
    Shortcut {
        sequences: [StandardKey.Find]
        onActivated: {
            if (window.currentTab === 2) {
                signalsPage.focusSearch()
            } else {
                window.currentTab = 0
                tracePage.openFind()
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
            else if (window.currentTab === 3 && Session.plot.hasCursors)
                Session.plot.clearCursors()
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
        // ---- plots (arguments are "a@b@c" strings) ----
        case "plotAdd": {
            const p = String(arg).split("@"), n = p[0].split(".")
            plotsPage.addSignalByName(n[0], n[1], p.length > 1 ? Number(p[1]) : -1)
            break
        }
        case "plotWindow": Session.plot.windowSeconds = Number(arg); Session.plot.follow(); break
        case "plotZoom": { const p = String(arg).split("@"); Session.plot.zoomAt(Number(p[1]), Number(p[0])); break }
        case "plotPan": Session.plot.panByFraction(Number(arg)); break
        case "plotFollow": Session.plot.follow(); break
        case "plotCursors": if (arg === "clear") Session.plot.clearCursors(); else Session.plot.placeCursors(); break
        case "plotCursor": {
            const p = String(arg).split("@")
            Session.plot.setCursor(Number(p[0]), Session.plot.viewStartUs + Number(p[1]) * (Session.plot.viewEndUs - Session.plot.viewStartUs))
            break
        }
        case "plotToggle": {
            const p = String(arg).split("@"), n = p[1].split(".")
            Session.plotModel.toggleVisible(Number(p[0]), Session.signalModel.signalIdByName(n[0], n[1]))
            break
        }
        case "plotThreshold": { const p = String(arg).split("@"); Session.plotModel.addThreshold(Number(p[0]), Number(p[1])); break }
        case "plotThresholdMove": {
            const p = String(arg).split("@")
            Session.plotModel.setThreshold(Number(p[0]), Number(p[1]), Number(p[2]))
            break
        }
        case "plotExportPng": plotsPage.exportPng(arg); break
        case "plotExportCsv": plotsPage.exportCsv(arg); break
        case "plotHover": plotsPage.hoverX = Number(arg) < 0 ? -1 : Number(arg) * plotsPage.plotWidth; break
        // ---- phase 4: logging, replay, search, dialogs ----
        case "startDemo": window.startDemo(); break
        case "speed": Session.replay.speed = Number(arg); break
        case "loop": Session.replay.loop = (arg === "on"); break
        case "seekFraction": Session.seekReplay(Session.replay.startUs + Number(arg) * Session.replay.durationUs); break
        case "seekHover": {
            const f = Number(arg)
            replayBar.seekBar.demoHover = f >= 0
            replayBar.seekBar.hoverUs = f < 0 ? -1 : Session.replay.startUs + f * Session.replay.durationUs
            break
        }
        case "marker": Session.addMarker(String(arg)); break
        case "markerJump": Session.jumpToMarker(arg === "next"); break
        case "step": Session.replay.stepFrame(); break
        case "playPause": Session.togglePause(); break
        case "find": window.currentTab = 0; tracePage.setSearch(String(arg)); break
        case "findNext": tracePage.findNext(arg !== "prev"); break
        case "findClose": tracePage.closeFind(); break
        case "summary": window.openSummary(); break
        case "summaryClose": summaryDialog.close(); break
        case "about": aboutDialog.open(); break
        case "aboutClose": aboutDialog.close(); break
        case "toast": { const p = String(arg).split("@"); toast.show(p[0], p[1] || "ok"); break }
        default: console.warn("unknown demo action", name)
        }
    }
}
