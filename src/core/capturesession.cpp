#include "capturesession.h"

#include "framemodel.h"
#include "messageoverviewmodel.h"
#include "signaldatabase.h"
#include "signallistmodel.h"
#include "plotcontroller.h"
#include "plotmodel.h"
#include "timeseriesstore.h"
#include "simulatedsource.h"

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
{
    m_signalModel->setStore(m_store);
    m_plot->setStore(m_store);
    connect(m_database, &SignalDatabase::changed, this, &CaptureSession::applyDatabase);
    Q_ASSERT(m_source);
    if (!m_source->parent())
        m_source->setParent(this);
    m_clock.start();

    connect(m_source, &FrameSource::framesReady, this, &CaptureSession::onFrames);
    connect(m_source, &FrameSource::runningChanged, this, [this](bool running) {
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
    m_source->disconnect(this);
    m_source->stop();
}

bool CaptureSession::isRunning() const
{
    return m_source->isRunning();
}

void CaptureSession::start()
{
    m_source->start();
}

void CaptureSession::stop()
{
    m_source->stop();
}

void CaptureSession::toggleRunning()
{
    isRunning() ? stop() : start();
}

void CaptureSession::setPaused(bool paused)
{
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
    m_frames->setDatabase(db);
    m_overview->setDatabase(db);
    m_signalModel->setDatabase(db);
    m_plotModel->setDatabase(db);
}

qint64 CaptureSession::now() const
{
    return m_manualTime ? m_manualNowMs : m_clock.elapsed();
}

void CaptureSession::setManualTime(bool manual)
{
    m_manualTime = manual;
    m_source->setManualClock(manual);
    if (manual) {
        m_flushTimer.stop();
        m_statsTimer.stop();
    }
}

void CaptureSession::manualTick(qint64 nowMs)
{
    m_manualNowMs = nowMs;
    flush();
    updateStatistics();
}

QString CaptureSession::sourceDescription() const
{
    return m_source->description();
}

void CaptureSession::onFrames(const CanFrameBatch &batch)
{
    m_incoming.insert(m_incoming.end(), batch.cbegin(), batch.cend());
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

void CaptureSession::clear()
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

} // namespace can
