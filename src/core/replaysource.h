#pragma once

#include "canlog.h"
#include "framesource.h"

#include <QElapsedTimer>
#include <QTimer>

#include <functional>

namespace can {

// Plays a .canlog file through the same pipeline as the simulator: frames are
// emitted with framesReady() in their original timestamps, so the trace,
// messages, signals and plots work unchanged.
//
// Timing: the replay position advances by (clock delta x speed) on every
// tick; all frames with timestamp <= position are emitted. The clock is
// injectable (fake clock in tests, the deterministic demo clock in --demo).
// Speed 0 means "as fast as possible" (kAfapFramesPerTick frames per tick).
//
// Position semantics: every frame with timestamp <= positionUs() has been
// delivered. Seeking emits discontinuity() (views clear) and then delivers a
// pre-roll of up to prerollUs of history before the target through
// prerollReady(), so plots and the trace show context right away.
class ReplaySource : public FrameSource
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY loadedChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY loadedChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY loadedChanged)
    Q_PROPERTY(bool playing READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(qint64 positionUs READ positionUs NOTIFY positionChanged)
    Q_PROPERTY(qint64 startUs READ startUs NOTIFY loadedChanged)
    Q_PROPERTY(qint64 endUs READ endUs NOTIFY loadedChanged)
    Q_PROPERTY(qint64 durationUs READ durationUs NOTIFY loadedChanged)
    Q_PROPERTY(qint64 frameIndex READ frameIndex NOTIFY positionChanged)
    Q_PROPERTY(qint64 frameCount READ frameCount NOTIFY loadedChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY loadedChanged)
    Q_PROPERTY(bool atEnd READ atEnd NOTIFY positionChanged)

public:
    using Clock = std::function<qint64()>; // monotonic microseconds

    static constexpr int kTickMs = 10;
    static constexpr int kAfapFramesPerTick = 4000;
    static constexpr int kMaxFramesPerTick = 200000;
    static constexpr std::int64_t kDefaultPrerollUs = 10'000'000;
    static constexpr int kMaxPrerollFrames = 60000;

    explicit ReplaySource(QObject *parent = nullptr);
    ~ReplaySource() override;

    bool isRunning() const override { return m_playing; }
    QString description() const override;

    bool open(const QString &path, QString *error = nullptr);
    void close();
    bool isLoaded() const { return m_reader.isOpen(); }
    QString fileName() const;
    QString filePath() const { return m_reader.path(); }
    canlog::Reader &reader() { return m_reader; }
    QString warning() const { return m_reader.warning(); }

    void setClock(Clock clock);
    // false: no internal timer; the owner calls tick() (tests, demo).
    void setTimerDriven(bool timerDriven);

    double speed() const { return m_speed; }
    void setSpeed(double speed); // 0 = as fast as possible, else 0.05..64
    bool loop() const { return m_loop; }
    void setLoop(bool loop);
    qint64 positionUs() const { return static_cast<qint64>(m_posUs); }
    qint64 startUs() const { return m_reader.firstTimestampUs(); }
    qint64 endUs() const { return m_reader.lastTimestampUs(); }
    qint64 durationUs() const { return m_reader.durationUs(); }
    qint64 frameIndex() const { return m_next; }
    qint64 frameCount() const { return m_reader.frameCount(); }
    bool atEnd() const { return isLoaded() && m_next >= m_reader.frameCount(); }

public slots:
    void start() override; // play
    void stop() override;  // pause
    void tick();
    void seek(qint64 timeUs, qint64 prerollUs = kDefaultPrerollUs);
    void stepFrame();

signals:
    void loadedChanged();
    void speedChanged();
    void loopChanged();
    void positionChanged();
    void discontinuity();                            // views should clear
    void prerollReady(const can::CanFrameBatch &batch); // history before a seek target
    void endReached();

private:
    void setPlaying(bool playing);
    void deliver(std::int64_t upToIndexExclusive);

    canlog::Reader m_reader;
    Clock m_clock;
    QElapsedTimer m_elapsed;
    QTimer m_timer;
    bool m_timerDriven = true;
    bool m_playing = false;
    bool m_loop = false;
    double m_speed = 1.0;
    double m_posUs = 0;
    qint64 m_lastClockUs = 0;
    std::int64_t m_next = 0;  // index of the next frame to deliver
    std::vector<CanFrame> m_scratch;
};

} // namespace can
