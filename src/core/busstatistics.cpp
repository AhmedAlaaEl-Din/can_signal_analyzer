#include "busstatistics.h"

#include <algorithm>

namespace can {

void BusStatistics::add(const CanFrame *frames, std::size_t count, std::int64_t nowMs)
{
    if (m_startMs < 0)
        m_startMs = nowMs;
    std::uint64_t bits = 0;
    for (std::size_t i = 0; i < count; ++i)
        bits += static_cast<std::uint64_t>(frameBits(frames[i]));
    if (!m_samples.empty() && m_samples.back().ms == nowMs) {
        m_samples.back().frames += count;
        m_samples.back().bits += bits;
    } else {
        m_samples.push_back({nowMs, count, bits});
    }
    m_windowFrames += count;
    m_windowBits += bits;
    m_totalFrames += count;
    tick(nowMs);
}

void BusStatistics::tick(std::int64_t nowMs)
{
    while (!m_samples.empty() && m_samples.front().ms <= nowMs - kWindowMs) {
        m_windowFrames -= m_samples.front().frames;
        m_windowBits -= m_samples.front().bits;
        m_samples.pop_front();
    }
}

void BusStatistics::reset()
{
    m_samples.clear();
    m_windowFrames = 0;
    m_windowBits = 0;
    m_totalFrames = 0;
    m_startMs = -1;
}

double BusStatistics::windowSeconds(std::int64_t nowMs) const
{
    // Until a full window has elapsed, divide by the time actually observed so
    // the first second does not under-report the rate.
    if (m_startMs < 0)
        return 1.0;
    const std::int64_t observed = std::clamp<std::int64_t>(nowMs - m_startMs, 100, kWindowMs);
    return static_cast<double>(observed) / 1000.0;
}

double BusStatistics::framesPerSecond(std::int64_t nowMs) const
{
    return static_cast<double>(m_windowFrames) / windowSeconds(nowMs);
}

double BusStatistics::bitsPerSecond(std::int64_t nowMs) const
{
    return static_cast<double>(m_windowBits) / windowSeconds(nowMs);
}

} // namespace can
