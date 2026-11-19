#pragma once

#include "canframe.h"

#include <array>
#include <cstdint>
#include <vector>

namespace can {

// Small, portable PRNG (SplitMix64). std:: distributions are not guaranteed to
// produce the same sequence across standard libraries, so we use our own.
class SplitMix64
{
public:
    explicit SplitMix64(std::uint64_t seed = 0) : m_state(seed) {}
    std::uint64_t next() noexcept
    {
        std::uint64_t z = (m_state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    // Uniform integer in [lo, hi].
    int range(int lo, int hi) noexcept
    {
        const auto span = static_cast<std::uint64_t>(static_cast<std::int64_t>(hi) - lo + 1);
        return lo + static_cast<int>(next() % span);
    }

private:
    std::uint64_t m_state;
};

// Deterministic CAN traffic generator. Pure logic: no threads, no event loop.
// The same seed and the same advanceTo() end time always produce exactly the
// same frames, independent of how the time span is chunked into calls.
class SimulatorEngine
{
public:
    enum class Pattern { Counter, Sine, RandomWalk, BitFlags, VariableLength };

    struct MessageDef
    {
        std::uint32_t id;
        bool extended;
        int periodMs;
        int dlc;
        Pattern pattern;
        std::uint8_t channel;
        const char *name; // invented, for docs/debugging only
    };

    static constexpr std::uint64_t kDefaultSeed = 0xC0FFEEull;
    static constexpr std::uint32_t kBurstId = 0x5B0;
    static constexpr int kBurstIntervalMs = 2500;
    static constexpr int kBurstFrames = 16;
    static constexpr int kBurstSpacingUs = 250;
    static constexpr int kMaxRate = 5000;

    explicit SimulatorEngine(std::uint64_t seed = kDefaultSeed);

    // The fixed table of invented messages.
    static const std::vector<MessageDef> &messages();
    // Sum of the nominal rates of all periodic messages (frames per second).
    static double nominalRate();

    // Target total rate in frames/s for the periodic traffic. 0 = nominal periods.
    void setTargetRate(int framesPerSecond);
    int targetRate() const noexcept { return m_targetRate; }
    double effectiveRate() const noexcept;

    void setBurstsEnabled(bool enabled);
    bool burstsEnabled() const noexcept { return m_burstsEnabled; }

    std::uint64_t seed() const noexcept { return m_seed; }
    std::int64_t currentTimeUs() const noexcept { return m_nowUs; }

    // Generates every frame due in (currentTime, endUs] in timestamp order and
    // appends it to out. Returns the number of frames produced.
    std::size_t advanceTo(std::int64_t endUs, std::vector<CanFrame> &out);

private:
    struct MessageState
    {
        std::int64_t nextDueUs = 0;
        std::int64_t periodUs = 0;
        std::uint32_t counter = 0;
        std::array<double, 4> walk{};
        std::uint8_t flags = 0;
    };

    void reschedulePeriods();
    CanFrame makeFrame(std::size_t index, std::int64_t tUs);
    CanFrame makeBurstFrame(std::int64_t tUs);
    std::int64_t jitterUs(std::int64_t periodUs);

    std::uint64_t m_seed;
    SplitMix64 m_rng;
    std::vector<MessageState> m_state;
    std::int64_t m_nowUs = 0;
    int m_targetRate = 0;
    bool m_burstsEnabled = false;
    std::int64_t m_nextBurstUs = 0;
    int m_burstRemaining = 0;
    std::int64_t m_nextBurstFrameUs = 0;
    std::uint32_t m_burstCounter = 0;
};

} // namespace can
