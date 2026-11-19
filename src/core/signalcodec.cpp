#include "signalcodec.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace can::codec {

std::vector<int> occupiedBits(int startBit, int length, ByteOrder order)
{
    std::vector<int> bits;
    if (length < 1 || length > 64 || startBit < 0 || startBit > 63)
        return bits;
    bits.reserve(static_cast<std::size_t>(length));
    if (order == ByteOrder::Intel) {
        if (startBit + length > 64)
            return {};
        for (int i = length - 1; i >= 0; --i) // MSB first
            bits.push_back(startBit + i);
    } else {
        int bit = startBit;
        for (int i = 0; i < length; ++i) {
            if (bit < 0 || bit > 63)
                return {};
            bits.push_back(bit);
            bit = (bit % 8 == 0) ? bit + 15 : bit - 1;
        }
    }
    return bits;
}

bool fits(int startBit, int length, ByteOrder order, int dlc)
{
    const auto bits = occupiedBits(startBit, length, order);
    if (bits.empty())
        return false;
    return std::all_of(bits.begin(), bits.end(), [dlc](int b) { return b < dlc * 8; });
}

namespace {
inline int bitAt(const std::uint8_t *data, int bit)
{
    return (data[bit / 8] >> (bit % 8)) & 1;
}
inline void setBit(std::uint8_t *data, int bit, int value)
{
    const auto mask = static_cast<std::uint8_t>(1u << (bit % 8));
    if (value)
        data[bit / 8] = static_cast<std::uint8_t>(data[bit / 8] | mask);
    else
        data[bit / 8] = static_cast<std::uint8_t>(data[bit / 8] & ~mask);
}
} // namespace

std::uint64_t extractRaw(const std::uint8_t *data, int startBit, int length, ByteOrder order)
{
    std::uint64_t raw = 0;
    for (int bit : occupiedBits(startBit, length, order))
        raw = (raw << 1) | static_cast<std::uint64_t>(bitAt(data, bit));
    return raw;
}

void insertRaw(std::uint8_t *data, int startBit, int length, ByteOrder order, std::uint64_t raw)
{
    const auto bits = occupiedBits(startBit, length, order);
    const int n = static_cast<int>(bits.size());
    for (int i = 0; i < n; ++i) // bits[0] is the MSB
        setBit(data, bits[static_cast<std::size_t>(i)], static_cast<int>((raw >> (n - 1 - i)) & 1u));
}

std::int64_t toSigned(std::uint64_t raw, int length, bool isSigned)
{
    if (length >= 64)
        return static_cast<std::int64_t>(raw);
    const std::uint64_t mask = (std::uint64_t{1} << length) - 1;
    raw &= mask;
    if (isSigned && (raw >> (length - 1)) & 1u)
        return static_cast<std::int64_t>(raw | ~mask);
    return static_cast<std::int64_t>(raw);
}

std::int64_t rawMin(const SignalDef &s)
{
    if (!s.isSigned)
        return 0;
    if (s.length >= 64)
        return std::numeric_limits<std::int64_t>::min();
    return -(std::int64_t{1} << (s.length - 1));
}

std::int64_t rawMax(const SignalDef &s)
{
    if (s.length >= 64)
        return std::numeric_limits<std::int64_t>::max();
    if (s.isSigned)
        return (std::int64_t{1} << (s.length - 1)) - 1;
    if (s.length == 63)
        return std::numeric_limits<std::int64_t>::max();
    return (std::int64_t{1} << s.length) - 1;
}

double toPhysical(const SignalDef &s, std::int64_t raw)
{
    return static_cast<double>(raw) * s.factor + s.offset;
}

std::int64_t toRaw(const SignalDef &s, double physical)
{
    if (s.factor == 0.0)
        return 0;
    const double r = std::round((physical - s.offset) / s.factor);
    const double lo = static_cast<double>(rawMin(s));
    const double hi = static_cast<double>(rawMax(s));
    return static_cast<std::int64_t>(std::clamp(r, lo, hi));
}

void encode(const SignalDef &s, double physical, std::uint8_t *data)
{
    insertRaw(data, s.startBit, s.length, s.byteOrder, static_cast<std::uint64_t>(toRaw(s, physical)));
}

int decimalsFor(const SignalDef &s)
{
    auto needed = [](double v) {
        v = std::abs(v);
        for (int d = 0; d <= 4; ++d) {
            const double scaled = v * std::pow(10.0, d);
            if (std::abs(scaled - std::round(scaled)) < 1e-6 * std::max(1.0, scaled))
                return d;
        }
        return 4;
    };
    return std::max(needed(s.factor), needed(s.offset));
}

QString formatPhysical(const SignalDef &s, double physical)
{
    return QString::number(physical, 'f', decimalsFor(s));
}

} // namespace can::codec

namespace can {

DecodedSignal Decoder::decodeSignal(const MessageDef &message, int signalIndex, const CanFrame &frame)
{
    const SignalDef &s = message.signalList[static_cast<std::size_t>(signalIndex)];
    DecodedSignal d;
    d.signalIndex = signalIndex;
    const std::uint64_t bits = codec::extractRaw(frame.data.data(), s.startBit, s.length, s.byteOrder);
    d.raw = codec::toSigned(bits, s.length, s.isSigned);
    d.physical = codec::toPhysical(s, d.raw);
    if (auto it = s.valueTable.find(d.raw); it != s.valueTable.end())
        d.valueText = it->second;
    if (s.hasRange())
        d.outOfRange = d.physical < s.minimum - 1e-9 || d.physical > s.maximum + 1e-9;
    return d;
}

std::vector<DecodedSignal> Decoder::decode(const MessageDef &message, const CanFrame &frame)
{
    std::vector<DecodedSignal> out;
    out.reserve(message.signalList.size());

    // Multiplexing: read the multiplexor first (if any and if present).
    std::int64_t muxValue = -1;
    bool muxKnown = false;
    for (std::size_t i = 0; i < message.signalList.size(); ++i) {
        const SignalDef &s = message.signalList[i];
        if (s.muxRole == SignalDef::MuxRole::Multiplexor
            && codec::fits(s.startBit, s.length, s.byteOrder, frame.dlc)) {
            muxValue = decodeSignal(message, static_cast<int>(i), frame).raw;
            muxKnown = true;
        }
    }

    for (std::size_t i = 0; i < message.signalList.size(); ++i) {
        const SignalDef &s = message.signalList[i];
        if (!codec::fits(s.startBit, s.length, s.byteOrder, frame.dlc))
            continue;
        if (s.muxRole == SignalDef::MuxRole::Multiplexed && (!muxKnown || muxValue != s.muxValue))
            continue;
        out.push_back(decodeSignal(message, static_cast<int>(i), frame));
    }
    return out;
}

} // namespace can
