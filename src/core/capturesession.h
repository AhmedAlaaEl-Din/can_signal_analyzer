#pragma once

#include "busstatistics.h"
#include "canframe.h"
#include "framefilter.h"
#include "framesearch.h"
#include "logsummary.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include <memory>

#include <deque>
#include <vector>

namespace can {

class FrameModel;
class MessageOverviewModel;
class SignalDatabase;
class SignalListModel;
class TimeSeriesStore;
class PlotModel;
class PlotController;
class SimulatedSource;
class ReplaySource;
class LogRecorder;
class MarkerStore;
class FrameSource;

// Owns the frame source and the models and moves frames between them.
// Frames arrive from the source thread at any rate; they are collected and
// pushed into the models in one batch every kFlushIntervalMs, so the UI does a
// bounded amount of work per frame it renders, regardless of the bus rate.
//
// Pause freezes the views but not the capture: frames keep arriving and are
// parked (bounded by the trace capacity) until resume.
class CaptureSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ isPaused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(int targetRate READ targetRate WRITE setTargetRate NOTIFY targetRateChanged)
    Q_PROPERTY(bool burstsEnabled READ burstsEnabled WRITE setBurstsEnabled NOTIFY burstsEnabledChanged)
    Q_PROPERTY(double nominalRate READ nominalRate NOTIFY modeChanged)
    Q_PROPERTY(QString sourceDescription READ sourceDescription NOTIFY modeChanged)
    Q_PROPERTY(bool vehicleMode READ vehicleMode WRITE setVehicleMode NOTIFY modeChanged)
    Q_PROPERTY(int bitrate READ bitrate CONSTANT)

    Q_PROPERTY(double framesPerSecond READ framesPerSecond NOTIFY statisticsChanged)
    Q_PROPERTY(double busLoad READ busLoad NOTIFY statisticsChanged)
    Q_PROPERTY(qulonglong totalFrames READ totalFrames NOTIFY statisticsChanged)
    Q_PROPERTY(int pendingWhilePaused READ pendingWhilePaused NOTIFY statisticsChanged)

    Q_PROPERTY(QString idFilter READ idFilter WRITE setIdFilter NOTIFY filterChanged)
    Q_PROPERTY(QString dlcFilter READ dlcFilter WRITE setDlcFilter NOTIFY filterChanged)
    Q_PROPERTY(int formatFilter READ formatFilter WRITE setFormatFilter NOTIFY filterChanged)
    Q_PROPERTY(QString idFilterError READ idFilterError NOTIFY filterChanged)
    Q_PROPERTY(QString dlcFilterError READ dlcFilterError NOTIFY filterChanged)
    Q_PROPERTY(bool filterActive READ filterActive NOTIFY filterChanged)

    Q_PROPERTY(QObject *frameModel READ frameModelObject CONSTANT)
    Q_PROPERTY(QObject *overviewModel READ overviewModelObject CONSTANT)
    Q_PROPERTY(QObject *signalModel READ signalModelObject CONSTANT)
    Q_PROPERTY(QObject *database READ databaseObject CONSTANT)
    Q_PROPERTY(QObject *timeSeries READ timeSeriesObject CONSTANT)
    Q_PROPERTY(QObject *plotModel READ plotModelObject CONSTANT)
    Q_PROPERTY(QObject *plot READ plotControllerObject CONSTANT)
    Q_PROPERTY(QObject *replay READ replayObject CONSTANT)
    Q_PROPERTY(QObject *recorder READ recorderObject CONSTANT)
    Q_PROPERTY(QObject *markers READ markersObject CONSTANT)

    // Replay: the same pipeline fed from a log file instead of the simulator.
    Q_PROPERTY(bool replayMode READ replayMode NOTIFY replayModeChanged)
    Q_PROPERTY(bool summaryBusy READ summaryBusy NOTIFY summaryBusyChanged)

    // Trace search (see FrameSearch for the syntax).
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchChanged)
    Q_PROPERTY(bool searchValid READ searchValid NOTIFY searchChanged)
    Q_PROPERTY(QString searchDescription READ searchDescription NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)

public:
    static constexpr int kFlushIntervalMs = 30;
    static constexpr int kStatsIntervalMs = 250;

    explicit CaptureSession(SimulatedSource *source, QObject *parent = nullptr);
    ~CaptureSession() override;

    bool isRunning() const;
    bool isPaused() const { return m_paused; }
    void setPaused(bool paused);
    int targetRate() const;
    void setTargetRate(int rate);
    bool burstsEnabled() const;
    void setBurstsEnabled(bool enabled);
    double nominalRate() const;
    QString sourceDescription() const;
    bool vehicleMode() const;
    void setVehicleMode(bool vehicle);
    int bitrate() const { return m_stats.bitrate(); }

    double framesPerSecond() const { return m_fps; }
    double busLoad() const { return m_busLoad; }
    qulonglong totalFrames() const { return m_stats.totalFrames(); }
    int pendingWhilePaused() const { return static_cast<int>(m_parked.size()); }

    QString idFilter() const { return m_idFilter; }
    void setIdFilter(const QString &text);
    QString dlcFilter() const { return m_dlcFilter; }
    void setDlcFilter(const QString &text);
    int formatFilter() const { return static_cast<int>(m_filter.format()); }
    void setFormatFilter(int format);
    QString idFilterError() const { return m_idFilterError; }
    QString dlcFilterError() const { return m_dlcFilterError; }
    bool filterActive() const { return m_filter.isActive(); }

    FrameModel *frameModel() const { return m_frames; }
    MessageOverviewModel *overviewModel() const { return m_overview; }
    SignalListModel *signalModel() const { return m_signalModel; }
    SignalDatabase *database() const { return m_database; }
    TimeSeriesStore *timeSeries() const { return m_store; }
    PlotModel *plotModel() const { return m_plotModel; }
    PlotController *plotController() const { return m_plot; }
    SimulatedSource *source() const { return m_source; }
    ReplaySource *replay() const { return m_replay; }
    LogRecorder *recorder() const { return m_recorder; }
    MarkerStore *markers() const { return m_markers; }
    FrameSource *activeSource() const;

    bool replayMode() const { return m_replayMode; }
    bool summaryBusy() const { return m_summaryBusy; }
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);
    bool searchValid() const { return m_search.isValid(); }
    QString searchDescription() const { return m_search.description(); }
    QString searchError() const { return m_search.error(); }

    // ---- recording
    Q_INVOKABLE QString defaultLogFolder() const;
    Q_INVOKABLE QString defaultLogName() const;
    Q_INVOKABLE bool startRecording(const QUrl &file);
    bool startRecordingPath(const QString &path);
    Q_INVOKABLE void stopRecording();
    // Fixed header start time (deterministic demo capture); invalid = now.
    void setRecordingStartOverride(const QDateTime &utc) { m_recordStartOverride = utc; }

    // ---- replay
    // Opens a .canlog (or imports a .csv) and switches to replay mode.
    Q_INVOKABLE bool openLog(const QUrl &file);
    bool openLogPath(const QString &path);
    Q_INVOKABLE void closeLog();       // back to the live simulator (stopped)
    Q_INVOKABLE void seekReplay(qint64 timeUs);
    Q_INVOKABLE void addMarker(const QString &name = {});
    Q_INVOKABLE bool jumpToMarker(bool forward);
    Q_INVOKABLE bool exportLogCsv(const QUrl &file);
    Q_INVOKABLE void requestSummary();  // summaryReady() when done
    Q_INVOKABLE bool exportSummaryCsv(const QUrl &file);
    Q_INVOKABLE QString localPath(const QUrl &url) const;
    Q_INVOKABLE bool fileExists(const QString &path) const;

    // ---- search
    Q_INVOKABLE int findNext(int fromRow, bool forward);
    Q_INVOKABLE int matchCount() const;

    // Demo setup in one step: demo database, default plot layout, simulator running.
    Q_INVOKABLE void loadDemoSetup();

    // Manual time (deterministic capture): timers are not used; the owner calls
    // manualTick() after advancing the source, with its own notion of "now".
    void setManualTime(bool manual);
    void manualTick(qint64 nowMs);

public slots:
    void start();
    void stop();
    void toggleRunning();
    void togglePause();
    void clear();
    void clearFilter();
    // Pushes pending frames into the models right now (normally timer driven).
    void flush();

signals:
    void runningChanged();
    void pausedChanged();
    void targetRateChanged();
    void burstsEnabledChanged();
    void statisticsChanged();
    void filterChanged();
    void cleared();
    void modeChanged();
    void replayModeChanged();
    void summaryBusyChanged();
    void searchChanged();
    void summaryReady(const QVariantMap &summary);
    // Short user-facing message (toast); tone is "ok" or "error".
    void notice(const QString &text, const QString &tone);

private:
    void onFrames(const can::CanFrameBatch &batch);
    void publish(const std::vector<CanFrame> &frames);
    void updateStatistics();
    QObject *frameModelObject() const;
    QObject *overviewModelObject() const;
    QObject *signalModelObject() const;
    QObject *databaseObject() const;
    QObject *timeSeriesObject() const;
    QObject *plotModelObject() const;
    QObject *plotControllerObject() const;
    QObject *replayObject() const;
    QObject *recorderObject() const;
    QObject *markersObject() const;
    void clearViews();
    void setReplayMode(bool on);
    qint64 now() const;
    void applyDatabase();

    SimulatedSource *m_source;
    FrameModel *m_frames;
    MessageOverviewModel *m_overview;
    SignalListModel *m_signalModel;
    SignalDatabase *m_database;
    TimeSeriesStore *m_store;
    PlotModel *m_plotModel;
    PlotController *m_plot;
    ReplaySource *m_replay;
    LogRecorder *m_recorder;
    MarkerStore *m_markers;
    bool m_replayMode = false;
    bool m_summaryBusy = false;
    LogSummary m_lastSummary;
    QString m_tempCanlog; // CSV imports are converted into this file
    QString m_openedPath; // the file the user opened (CSV or canlog)
    QDateTime m_recordStartOverride;
    qint64 m_lastRecorderFlushMs = 0;
    QString m_searchText;
    FrameSearch m_search;
    bool m_manualTime = false;
    qint64 m_manualNowMs = 0;
    BusStatistics m_stats;
    QElapsedTimer m_clock;
    QTimer m_flushTimer;
    QTimer m_statsTimer;
    std::vector<CanFrame> m_incoming;
    std::deque<CanFrame> m_parked; // frames received while paused
    bool m_paused = false;
    double m_fps = 0;
    double m_busLoad = 0;

    FrameFilter m_filter;
    QString m_idFilter;
    QString m_dlcFilter;
    QString m_idFilterError;
    QString m_dlcFilterError;
};

} // namespace can
