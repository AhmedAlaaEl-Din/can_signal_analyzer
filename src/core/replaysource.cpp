#include "replaysource.h"

#include <QFileInfo>

#include <algorithm>

namespace can {

ReplaySource::ReplaySource(QObject *parent)
    : FrameSource(parent)
{
    qRegisterMetaType<can::CanFrameBatch>("can::CanFrameBatch");
    m_elapsed.start();
    m_clock = [this] { return m_elapsed.nsecsElapsed() / 1000; };
    m_timer.setInterval(kTickMs);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &ReplaySource::tick);
}

ReplaySource::~ReplaySource() = default;

QString ReplaySource::description() const
{
    return isLoaded() ? tr("Replay · %1").arg(fileName()) : tr("Replay");
}

QString ReplaySource::fileName() const
{
    return isLoaded() ? QFileInfo(m_reader.path()).fileName() : QString();
}

bool ReplaySource::open(const QString &path, QString *error)
{
    close();
    if (!m_reader.open(path, error)) {
        emit loadedChanged();
        return false;
    }
    if (m_reader.frameCount() == 0) {
        if (error)
            *error = tr("%1 contains no complete frames.").arg(QFileInfo(path).fileName());
        m_reader.close();
        emit loadedChanged();
        return false;
    }
    m_next = 0;
    m_posUs = static_cast<double>(m_reader.firstTimestampUs());
    emit loadedChanged();
    emit positionChanged();
    return true;
}

void ReplaySource::close()
{
    setPlaying(false);
    if (m_reader.isOpen()) {
        m_reader.close();
        m_next = 0;
        m_posUs = 0;
        emit loadedChanged();
        emit positionChanged();
    }
}

void ReplaySource::setClock(Clock clock)
{
    m_clock = std::move(clock);
    m_lastClockUs = m_clock();
}

void ReplaySource::setTimerDriven(bool timerDriven)
{
    m_timerDriven = timerDriven;
    if (!m_timerDriven)
        m_timer.stop();
    else if (m_playing)
        m_timer.start();
}

void ReplaySource::setSpeed(double speed)
{
    speed = speed <= 0 ? 0.0 : std::clamp(speed, 0.05, 64.0);
    if (speed == m_speed)
        return;
    m_speed = speed;
    emit speedChanged();
}

void ReplaySource::setLoop(bool loop)
{
    if (loop == m_loop)
        return;
    m_loop = loop;
    emit loopChanged();
}

void ReplaySource::setPlaying(bool playing)
{
    if (playing == m_playing)
        return;
    m_playing = playing;
    if (m_playing) {
        m_lastClockUs = m_clock();
        if (m_timerDriven)
            m_timer.start();
    } else {
        m_timer.stop();
    }
    emit runningChanged(m_playing);
}

void ReplaySource::start()
{
    if (!isLoaded())
        return;
    if (atEnd()) // play at the end starts over
        seek(startUs());
    setPlaying(true);
}

void ReplaySource::stop()
{
    setPlaying(false);
}

void ReplaySource::deliver(std::int64_t upTo)
{
    upTo = std::min<std::int64_t>(upTo, m_reader.frameCount());
    while (m_next < upTo) {
        m_scratch.clear();
        const auto want = static_cast<std::size_t>(std::min<std::int64_t>(upTo - m_next, 50000));
        const std::size_t got = m_reader.read(m_next, want, m_scratch);
        if (got == 0) { // unreadable tail
            m_next = upTo;
            break;
        }
        m_next += static_cast<std::int64_t>(got);
        emit framesReady(CanFrameBatch(m_scratch.begin(), m_scratch.end()));
    }
}

void ReplaySource::tick()
{
    const qint64 now = m_clock();
    const qint64 dt = std::max<qint64>(0, now - m_lastClockUs);
    m_lastClockUs = now;
    if (!m_playing || !isLoaded())
        return;

    const std::int64_t count = m_reader.frameCount();
    if (m_speed <= 0) {
        // As fast as possible: a fixed amount of frames per tick so the views
        // keep up and the UI stays responsive.
        const std::int64_t target = std::min<std::int64_t>(count, m_next + kAfapFramesPerTick);
        deliver(target);
        CanFrame last;
        if (m_next > 0 && m_reader.frameAt(m_next - 1, last))
            m_posUs = static_cast<double>(last.timestampUs);
    } else {
        m_posUs += static_cast<double>(dt) * m_speed;
        const auto pos = static_cast<std::int64_t>(m_posUs);
        // First frame after the position = index of (pos + 1).
        std::int64_t target = m_reader.indexAtTime(pos + 1);
        target = std::min<std::int64_t>(target, m_next + kMaxFramesPerTick);
        deliver(target);
    }

    if (m_next >= count) {
        m_posUs = static_cast<double>(m_reader.lastTimestampUs());
        if (m_loop) {
            emit discontinuity();
            m_next = 0;
            m_posUs = static_cast<double>(m_reader.firstTimestampUs());
        } else {
            setPlaying(false);
            emit endReached();
        }
    }
    emit positionChanged();
}

void ReplaySource::seek(qint64 timeUs, qint64 prerollUs)
{
    if (!isLoaded())
        return;
    timeUs = std::clamp<qint64>(timeUs, startUs(), endUs());
    emit discontinuity();
    const std::int64_t next = m_reader.indexAtTime(timeUs + 1);
    std::int64_t from = prerollUs > 0 ? m_reader.indexAtTime(timeUs - prerollUs) : next;
    from = std::max<std::int64_t>(from, next - kMaxPrerollFrames);
    if (from < next) {
        std::vector<CanFrame> frames;
        frames.reserve(static_cast<std::size_t>(next - from));
        m_reader.read(from, static_cast<std::size_t>(next - from), frames);
        emit prerollReady(CanFrameBatch(frames.begin(), frames.end()));
    }
    m_next = next;
    m_posUs = static_cast<double>(timeUs);
    m_lastClockUs = m_clock();
    emit positionChanged();
}

void ReplaySource::stepFrame()
{
    if (!isLoaded())
        return;
    setPlaying(false);
    if (m_next >= m_reader.frameCount())
        return;
    CanFrame f;
    if (m_reader.frameAt(m_next, f)) {
        // Deliver every frame sharing this timestamp (keeps position semantics).
        const std::int64_t upTo = m_reader.indexAtTime(f.timestampUs + 1);
        deliver(upTo);
        m_posUs = static_cast<double>(f.timestampUs);
    }
    emit positionChanged();
}

} // namespace can
