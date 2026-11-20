#pragma once

#include <QString>
#include <QStringList>

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace can {

// In-memory signal database built from a DBC file (subset, see docs).
// Plain data, no Qt GUI/Quick dependency.

enum class ByteOrder { Intel, Motorola }; // DBC "@1" = Intel (little endian), "@0" = Motorola

struct SignalDef
{
    enum class MuxRole { None, Multiplexor, Multiplexed };

    QString name;
    int startBit = 0;          // as written in the DBC (Motorola: MSB position, sawtooth numbering)
    int length = 1;            // 1..64
    ByteOrder byteOrder = ByteOrder::Intel;
    bool isSigned = false;
    double factor = 1.0;
    double offset = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;      // min == max (usually 0|0) means "no range given"
    QString unit;
    QStringList receivers;
    QString comment;
    MuxRole muxRole = MuxRole::None;
    int muxValue = -1;         // for Multiplexed signals: the multiplexor value that selects it
    std::map<std::int64_t, QString> valueTable; // raw value -> text
    int line = 0;              // source line, for messages

    bool hasRange() const { return minimum != maximum; }
};

struct MessageDef
{
    std::uint32_t id = 0;
    bool extended = false;
    QString name;
    int dlc = 8;
    int cycleTimeMs = 0;       // from BA_ "GenMsgCycleTime", 0 = unknown
    QString transmitter;
    QString comment;
    std::vector<SignalDef> signalList;
    int line = 0;

    std::uint32_t key() const { return id | (extended ? 0x80000000u : 0u); }
    const SignalDef *multiplexor() const;
    const SignalDef *findSignal(const QString &name) const;
};

struct DbcIssue
{
    int line = 0;       // 1-based; 0 = whole file
    QString message;
    QString toString() const
    {
        return line > 0 ? QStringLiteral("Line %1: %2").arg(line).arg(message) : message;
    }
};

class Database
{
public:
    std::vector<MessageDef> messages;
    QString comment;    // global CM_ "..."
    QString sourceName; // file name or "demo_vehicle.dbc"

    const MessageDef *find(std::uint32_t id, bool extended) const;
    int indexOf(std::uint32_t key) const;
    int signalCount() const;
    void rebuildIndex();

private:
    std::unordered_map<std::uint32_t, int> m_index;
};

struct ParseResult
{
    std::optional<Database> database; // set only when there are no errors
    std::vector<DbcIssue> errors;
    std::vector<DbcIssue> warnings;
    bool ok() const { return database.has_value(); }
};

// The invented sample database shipped as a resource (:/can/demo_vehicle.dbc).
// Parsed once and shared; never null.
std::shared_ptr<const Database> demoDatabase();
QString demoDatabaseText();

// Parser for the public DBC text format subset:
//   BO_ (messages), SG_ (signals incl. simple multiplexing M / mN), VAL_ (value
//   tables), CM_ (comments), BA_ "GenMsgCycleTime" (message period).
//   Everything else (VERSION, NS_, BS_, BU_, other BA_*, VAL_TABLE_,
//   SIG_VALTYPE_, ...) is skipped.
class DbcParser
{
public:
    static ParseResult parse(const QString &text, const QString &sourceName = QString());
    static ParseResult parseFile(const QString &path);
};

} // namespace can
