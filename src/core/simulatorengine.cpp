#include "simulatorengine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace can {

namespace {
using P = SimulatorEngine::Pattern;

// All IDs and names are invented for this project.
const std::vector<SimulatorEngine::MessageDef> kMessages = {
    {0x0A0, false, 10, 2, P::Counter, 1, "SyncPulse"},
    {0x123, false, 10, 8, P::Sine, 1, "MotorDemand"},
    {0x18F, false, 20, 8, P::RandomWalk, 1, "WheelTicks"},
    {0x1A4, false, 20, 4, P::Sine, 1, "SteerAngle"},
    {0x220, false, 50, 6, P::Sine, 1, "PackVoltage"},
    {0x2F0, false, 50, 8, P::RandomWalk, 1, "ThermalZones"},
    {0x305, false, 100, 1, P::BitFlags, 1, "DoorFlags"},
    {0x3E8, false, 100, 3, P::BitFlags, 1, "LampState"},
    {0x1ABCDE01, true, 500, 8, P::Counter, 2, "AuxNodeStatus"},
    {0x0C0FFEE5, true, 500, 5, P::Sine, 2, "BrewerTelemetry"},
    {0x601, false, 1000, 1, P::Counter, 1, "NodeHeartbeat"},
    {0x6F0, false, 1000, 8, P::VariableLength, 1, "ConfigEcho"},
};

std::uint8_t clampByte(double v)
{
    return static_cast<std::uint8_t>(std::clamp(v, 0.0, 255.0));
}

void put16(CanFrame &f, int offset, double value)
{
    const auto v = static_cast<std::uint16_t>(std::clamp(value, 0.0, 65535.0));
    f.data[static_cast<std::size_t>(offset)] = static_cast<std::uint8_t>(v & 0xFF);
    f.data[static_cast<std::size_t>(offset + 1)] = static_cast<std::uint8_t>(v >> 8);
}
} // namespace

SimulatorEngine::SimulatorEngine(std::uint64_t seed)
    : m_seed(seed)
    , m_rng(seed)
    , m_state(kMessages.size())
{
    for (auto &s : m_state) {
        for (auto &w : s.walk)
            w = 96.0 + m_rng.range(0, 64);
        s.flags = static_cast<std::uint8_t>(m_rng.range(0, 255));
    }
    reschedulePeriods();
    // Stagger start offsets so periodic messages do not all collide at t = 0.
    for (auto &s : m_state)
        s.nextDueUs = m_rng.range(1, static_cast<int>(s.periodUs));
    m_nextBurstUs = static_cast<std::int64_t>(kBurstIntervalMs) * 1000;
}

const std::vector<SimulatorEngine::MessageDef> &SimulatorEngine::messages()
{
    return kMessages;
}

double SimulatorEngine::nominalRate()
{
    double rate = 0;
    for (const auto &m : kMessages)
        rate += 1000.0 / m.periodMs;
    return rate;
}

double SimulatorEngine::effectiveRate() const noexcept
{
    return m_targetRate > 0 ? m_targetRate : nominalRate();
}

void SimulatorEngine::setTargetRate(int framesPerSecond)
{
    const int rate = std::clamp(framesPerSecond, 0, kMaxRate);
    if (rate == m_targetRate)
        return;
    m_targetRate = rate;
    std::vector<std::int64_t> lastSent(m_state.size());
    for (std::size_t i = 0; i < m_state.size(); ++i)
        lastSent[i] = m_state[i].nextDueUs - m_state[i].periodUs;
    reschedulePeriods();
    // Keep the phase: the next frame is due one new period after the last one.
    for (std::size_t i = 0; i < m_state.size(); ++i)
        m_state[i].nextDueUs = std::max(m_nowUs + 1, lastSent[i] + m_state[i].periodUs);
}

void SimulatorEngine::setBurstsEnabled(bool enabled)
{
    if (enabled == m_burstsEnabled)
        return;
    m_burstsEnabled = enabled;
    m_burstRemaining = 0;
    m_nextBurstUs = m_nowUs + static_cast<std::int64_t>(kBurstIntervalMs) * 1000;
}

void SimulatorEngine::reschedulePeriods()
{
    const double scale = m_targetRate > 0 ? m_targetRate / nominalRate() : 1.0;
    for (std::size_t i = 0; i < m_state.size(); ++i) {
        const double us = kMessages[i].periodMs * 1000.0 / scale;
        m_state[i].periodUs = std::max<std::int64_t>(100, std::llround(us));
    }
}

std::int64_t SimulatorEngine::jitterUs(std::int64_t periodUs)
{
    // +-1.5 % timing jitter, like a real node scheduler.
    const int span = static_cast<int>(std::max<std::int64_t>(1, periodUs * 15 / 1000));
    return m_rng.range(-span, span);
}

std::size_t SimulatorEngine::advanceTo(std::int64_t endUs, std::vector<CanFrame> &out)
{
    std::size_t produced = 0;
    for (;;) {
        // Earliest due event; ties resolved by table order, bursts after periodic.
        std::int64_t best = std::numeric_limits<std::int64_t>::max();
        std::size_t bestIndex = m_state.size();
        for (std::size_t i = 0; i < m_state.size(); ++i) {
            if (m_state[i].nextDueUs < best) {
                best = m_state[i].nextDueUs;
                bestIndex = i;
            }
        }
        bool burstEvent = false;
        if (m_burstsEnabled) {
            const std::int64_t burstDue = m_burstRemaining > 0 ? m_nextBurstFrameUs : m_nextBurstUs;
            if (burstDue < best) {
                best = burstDue;
                burstEvent = true;
            }
        }
        if (best > endUs)
            break;

        m_nowUs = best;
        if (burstEvent) {
            if (m_burstRemaining == 0) {
                m_burstRemaining = kBurstFrames;
                m_nextBurstUs = best + static_cast<std::int64_t>(kBurstIntervalMs) * 1000;
            }
            out.push_back(makeBurstFrame(best));
            --m_burstRemaining;
            m_nextBurstFrameUs = best + kBurstSpacingUs;
        } else {
            out.push_back(makeFrame(bestIndex, best));
            auto &s = m_state[bestIndex];
            s.nextDueUs = best + std::max<std::int64_t>(50, s.periodUs + jitterUs(s.periodUs));
        }
        ++produced;
    }
    m_nowUs = std::max(m_nowUs, endUs);
    return produced;
}

CanFrame SimulatorEngine::makeFrame(std::size_t index, std::int64_t tUs)
{
    const MessageDef &def = kMessages[index];
    MessageState &s = m_state[index];
    CanFrame f;
    f.id = def.id;
    f.extended = def.extended;
    f.dlc = static_cast<std::uint8_t>(def.dlc);
    f.timestampUs = tUs;
    f.channel = def.channel;
    const double t = static_cast<double>(tUs) / 1e6;
    constexpr double twoPi = 2.0 * std::numbers::pi;
    auto at = [&f](int i) -> std::uint8_t & { return f.data[static_cast<std::size_t>(i)]; };

    switch (def.pattern) {
    case Pattern::Counter: {
        at(0) = static_cast<std::uint8_t>(s.counter & 0xFF);
        if (f.dlc > 1)
            at(1) = static_cast<std::uint8_t>((s.counter >> 8) & 0xFF);
        for (int i = 2; i < f.dlc; ++i)
            at(i) = static_cast<std::uint8_t>(0x10 * i + static_cast<int>(index & 0xF));
        if (f.dlc > 2) { // last byte: XOR checksum of the others
            std::uint8_t x = 0;
            for (int i = 0; i < f.dlc - 1; ++i)
                x ^= at(i);
            at(f.dlc - 1) = x;
        }
        break;
    }
    case Pattern::Sine: {
        const double slow = std::sin(twoPi * t / (6.0 + static_cast<double>(index)));
        const double fast = std::sin(twoPi * t / 1.7 + static_cast<double>(index));
        if (f.dlc >= 2)
            put16(f, 0, 32768.0 + 30000.0 * slow);
        if (f.dlc >= 4)
            put16(f, 2, 32768.0 + 12000.0 * fast);
        if (f.dlc >= 5)
            at(4) = clampByte(128.0 + 100.0 * slow * fast);
        if (f.dlc >= 6)
            at(5) = static_cast<std::uint8_t>(s.counter & 0x0F);
        if (f.dlc == 8) {
            at(6) = clampByte(128.0 + 120.0 * std::sin(twoPi * t / 3.3));
            at(7) = static_cast<std::uint8_t>(s.counter & 0xFF);
        }
        break;
    }
    case Pattern::RandomWalk: {
        if (index % 2 == 0) { // four 16-bit channels
            for (int c = 0; c < 4; ++c) {
                double &w = s.walk[static_cast<std::size_t>(c)];
                w = std::clamp(w + m_rng.range(-3, 3), 0.0, 255.0);
                put16(f, c * 2, w * 180.0 + m_rng.range(0, 15));
            }
        } else { // eight 8-bit channels drifting slowly
            for (int c = 0; c < f.dlc; ++c) {
                double &w = s.walk[static_cast<std::size_t>(c % 4)];
                if (c < 4)
                    w = std::clamp(w + m_rng.range(-1, 1) * 0.5, 20.0, 230.0);
                at(c) = clampByte(w + (c >= 4 ? 6.0 * (c - 3) : 0.0));
            }
        }
        break;
    }
    case Pattern::BitFlags: {
        if (m_rng.range(0, 99) < 8) // rare toggles
            s.flags ^= static_cast<std::uint8_t>(1u << m_rng.range(0, 7));
        at(0) = s.flags;
        if (f.dlc > 1)
            at(1) = static_cast<std::uint8_t>(~s.flags);
        if (f.dlc > 2)
            at(2) = static_cast<std::uint8_t>(s.counter & 0x0F);
        break;
    }
    case Pattern::VariableLength: {
        f.dlc = static_cast<std::uint8_t>(2 + (s.counter % 7)); // 2..8
        for (int i = 0; i < f.dlc; ++i)
            at(i) = static_cast<std::uint8_t>(0xA0 + i + static_cast<int>(s.counter & 0x0F));
        break;
    }
    }
    ++s.counter;
    return f;
}

CanFrame SimulatorEngine::makeBurstFrame(std::int64_t tUs)
{
    CanFrame f;
    f.id = kBurstId;
    f.dlc = 8;
    f.timestampUs = tUs;
    f.channel = 1;
    f.data[0] = static_cast<std::uint8_t>(m_burstCounter & 0xFF);
    f.data[1] = static_cast<std::uint8_t>(kBurstFrames - m_burstRemaining);
    for (std::size_t i = 2; i < 8; ++i)
        f.data[i] = static_cast<std::uint8_t>(m_rng.next() & 0xFF);
    if (m_burstRemaining == 1)
        ++m_burstCounter;
    return f;
}

} // namespace can
