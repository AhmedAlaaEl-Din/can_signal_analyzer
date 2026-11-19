#pragma once

#include "canframe.h"
#include "dbc.h"

#include <QVariantList>

#include <cstdint>
#include <vector>

namespace can {

// Bit-level signal access for classic CAN payloads (8 bytes).
//
// Bit numbering follows the DBC convention: bit n is bit (n % 8) of byte (n / 8),
// bit 0 being the least significant bit of a byte.
//  - Intel (little endian): startBit is the LSB; following bits go upwards
//    (n, n+1, ...), naturally continuing into the next byte.
//  - Motorola (big endian): startBit is the MSB; following bits go downwards
//    inside a byte and jump to bit 7 of the next byte ("sawtooth").
namespace codec {

// Data-bit indices the signal occupies, from MSB to LSB of the raw value.
// Empty if the signal does not fit into 64 bits.
std::vector<int> occupiedBits(int startBit, int length, ByteOrder order);
// True if all bits lie inside the first `dlc` bytes.
bool fits(int startBit, int length, ByteOrder order, int dlc);

std::uint64_t extractRaw(const std::uint8_t *data, int startBit, int length, ByteOrder order);
void insertRaw(std::uint8_t *data, int startBit, int length, ByteOrder order, std::uint64_t raw);

// Interprets the low `length` bits as two's complement when isSigned.
std::int64_t toSigned(std::uint64_t raw, int length, bool isSigned);
// Smallest / largest raw value representable by the signal.
std::int64_t rawMin(const SignalDef &s);
std::int64_t rawMax(const SignalDef &s);

double toPhysical(const SignalDef &s, std::int64_t raw);
// Physical -> raw (rounded, clamped to the representable raw range).
std::int64_t toRaw(const SignalDef &s, double physical);
// Writes a physical value into the payload.
void encode(const SignalDef &s, double physical, std::uint8_t *data);

// Number of decimals that represent factor/offset exactly (0..4).
int decimalsFor(const SignalDef &s);
QString formatPhysical(const SignalDef &s, double physical);

} // namespace codec

struct DecodedSignal
{
    int signalIndex = -1;      // index into MessageDef::signalList
    std::int64_t raw = 0;
    double physical = 0.0;
    QString valueText;         // from VAL_, empty if none
    bool outOfRange = false;   // physical outside [min, max] (when a range is given)
};

class Decoder
{
public:
    // Decoded signals as a QVariantList of maps for QML:
    // { name, value (formatted), unit, enumText, raw, outOfRange }
    static QVariantList toVariantList(const MessageDef &message, const CanFrame &frame);

    // Decodes all signals of `message` present in `frame`. Signals that do not
    // fit into the frame's DLC, and multiplexed signals whose multiplexor value
    // does not match, are skipped.
    static std::vector<DecodedSignal> decode(const MessageDef &message, const CanFrame &frame);
    static DecodedSignal decodeSignal(const MessageDef &message, int signalIndex, const CanFrame &frame);
};

} // namespace can
