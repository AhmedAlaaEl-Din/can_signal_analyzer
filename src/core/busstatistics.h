#pragma once

#include "canframe.h"

#include <cstdint>
#include <deque>

namespace can {

// Exponential moving average of the inter-arrival time of one message ID.
class PeriodEstimator
{
public:
    static constexpr double kAlpha = 0.125; // weight of the newest sample

    void add(std::int64_t timestampUs)
    {
        if (m_samples > 0) {
            const double delta = static_cast<double>(timestampUs - m_lastUs);
            m_avgUs = (m_samples == 1) ? delta : m_avgUs + kAlpha * (delta - m_avgUs);
        }
        m_lastUs = timestampUs;
        ++m_samples;
    }
    // Average period in milliseconds; 0 until two frames have been seen.
    double periodMs() const { return m_samples >= 2 ? m_avgUs / 1000.0 : 0.0; }
    std::int64_t lastUs() const { return m_lastUs; }
    std::uint64_t samples() const { return m_samples; }
    void reset() { *this = PeriodEstimator{}; }

private:
    std::int64_t m_lastUs = 0;
    double m_avgUs = 0;
    std::uint64_t m_samples = 0;
};

// Frame rate and bus-load estimation over a sliding wall-clock window.
class BusStatistics
{
public:
    static constexpr int kDefaultBitrate = 500000;
    static constexpr std::int64_t kWindowMs = 1000;

    // Nominal bits on the wire for a classic data frame without bit stuffing,
    // including the 3-bit interframe space:
    //   standard: 44 + 8*dlc + 3,  extended: 64 + 8*dlc + 3
    static int frameBits(const CanFrame &frame)
    {
        return (frame.extended ? 67 : 47) + 8 * frame.dlc;
    }

    explicit BusStatistics(int bitrate = kDefaultBitrate) : m_bitrate(bitrate) {}

    void add(const CanFrame *frames, std::size_t count, std::int64_t nowMs);
    void tick(std::int64_t nowMs); // drop samples that left the window
    void reset();

    std::uint64_t totalFrames() const { return m_totalFrames; }
    double framesPerSecond(std::int64_t nowMs) const;
    double bitsPerSecond(std::int64_t nowMs) const;
    // Fraction 0..1 (may exceed 1 if the simulated traffic overloads the bus).
    double busLoad(std::int64_t nowMs) const { return bitsPerSecond(nowMs) / m_bitrate; }
    int bitrate() const { return m_bitrate; }

private:
    struct Sample
    {
        std::int64_t ms;
        std::uint64_t frames;
        std::uint64_t bits;
    };
    double windowSeconds(std::int64_t nowMs) const;

    int m_bitrate;
    std::deque<Sample> m_samples;
    std::uint64_t m_windowFrames = 0;
    std::uint64_t m_windowBits = 0;
    std::uint64_t m_totalFrames = 0;
    std::int64_t m_startMs = -1;
};

} // namespace can
