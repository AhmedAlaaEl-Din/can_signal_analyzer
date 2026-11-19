#pragma once

#include "canframe.h"
#include "dbc.h"

#include <QString>

#include <array>
#include <memory>

namespace can {

// A trace search query. The kind is detected from the text:
//
//   Signal condition  "VehicleSpeed > 100", "EngineData.EngineSpeed <= 800",
//                     "CurrentGear == 7", "GearLever = Drive" (value-table text)
//                     operators: >  >=  <  <=  ==  =  !=
//   Data pattern      "12 ?? FF"  bytes from byte 0, ?? (or ?, xx, **) = any byte;
//                     a pattern with fewer bytes than the frame matches its start
//   ID                "1A4", "0x1A4", "id 1A4", "id:1A4", a message name
//                     ("EngineData") with a database; 9+ hex digits are rejected
//
// Equality on signals compares within half a quantisation step (factor / 2).
class FrameSearch
{
public:
    enum class Kind { Empty, Invalid, Id, Data, Signal };
    enum class Op { Greater, GreaterEqual, Less, LessEqual, Equal, NotEqual };

    FrameSearch() = default;
    static FrameSearch parse(const QString &text, std::shared_ptr<const Database> db);

    Kind kind() const { return m_kind; }
    bool isValid() const { return m_kind == Kind::Id || m_kind == Kind::Data || m_kind == Kind::Signal; }
    QString error() const { return m_error; }
    // Human readable interpretation, e.g. "Signal VehicleSpeed > 100 km/h".
    QString description() const { return m_description; }

    bool matches(const CanFrame &frame) const;

private:
    Kind m_kind = Kind::Empty;
    QString m_error;
    QString m_description;
    // Id
    std::uint32_t m_id = 0;
    bool m_idHasFormat = false; // a message name fixes standard/extended
    bool m_idExtended = false;
    // Data
    int m_patternLength = 0;
    std::array<std::uint8_t, 8> m_bytes{};
    std::array<bool, 8> m_wild{};
    // Signal
    std::shared_ptr<const Database> m_db;
    std::uint32_t m_msgKey = 0;
    int m_signalIndex = -1;
    Op m_op = Op::Equal;
    double m_value = 0;
    double m_tolerance = 0;
};

} // namespace can
