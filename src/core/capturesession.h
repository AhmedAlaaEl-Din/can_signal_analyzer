#pragma once

#include "busstatistics.h"
#include "canframe.h"
#include "framefilter.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <deque>
#include <vector>

namespace can {

class FrameModel;
class MessageOverviewModel;
class SignalDatabase;
class SignalListModel;
class SimulatedSource;

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
    SimulatedSource *source() const { return m_source; }

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

private:
    void onFrames(const can::CanFrameBatch &batch);
    void publish(const std::vector<CanFrame> &frames);
    void updateStatistics();
    QObject *frameModelObject() const;
    QObject *overviewModelObject() const;
    QObject *signalModelObject() const;
    QObject *databaseObject() const;
    qint64 now() const;
    void applyDatabase();

    SimulatedSource *m_source;
    FrameModel *m_frames;
    MessageOverviewModel *m_overview;
    SignalListModel *m_signalModel;
    SignalDatabase *m_database;
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
