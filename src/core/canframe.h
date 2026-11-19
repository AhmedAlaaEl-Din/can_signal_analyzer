#pragma once

#include <QList>
#include <QMetaType>
#include <QString>

#include <array>
#include <cstdint>

namespace can {

// One classic CAN frame (no CAN FD in phase 1).
struct CanFrame
{
    static constexpr std::uint32_t kMaxStandardId = 0x7FF;
    static constexpr std::uint32_t kMaxExtendedId = 0x1FFFFFFF;
    static constexpr int kMaxDlc = 8;

    std::uint32_t id = 0;
    bool extended = false;
    std::uint8_t dlc = 0;                   // 0..8, classic CAN only
    std::array<std::uint8_t, 8> data{};     // bytes beyond dlc are zero
    std::int64_t timestampUs = 0;           // microseconds since measurement start
    std::uint8_t channel = 1;               // 1-based bus channel

    bool isValid() const noexcept
    {
        return dlc <= kMaxDlc && id <= (extended ? kMaxExtendedId : kMaxStandardId);
    }

    // Key that keeps standard and extended frames with the same numeric ID apart.
    std::uint32_t key() const noexcept { return id | (extended ? 0x80000000u : 0u); }

    bool operator==(const CanFrame &other) const noexcept = default;
};

using CanFrameBatch = QList<CanFrame>;

// Formatting helpers (used by the models and tested in isolation).
// IDs: 3 hex digits for standard frames, 8 for extended ones, upper case.
QString formatId(std::uint32_t id, bool extended);
// Data: "01 A2 FF" (upper-case hex, single space), only the first dlc bytes.
QString formatData(const CanFrame &frame);
// Timestamp in seconds with 6 decimals, e.g. "12.345678".
QString formatTimestamp(std::int64_t timestampUs);
// Parse a hex ID like "1A4", "0x1a4". Returns false on invalid input.
bool parseHexId(QStringView text, std::uint32_t &out);

} // namespace can

Q_DECLARE_METATYPE(can::CanFrame)
