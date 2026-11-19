#include "simulatedsource.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

namespace can {

// Lives in the worker thread. Advances the engine to "now" on every tick and
// emits whatever frames became due as one batch.
class SimWorker : public QObject
{
    Q_OBJECT
public:
    SimWorker(std::shared_ptr<SimulatorEngine> engine, std::shared_ptr<QMutex> mutex)
        : m_engine(std::move(engine))
        , m_mutex(std::move(mutex))
    {}

public slots:
    void begin()
    {
        {
            QMutexLocker lock(m_mutex.get());
            m_baseUs = m_engine->currentTimeUs();
        }
        m_clock.start();
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::PreciseTimer);
        m_timer->setInterval(SimulatedSource::kTickMs);
        connect(m_timer, &QTimer::timeout, this, &SimWorker::tick);
        m_timer->start();
    }

    void tick()
    {
        m_scratch.clear();
        {
            QMutexLocker lock(m_mutex.get());
            m_engine->advanceTo(m_baseUs + m_clock.nsecsElapsed() / 1000, m_scratch);
        }
        if (m_scratch.empty())
            return;
        CanFrameBatch batch(m_scratch.begin(), m_scratch.end());
        emit batchReady(batch);
    }

    void end()
    {
        if (m_timer)
            m_timer->stop();
        tick(); // flush frames that became due up to the stop instant
    }

signals:
    void batchReady(const can::CanFrameBatch &batch);

private:
    std::shared_ptr<SimulatorEngine> m_engine;
    std::shared_ptr<QMutex> m_mutex;
    QElapsedTimer m_clock;
    QTimer *m_timer = nullptr;
    std::int64_t m_baseUs = 0;
    std::vector<CanFrame> m_scratch;
};

SimulatedSource::SimulatedSource(std::uint64_t seed, SimulatorEngine::Mode mode, QObject *parent)
    : FrameSource(parent)
    , m_seed(seed)
    , m_engine(std::make_shared<SimulatorEngine>(seed, mode))
    , m_mutex(std::make_shared<QMutex>())
{
    qRegisterMetaType<can::CanFrameBatch>("can::CanFrameBatch");
}

SimulatedSource::~SimulatedSource()
{
    stop();
}

bool SimulatedSource::isRunning() const
{
    return m_thread != nullptr || m_manualRunning;
}

bool SimulatedSource::vehicleMode() const
{
    QMutexLocker lock(m_mutex.get());
    return m_engine->mode() == SimulatorEngine::Mode::Vehicle;
}

void SimulatedSource::setVehicleMode(bool vehicle)
{
    {
        QMutexLocker lock(m_mutex.get());
        const auto mode = vehicle ? SimulatorEngine::Mode::Vehicle : SimulatorEngine::Mode::Raw;
        if (m_engine->mode() == mode)
            return;
        m_engine->setMode(mode);
    }
    emit modeChanged();
}

double SimulatedSource::nominalRate() const
{
    QMutexLocker lock(m_mutex.get());
    return m_engine->nominalRate();
}

void SimulatedSource::setManualClock(bool manual)
{
    if (isRunning())
        return;
    m_manual = manual;
}

std::int64_t SimulatedSource::currentTimeUs() const
{
    QMutexLocker lock(m_mutex.get());
    return m_engine->currentTimeUs();
}

void SimulatedSource::stepTo(std::int64_t timeUs)
{
    if (!m_manualRunning)
        return;
    std::vector<CanFrame> frames;
    {
        QMutexLocker lock(m_mutex.get());
        m_engine->advanceTo(timeUs, frames);
    }
    if (!frames.empty())
        emit framesReady(CanFrameBatch(frames.begin(), frames.end()));
}

QString SimulatedSource::description() const
{
    return QStringLiteral("%1 · seed 0x%2").arg(vehicleMode() ? QStringLiteral("Demo vehicle") : QStringLiteral("Raw patterns"), QString::number(m_seed, 16).toUpper());
}

int SimulatedSource::targetRate() const
{
    QMutexLocker lock(m_mutex.get());
    return m_engine->targetRate();
}

void SimulatedSource::setTargetRate(int framesPerSecond)
{
    {
        QMutexLocker lock(m_mutex.get());
        if (m_engine->targetRate() == framesPerSecond)
            return;
        m_engine->setTargetRate(framesPerSecond);
    }
    emit targetRateChanged(framesPerSecond);
}

bool SimulatedSource::burstsEnabled() const
{
    QMutexLocker lock(m_mutex.get());
    return m_engine->burstsEnabled();
}

void SimulatedSource::setBurstsEnabled(bool enabled)
{
    {
        QMutexLocker lock(m_mutex.get());
        if (m_engine->burstsEnabled() == enabled)
            return;
        m_engine->setBurstsEnabled(enabled);
    }
    emit burstsEnabledChanged(enabled);
}

void SimulatedSource::start()
{
    if (isRunning())
        return;
    if (m_manual) {
        m_manualRunning = true;
        emit runningChanged(true);
        return;
    }
    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("SimulatorThread"));
    auto *worker = new SimWorker(m_engine, m_mutex);
    worker->moveToThread(m_thread);
    m_worker = worker;
    connect(m_thread, &QThread::started, worker, &SimWorker::begin);
    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(worker, &SimWorker::batchReady, this, &FrameSource::framesReady);
    m_thread->start(QThread::HighPriority);
    emit runningChanged(true);
}

void SimulatedSource::stop()
{
    if (m_manualRunning) {
        m_manualRunning = false;
        emit runningChanged(false);
        return;
    }
    if (!m_thread)
        return;
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, &SimWorker::end, Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
    emit runningChanged(false);
}

} // namespace can

#include "simulatedsource.moc"
