#include "capturesession.h"

#include "framemodel.h"
#include "messageoverviewmodel.h"
#include "signaldatabase.h"
#include "signallistmodel.h"
#include "plotcontroller.h"
#include "plotmodel.h"
#include "timeseriesstore.h"
#include "simulatedsource.h"
#include "replaysource.h"
#include "logrecorder.h"
#include "markerstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace can {

CaptureSession::CaptureSession(SimulatedSource *source, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_frames(new FrameModel(this))
    , m_overview(new MessageOverviewModel(this))
    , m_signalModel(new SignalListModel(this))
    , m_database(new SignalDatabase(this))
    , m_store(new TimeSeriesStore(this))
    , m_plotModel(new PlotModel(this))
    , m_plot(new PlotController(this))
    , m_replay(new ReplaySource(this))
    , m_recorder(new LogRecorder(this))
    , m_markers(new MarkerStore(this))
{
    m_signalModel->setStore(m_store);
    m_plot->setStore(m_store);
    connect(m_database, &SignalDatabase::changed, this, &CaptureSession::applyDatabase);
    Q_ASSERT(m_source);
    if (!m_source->parent())
        m_source->setParent(this);
    m_clock.start();

    connect(m_source, &FrameSource::framesReady, this, [this](const CanFrameBatch &batch) {
        if (!m_replayMode)
            onFrames(batch);
    });
    connect(m_replay, &FrameSource::framesReady, this, [this](const CanFrameBatch &batch) {
        if (!m_replayMode)
            return;
        onFrames(batch);
        if (!m_replay->isRunning())
            flush(); // single steps while paused show up immediately
    });
    connect(m_replay, &ReplaySource::discontinuity, this, &CaptureSession::clearViews);
    connect(m_replay, &ReplaySource::prerollReady, this, [this](const CanFrameBatch &batch) {
        if (!m_replayMode)
            return;
        // History before a seek target: shown, but not counted as bus traffic.
        std::vector<CanFrame> frames(batch.cbegin(), batch.cend());
        publish(frames);
    });
    connect(m_recorder, &LogRecorder::errorChanged, this, [this] {
        if (!m_recorder->lastError().isEmpty())
            emit notice(m_recorder->lastError(), QStringLiteral("error"));
    });
    auto onRunning = [this](bool running) {
        if (running && !m_manualTime) {
            m_flushTimer.start();
            m_statsTimer.start();
        } else if (running) {
            // manual time: the owner drives flush() / statistics
        } else {
            flush();
            m_flushTimer.stop();
            // keep the stats timer briefly so the rate decays to 0 visibly
        }
        emit runningChanged();
    };
    connect(m_source, &FrameSource::runningChanged, this, [this, onRunning](bool running) {
        if (!m_replayMode)
            onRunning(running);
    });
    connect(m_replay, &FrameSource::runningChanged, this, [this, onRunning](bool running) {
        if (m_replayMode)
            onRunning(running);
    });
    connect(m_source, &SimulatedSource::targetRateChanged, this, &CaptureSession::targetRateChanged);
    connect(m_source, &SimulatedSource::burstsEnabledChanged, this, &CaptureSession::burstsEnabledChanged);
    connect(m_source, &SimulatedSource::modeChanged, this, &CaptureSession::modeChanged);

    m_flushTimer.setInterval(kFlushIntervalMs);
    m_flushTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_flushTimer, &QTimer::timeout, this, &CaptureSession::flush);
    m_statsTimer.setInterval(kStatsIntervalMs);
    connect(&m_statsTimer, &QTimer::timeout, this, &CaptureSession::updateStatistics);
}

CaptureSession::~CaptureSession()
{
    m_recorder->stop();
    m_source->disconnect(this);
    m_replay->disconnect(this);
    m_source->stop();
    m_replay->stop();
    if (!m_tempCanlog.isEmpty())
        QFile::remove(m_tempCanlog);
}

FrameSource *CaptureSession::activeSource() const
{
    return m_replayMode ? static_cast<FrameSource *>(m_replay) : static_cast<FrameSource *>(m_source);
}

bool CaptureSession::isRunning() const
{
    return activeSource()->isRunning();
}

void CaptureSession::start()
{
    activeSource()->start();
}

void CaptureSession::stop()
{
    activeSource()->stop();
}

void CaptureSession::toggleRunning()
{
    isRunning() ? stop() : start();
}

void CaptureSession::setPaused(bool paused)
{
    if (m_replayMode) { // in replay, pause means pause the playback
        paused ? m_replay->stop() : m_replay->start();
        return;
    }
    if (paused == m_paused)
        return;
    m_paused = paused;
    if (!m_paused && !m_parked.empty()) {
        // Resume: hand everything that arrived meanwhile to the views at once.
        std::vector<CanFrame> parked(m_parked.begin(), m_parked.end());
        m_parked.clear();
        publish(parked);
        emit statisticsChanged();
    }
    emit pausedChanged();
}

void CaptureSession::togglePause()
{
    if (m_replayMode) {
        m_replay->isRunning() ? m_replay->stop() : m_replay->start();
        return;
    }
    setPaused(!m_paused);
}

int CaptureSession::targetRate() const
{
    return m_source->targetRate();
}

void CaptureSession::setTargetRate(int rate)
{
    m_source->setTargetRate(rate);
}

bool CaptureSession::burstsEnabled() const
{
    return m_source->burstsEnabled();
}

void CaptureSession::setBurstsEnabled(bool enabled)
{
    m_source->setBurstsEnabled(enabled);
}

double CaptureSession::nominalRate() const
{
    return m_source->nominalRate();
}

bool CaptureSession::vehicleMode() const
{
    return m_source->vehicleMode();
}

void CaptureSession::setVehicleMode(bool vehicle)
{
    m_source->setVehicleMode(vehicle);
}

void CaptureSession::applyDatabase()
{
    const auto db = m_database->database();
    m_search = FrameSearch::parse(m_searchText, db);
    m_frames->setSearch(m_search);
    emit searchChanged();
    m_frames->setDatabase(db);
    m_overview->setDatabase(db);
    m_plotModel->setDatabase(db); // first: views of the signal model look up plot info
    m_signalModel->setDatabase(db);
}

qint64 CaptureSession::now() const
{
    return m_manualTime ? m_manualNowMs : m_clock.elapsed();
}

void CaptureSession::setManualTime(bool manual)
{
    m_manualTime = manual;
    m_source->setManualClock(manual);
    m_recorder->setSynchronous(manual);
    m_replay->setTimerDriven(!manual);
    if (manual)
        m_replay->setClock([this] { return m_manualNowMs * 1000; });
    if (manual) {
        m_flushTimer.stop();
        m_statsTimer.stop();
    }
}

void CaptureSession::manualTick(qint64 nowMs)
{
    m_manualNowMs = nowMs;
    if (m_replayMode)
        m_replay->tick();
    flush();
    updateStatistics();
    if (m_recorder->isRecording() && nowMs - m_lastRecorderFlushMs >= LogRecorder::kFlushIntervalMs) {
        m_recorder->flushNow();
        m_lastRecorderFlushMs = nowMs;
    }
}

QString CaptureSession::sourceDescription() const
{
    return activeSource()->description();
}

void CaptureSession::onFrames(const CanFrameBatch &batch)
{
    m_incoming.insert(m_incoming.end(), batch.cbegin(), batch.cend());
    if (m_recorder->isRecording() && !m_replayMode)
        m_recorder->append(batch.constData(), static_cast<std::size_t>(batch.size()));
}

void CaptureSession::flush()
{
    if (m_incoming.empty())
        return;
    std::vector<CanFrame> frames;
    frames.swap(m_incoming);
    m_stats.add(frames.data(), frames.size(), now());

    if (m_paused) {
        m_parked.insert(m_parked.end(), frames.begin(), frames.end());
        const auto cap = static_cast<std::size_t>(m_frames->capacity());
        while (m_parked.size() > cap)
            m_parked.pop_front(); // the ring buffer would have dropped these anyway
        return;
    }
    publish(frames);
}

void CaptureSession::publish(const std::vector<CanFrame> &frames)
{
    m_frames->append(frames);
    m_overview->update(frames);
    m_signalModel->update(frames);
}

void CaptureSession::updateStatistics()
{
    const std::int64_t t = now();
    m_stats.tick(t);
    m_fps = m_stats.framesPerSecond(t);
    m_busLoad = m_stats.busLoad(t);
    if (!isRunning() && m_fps < 0.5)
        m_statsTimer.stop();
    emit statisticsChanged();
}

void CaptureSession::clearViews()
{
    m_incoming.clear();
    m_parked.clear();
    m_frames->clear();
    m_overview->clear();
    m_signalModel->clearValues();
    m_stats.reset();
    m_fps = 0;
    m_busLoad = 0;
    emit statisticsChanged();
    emit cleared();
}

void CaptureSession::clear()
{
    clearViews();
}

void CaptureSession::setIdFilter(const QString &text)
{
    if (text == m_idFilter)
        return;
    m_idFilter = text;
    QString error;
    if (m_filter.setIdExpression(text, &error))
        m_idFilterError.clear();
    else
        m_idFilterError = error; // keep the last valid criterion active
    m_frames->setFilter(m_filter);
    emit filterChanged();
}

void CaptureSession::setDlcFilter(const QString &text)
{
    if (text == m_dlcFilter)
        return;
    m_dlcFilter = text;
    QString error;
    if (m_filter.setDlcExpression(text, &error))
        m_dlcFilterError.clear();
    else
        m_dlcFilterError = error;
    m_frames->setFilter(m_filter);
    emit filterChanged();
}

void CaptureSession::setFormatFilter(int format)
{
    format = std::clamp(format, 0, 2);
    if (format == formatFilter())
        return;
    m_filter.setFormat(static_cast<FrameFilter::Format>(format));
    m_frames->setFilter(m_filter);
    emit filterChanged();
}

void CaptureSession::clearFilter()
{
    m_idFilter.clear();
    m_dlcFilter.clear();
    m_idFilterError.clear();
    m_dlcFilterError.clear();
    m_filter = FrameFilter{};
    m_frames->setFilter(m_filter);
    emit filterChanged();
}

QObject *CaptureSession::frameModelObject() const
{
    return m_frames;
}

QObject *CaptureSession::overviewModelObject() const
{
    return m_overview;
}

QObject *CaptureSession::signalModelObject() const
{
    return m_signalModel;
}

QObject *CaptureSession::databaseObject() const
{
    return m_database;
}

QObject *CaptureSession::timeSeriesObject() const
{
    return m_store;
}

QObject *CaptureSession::plotModelObject() const
{
    return m_plotModel;
}

QObject *CaptureSession::plotControllerObject() const
{
    return m_plot;
}

QObject *CaptureSession::replayObject() const
{
    return m_replay;
}

QObject *CaptureSession::recorderObject() const
{
    return m_recorder;
}

QObject *CaptureSession::markersObject() const
{
    return m_markers;
}

QString CaptureSession::localPath(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

bool CaptureSession::fileExists(const QString &path) const
{
    return !path.isEmpty() && QFileInfo::exists(path);
}

// ------------------------------------------------------------------ recording

QString CaptureSession::defaultLogFolder() const
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(docs.isEmpty() ? QDir::homePath() : docs).filePath(QStringLiteral("CAN Analyzer logs"));
}

QString CaptureSession::defaultLogName() const
{
    return LogRecorder::defaultFileName(QDateTime::currentDateTime());
}

bool CaptureSession::startRecording(const QUrl &file)
{
    return startRecordingPath(localPath(file));
}

bool CaptureSession::startRecordingPath(const QString &path)
{
    if (m_replayMode) {
        emit notice(tr("Recording is available while the live source is running."), QStringLiteral("error"));
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    canlog::Header header;
    header.startTime = m_recordStartOverride.isValid() ? m_recordStartOverride : QDateTime::currentDateTimeUtc();
    header.application = QStringLiteral("CAN Analyzer %1").arg(QCoreApplication::applicationVersion());
    header.source = m_source->description();
    header.channels = {canlog::Channel{1, QStringLiteral("CAN 1 (simulated)"), m_stats.bitrate()}};
    m_lastRecorderFlushMs = m_manualNowMs;
    if (!m_recorder->start(path, header))
        return false;
    if (!isRunning())
        start(); // recording an idle bus is never what is wanted
    return true;
}

void CaptureSession::stopRecording()
{
    if (!m_recorder->isRecording())
        return;
    flush(); // frames still waiting for the next flush belong to the recording
    const bool ok = m_recorder->stop();
    if (ok)
        emit notice(tr("Saved %1 (%2 frames)").arg(QFileInfo(m_recorder->lastFile()).fileName())
                        .arg(m_recorder->framesWritten()), QStringLiteral("ok"));
}

// --------------------------------------------------------------------- replay

void CaptureSession::setReplayMode(bool on)
{
    if (on == m_replayMode)
        return;
    m_replayMode = on;
    if (m_paused) {
        m_paused = false;
        emit pausedChanged();
    }
    emit replayModeChanged();
    emit modeChanged();
    emit runningChanged();
}

bool CaptureSession::openLog(const QUrl &file)
{
    return openLogPath(localPath(file));
}

bool CaptureSession::openLogPath(const QString &path)
{
    QString logPath = path;
    if (path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive)) {
        // CSV import: convert into a temporary .canlog, replay that.
        if (!m_tempCanlog.isEmpty()) {
            m_replay->close();
            QFile::remove(m_tempCanlog);
        }
        m_tempCanlog = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                           .filePath(QStringLiteral("can_analyzer_import_%1.canlog").arg(QCoreApplication::applicationPid()));
        canlog::Header header;
        header.application = QStringLiteral("CAN Analyzer %1 (CSV import)").arg(QCoreApplication::applicationVersion());
        header.source = QFileInfo(path).fileName();
        const auto r = canlog::importCsv(path, m_tempCanlog, header);
        if (!r.ok) {
            emit notice(r.error, QStringLiteral("error"));
            return false;
        }
        logPath = m_tempCanlog;
    }
    if (m_recorder->isRecording())
        stopRecording();
    m_source->stop();
    QString error;
    m_replay->stop();
    if (!m_replay->open(logPath, &error)) {
        emit notice(error, QStringLiteral("error"));
        if (m_replayMode)
            closeLog();
        return false;
    }
    m_openedPath = path;
    // Markers live next to the file the user opened (also for CSV imports).
    if (!m_markers->attach(path))
        emit notice(m_markers->lastError(), QStringLiteral("error"));
    setReplayMode(true);
    clearViews();
    m_plot->clearCursors();
    m_replay->seek(m_replay->startUs(), 0);
    if (!m_replay->warning().isEmpty())
        emit notice(m_replay->warning(), QStringLiteral("error"));
    m_replay->start();
    m_plot->follow();
    return true;
}

void CaptureSession::closeLog()
{
    m_replay->close();
    m_markers->detach();
    m_openedPath.clear();
    setReplayMode(false);
    clearViews();
}

void CaptureSession::seekReplay(qint64 timeUs)
{
    if (!m_replayMode)
        return;
    m_replay->seek(timeUs);
    m_plot->follow();
}

void CaptureSession::addMarker(const QString &name)
{
    if (!m_replayMode)
        return;
    m_markers->add(m_replay->positionUs(), name);
}

bool CaptureSession::jumpToMarker(bool forward)
{
    if (!m_replayMode)
        return false;
    const qint64 pos = m_replay->positionUs();
    // A small tolerance so "previous" from just after a marker skips it.
    const qint64 t = forward ? m_markers->nextAfter(pos) : m_markers->previousBefore(pos - 50'000);
    if (t < 0)
        return false;
    seekReplay(t);
    return true;
}

bool CaptureSession::exportLogCsv(const QUrl &file)
{
    if (!m_replay->isLoaded())
        return false;
    const QString dest = localPath(file);
    const auto r = canlog::exportCsv(m_replay->filePath(), dest);
    if (r.ok)
        emit notice(tr("Saved %1 (%2 frames)").arg(QFileInfo(dest).fileName()).arg(r.frames), QStringLiteral("ok"));
    else
        emit notice(r.error, QStringLiteral("error"));
    return r.ok;
}

void CaptureSession::requestSummary()
{
    const QString path = m_replay->isLoaded() ? m_replay->filePath() : m_recorder->lastFile();
    if (path.isEmpty() || m_summaryBusy)
        return;
    const QString shownName = m_replay->isLoaded() ? m_openedPath : path;
    m_summaryBusy = true;
    emit summaryBusyChanged();
    auto db = m_database->database();
    auto *thread = QThread::create([this, path, db, shownName] {
        LogSummary s = LogSummary::compute(path, db);
        s.fileName = QFileInfo(shownName).fileName();
        QMetaObject::invokeMethod(this, [this, s] {
            m_lastSummary = s;
            m_summaryBusy = false;
            emit summaryBusyChanged();
            emit summaryReady(s.toVariant());
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

bool CaptureSession::exportSummaryCsv(const QUrl &file)
{
    QString error;
    const QString dest = localPath(file);
    if (!m_lastSummary.ok || !m_lastSummary.writeCsv(dest, &error)) {
        emit notice(error.isEmpty() ? tr("No summary to export.") : error, QStringLiteral("error"));
        return false;
    }
    emit notice(tr("Saved %1").arg(QFileInfo(dest).fileName()), QStringLiteral("ok"));
    return true;
}

// --------------------------------------------------------------------- search

void CaptureSession::setSearchText(const QString &text)
{
    if (text == m_searchText)
        return;
    m_searchText = text;
    m_search = FrameSearch::parse(text, m_database->database());
    m_frames->setSearch(m_search);
    emit searchChanged();
}

int CaptureSession::findNext(int fromRow, bool forward)
{
    return m_frames->findMatch(fromRow, forward);
}

int CaptureSession::matchCount() const
{
    return m_frames->matchCount();
}

void CaptureSession::loadDemoSetup()
{
    if (m_replayMode)
        closeLog();
    if (!m_database->isLoaded() || !m_database->isDemo())
        m_database->loadDemo();
    if (!m_source->vehicleMode())
        m_source->setVehicleMode(true);
    if (m_plotModel->count() == 0)
        m_plotModel->applyDemoLayout();
    m_plot->follow();
    if (!isRunning())
        start();
}

} // namespace can
