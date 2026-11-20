#pragma once

#include "framesource.h"
#include "simulatorengine.h"

#include <QMutex>
#include <QPointer>

#include <memory>

class QThread;

namespace can {

class SimWorker;

// FrameSource backed by SimulatorEngine. The engine runs in a dedicated worker
// thread driven by a ~10 ms precise timer; frames are delivered in batches via
// framesReady() (queued into the receiver's thread).
class SimulatedSource : public FrameSource
{
    Q_OBJECT
    Q_PROPERTY(int targetRate READ targetRate WRITE setTargetRate NOTIFY targetRateChanged)
    Q_PROPERTY(bool burstsEnabled READ burstsEnabled WRITE setBurstsEnabled NOTIFY burstsEnabledChanged)
    Q_PROPERTY(bool vehicleMode READ vehicleMode WRITE setVehicleMode NOTIFY modeChanged)

public:
    explicit SimulatedSource(std::uint64_t seed = SimulatorEngine::kDefaultSeed,
                             SimulatorEngine::Mode mode = SimulatorEngine::Mode::Raw,
                             QObject *parent = nullptr);
    ~SimulatedSource() override;

    bool isRunning() const override;
    QString description() const override;

    int targetRate() const;
    void setTargetRate(int framesPerSecond);
    bool burstsEnabled() const;
    void setBurstsEnabled(bool enabled);
    std::uint64_t seed() const { return m_seed; }

    // true = demo vehicle (matches demo_vehicle.dbc), false = raw patterns.
    bool vehicleMode() const;
    void setVehicleMode(bool vehicle);
    double nominalRate() const;

    // Manual clock: no worker thread; the owner advances simulated time with
    // stepTo() and frames are emitted synchronously. Used for deterministic,
    // frame-exact demo capture. Must be set while stopped.
    void setManualClock(bool manual);
    bool manualClock() const { return m_manual; }
    std::int64_t currentTimeUs() const;
    void stepTo(std::int64_t timeUs);

    // Tick interval of the worker thread (ms). Mainly for tests.
    static constexpr int kTickMs = 10;

public slots:
    void start() override;
    void stop() override;

signals:
    void targetRateChanged(int rate);
    void burstsEnabledChanged(bool enabled);
    void modeChanged();

private:
    std::uint64_t m_seed;
    // The engine is shared with the worker; every access goes through m_mutex.
    std::shared_ptr<SimulatorEngine> m_engine;
    std::shared_ptr<QMutex> m_mutex;
    QThread *m_thread = nullptr;
    QPointer<SimWorker> m_worker;
    bool m_manual = false;
    bool m_manualRunning = false;
};

} // namespace can
