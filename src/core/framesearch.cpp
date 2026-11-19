#include "framesearch.h"

#include "signalcodec.h"

#include <QRegularExpression>

#include <cmath>

namespace can {

namespace {

QString tr(const char *s)
{
    return QString::fromUtf8(s);
}

bool isWildToken(const QString &t)
{
    return t == QLatin1String("??") || t == QLatin1String("?") || t.compare(QLatin1String("xx"), Qt::CaseInsensitive) == 0
           || t == QLatin1String("**") || t == QLatin1String("*");
}

bool isByteToken(const QString &t)
{
    static const QRegularExpression re(QStringLiteral("^[0-9A-Fa-f]{1,2}$"));
    return re.match(t).hasMatch();
}

QString opText(FrameSearch::Op op)
{
    switch (op) {
    case FrameSearch::Op::Greater: return QStringLiteral(">");
    case FrameSearch::Op::GreaterEqual: return QStringLiteral("≥");
    case FrameSearch::Op::Less: return QStringLiteral("<");
    case FrameSearch::Op::LessEqual: return QStringLiteral("≤");
    case FrameSearch::Op::Equal: return QStringLiteral("=");
    case FrameSearch::Op::NotEqual: return QStringLiteral("≠");
    }
    return {};
}

} // namespace

FrameSearch FrameSearch::parse(const QString &input, std::shared_ptr<const Database> db)
{
    FrameSearch s;
    const QString text = input.trimmed();
    if (text.isEmpty())
        return s;
    auto invalid = [&](const QString &e) {
        s.m_kind = Kind::Invalid;
        s.m_error = e;
        return s;
    };

    // ---- signal condition
    static const QRegularExpression cond(
        QStringLiteral("^([A-Za-z_][A-Za-z0-9_]*(?:\\.[A-Za-z_][A-Za-z0-9_]*)?)\\s*(>=|<=|==|!=|=|>|<)\\s*(.+)$"));
    const auto cm = cond.match(text);
    if (cm.hasMatch()) {
        if (!db)
            return invalid(tr("Load a database to search by signal value."));
        const QString name = cm.captured(1);
        const QString opStr = cm.captured(2);
        const QString valueText = cm.captured(3).trimmed();
        QString msgName, sigName = name;
        if (name.contains(QLatin1Char('.'))) {
            msgName = name.section(QLatin1Char('.'), 0, 0);
            sigName = name.section(QLatin1Char('.'), 1);
        }
        const MessageDef *msg = nullptr;
        int sigIndex = -1;
        for (const auto &m : db->messages) {
            if (!msgName.isEmpty() && m.name.compare(msgName, Qt::CaseInsensitive) != 0)
                continue;
            for (std::size_t i = 0; i < m.signalList.size(); ++i) {
                if (m.signalList[i].name.compare(sigName, Qt::CaseInsensitive) == 0) {
                    msg = &m;
                    sigIndex = static_cast<int>(i);
                    break;
                }
            }
            if (msg)
                break;
        }
        if (!msg)
            return invalid(tr("Unknown signal \"%1\".").arg(name));
        const SignalDef &sig = msg->signalList[static_cast<std::size_t>(sigIndex)];
        s.m_op = opStr == QLatin1String(">")    ? Op::Greater
                 : opStr == QLatin1String(">=") ? Op::GreaterEqual
                 : opStr == QLatin1String("<")  ? Op::Less
                 : opStr == QLatin1String("<=") ? Op::LessEqual
                 : opStr == QLatin1String("!=") ? Op::NotEqual
                                                : Op::Equal;
        bool ok = false;
        double value = valueText.toDouble(&ok);
        QString shown = valueText;
        if (!ok) {
            // Value-table text, e.g. "GearLever = Drive".
            QString label = valueText;
            if (label.size() >= 2 && label.startsWith(QLatin1Char('"')) && label.endsWith(QLatin1Char('"')))
                label = label.mid(1, label.size() - 2);
            for (const auto &[raw, txt] : sig.valueTable) {
                if (txt.compare(label, Qt::CaseInsensitive) == 0) {
                    value = codec::toPhysical(sig, raw);
                    ok = true;
                    shown = txt;
                    break;
                }
            }
            if (!ok)
                return invalid(sig.valueTable.empty()
                                   ? tr("\"%1\" is not a number.").arg(valueText)
                                   : tr("\"%1\" is not a number or a state of %2.").arg(valueText, sig.name));
        }
        s.m_kind = Kind::Signal;
        s.m_db = std::move(db);
        s.m_msgKey = msg->key();
        s.m_signalIndex = sigIndex;
        s.m_value = value;
        s.m_tolerance = std::abs(sig.factor) / 2.0;
        s.m_description = tr("Signal %1 %2 %3%4")
                              .arg(sig.name, opText(s.m_op), shown,
                                   sig.unit.isEmpty() || shown != valueText ? QString() : QStringLiteral(" ") + sig.unit);
        return s;
    }

    // An operator without a complete "Signal op value" is an unfinished condition.
    static const QRegularExpression anyOp(QStringLiteral("[<>=!]"));
    if (text.contains(anyOp))
        return invalid(tr("Write a condition as Signal > value, e.g. VehicleSpeed > 100."));

    // ---- explicit id prefix
    QString idText;
    static const QRegularExpression idPrefix(QStringLiteral("^id\\s*[: ]\\s*(\\S+)$"), QRegularExpression::CaseInsensitiveOption);
    const auto im = idPrefix.match(text);
    if (im.hasMatch())
        idText = im.captured(1);

    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (idText.isEmpty() && tokens.size() >= 2) {
        // ---- data pattern
        if (!isByteToken(tokens[0]) && !isWildToken(tokens[0]))
            return invalid(db ? tr("Not an ID, message, data pattern or signal condition.")
                              : tr("Not an ID or data pattern (load a database for signal conditions)."));
        if (tokens.size() > CanFrame::kMaxDlc)
            return invalid(tr("A data pattern has at most 8 bytes."));
        for (int i = 0; i < tokens.size(); ++i) {
            const QString &t = tokens[i];
            if (isWildToken(t)) {
                s.m_wild[static_cast<std::size_t>(i)] = true;
            } else if (isByteToken(t)) {
                s.m_bytes[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(t.toUInt(nullptr, 16));
            } else {
                return invalid(tr("\"%1\" is not a byte; use hex like 1F or ?? for any value.").arg(t));
            }
        }
        s.m_patternLength = static_cast<int>(tokens.size());
        s.m_kind = Kind::Data;
        QStringList shown;
        for (int i = 0; i < s.m_patternLength; ++i)
            shown << (s.m_wild[static_cast<std::size_t>(i)] ? QStringLiteral("??")
                                                            : QStringLiteral("%1").arg(s.m_bytes[static_cast<std::size_t>(i)], 2, 16, QLatin1Char('0')).toUpper());
        s.m_description = tr("Data %1 (from byte 0)").arg(shown.join(QLatin1Char(' ')));
        return s;
    }
    if (idText.isEmpty())
        idText = tokens.value(0);

    // ---- message name
    if (db) {
        for (const auto &m : db->messages) {
            if (m.name.compare(idText, Qt::CaseInsensitive) == 0) {
                s.m_kind = Kind::Id;
                s.m_id = m.id;
                s.m_idHasFormat = true;
                s.m_idExtended = m.extended;
                s.m_description = tr("Message %1 (%2)").arg(m.name, formatId(m.id, m.extended));
                return s;
            }
        }
    }
    // ---- hex id
    std::uint32_t id = 0;
    if (!parseHexId(idText, id))
        return invalid(db ? tr("Not an ID, message, data pattern or signal condition.")
                          : tr("Not an ID or data pattern (load a database for signal conditions)."));
    s.m_kind = Kind::Id;
    s.m_id = id;
    s.m_description = tr("ID %1").arg(formatId(id, id > CanFrame::kMaxStandardId));
    return s;
}

bool FrameSearch::matches(const CanFrame &f) const
{
    switch (m_kind) {
    case Kind::Id:
        return f.id == m_id && (!m_idHasFormat || f.extended == m_idExtended);
    case Kind::Data:
        if (f.dlc < m_patternLength)
            return false;
        for (int i = 0; i < m_patternLength; ++i) {
            const auto k = static_cast<std::size_t>(i);
            if (!m_wild[k] && f.data[k] != m_bytes[k])
                return false;
        }
        return true;
    case Kind::Signal: {
        if (f.key() != m_msgKey || !m_db)
            return false;
        const MessageDef *msg = m_db->find(f.id, f.extended);
        if (!msg)
            return false;
        for (const DecodedSignal &d : Decoder::decode(*msg, f)) {
            if (d.signalIndex != m_signalIndex)
                continue;
            const double v = d.physical;
            switch (m_op) {
            case Op::Greater: return v > m_value;
            case Op::GreaterEqual: return v >= m_value - m_tolerance;
            case Op::Less: return v < m_value;
            case Op::LessEqual: return v <= m_value + m_tolerance;
            case Op::Equal: return std::abs(v - m_value) <= m_tolerance;
            case Op::NotEqual: return std::abs(v - m_value) > m_tolerance;
            }
        }
        return false;
    }
    default:
        return false;
    }
}

} // namespace can
