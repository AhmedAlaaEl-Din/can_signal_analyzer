#include "dbc.h"

#include "signalcodec.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace can {

// ------------------------------------------------------------------ model

const SignalDef *MessageDef::multiplexor() const
{
    for (const auto &s : signalList)
        if (s.muxRole == SignalDef::MuxRole::Multiplexor)
            return &s;
    return nullptr;
}

const SignalDef *MessageDef::findSignal(const QString &signalName) const
{
    for (const auto &s : signalList)
        if (s.name == signalName)
            return &s;
    return nullptr;
}

const MessageDef *Database::find(std::uint32_t id, bool extended) const
{
    const int i = indexOf(id | (extended ? 0x80000000u : 0u));
    return i < 0 ? nullptr : &messages[static_cast<std::size_t>(i)];
}

int Database::indexOf(std::uint32_t key) const
{
    auto it = m_index.find(key);
    return it == m_index.end() ? -1 : it->second;
}

int Database::signalCount() const
{
    int n = 0;
    for (const auto &m : messages)
        n += static_cast<int>(m.signalList.size());
    return n;
}

void Database::rebuildIndex()
{
    m_index.clear();
    for (std::size_t i = 0; i < messages.size(); ++i)
        m_index[messages[i].key()] = static_cast<int>(i);
}

// ----------------------------------------------------------------- parser

namespace {

// Cursor over one statement (which may span several physical lines).
class Cursor
{
public:
    Cursor(const QString &text, int line) : m_text(text), m_line(line) {}

    int line() const { return m_line; }
    bool atEnd() { skipWs(); return m_pos >= m_text.size(); }
    QChar peek() { skipWs(); return m_pos < m_text.size() ? m_text[m_pos] : QChar(); }

    bool tryChar(QChar c)
    {
        skipWs();
        if (m_pos < m_text.size() && m_text[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }
    bool expectChar(QChar c, const QString &what, QString *error)
    {
        if (tryChar(c))
            return true;
        *error = QStringLiteral("expected '%1' %2").arg(c).arg(what);
        return false;
    }

    // Identifier / number-like token: everything up to whitespace or punctuation.
    QString word()
    {
        skipWs();
        const qsizetype start = m_pos;
        while (m_pos < m_text.size()) {
            const QChar c = m_text[m_pos];
            if (c.isSpace() || QStringLiteral(":|@()[],;\"").contains(c))
                break;
            ++m_pos;
        }
        return m_text.mid(start, m_pos - start);
    }

    bool string(QString *out)
    {
        skipWs();
        if (m_pos >= m_text.size() || m_text[m_pos] != QLatin1Char('"'))
            return false;
        ++m_pos;
        QString s;
        while (m_pos < m_text.size()) {
            const QChar c = m_text[m_pos++];
            if (c == QLatin1Char('\\') && m_pos < m_text.size()) {
                s += m_text[m_pos++];
                continue;
            }
            if (c == QLatin1Char('"')) {
                *out = s;
                return true;
            }
            s += c;
        }
        return false; // unterminated
    }

    bool number(double *out)
    {
        const QString w = word();
        bool ok = false;
        const double v = w.toDouble(&ok);
        if (ok)
            *out = v;
        return ok;
    }

    bool integer(qint64 *out)
    {
        const QString w = word();
        bool ok = false;
        const qint64 v = w.toLongLong(&ok, 10);
        if (ok)
            *out = v;
        return ok;
    }

    bool unsignedInt(quint64 *out)
    {
        const QString w = word();
        bool ok = false;
        const quint64 v = w.toULongLong(&ok, 10);
        if (ok)
            *out = v;
        return ok;
    }

private:
    void skipWs()
    {
        while (m_pos < m_text.size() && m_text[m_pos].isSpace())
            ++m_pos;
    }
    const QString &m_text;
    qsizetype m_pos = 0;
    int m_line;
};

// Statements that end with ';' and may therefore span several lines.
bool isSemicolonStatement(const QString &kw)
{
    static const QSet<QString> kws = {
        QStringLiteral("CM_"), QStringLiteral("VAL_"), QStringLiteral("BA_"),
        QStringLiteral("BA_DEF_"), QStringLiteral("BA_DEF_DEF_"), QStringLiteral("BA_DEF_SGTYPE_"),
        QStringLiteral("BA_SGTYPE_"), QStringLiteral("BA_DEF_REL_"), QStringLiteral("BA_REL_"),
        QStringLiteral("BA_DEF_DEF_REL_"), QStringLiteral("VAL_TABLE_"), QStringLiteral("SIG_VALTYPE_"),
        QStringLiteral("BO_TX_BU_"), QStringLiteral("EV_"), QStringLiteral("SG_MUL_VAL_"),
        QStringLiteral("SIG_GROUP_"), QStringLiteral("SIG_TYPE_REF_"), QStringLiteral("SGTYPE_"),
        QStringLiteral("SGTYPE_VAL_"), QStringLiteral("ENVVAR_DATA_"), QStringLiteral("CAT_"),
        QStringLiteral("CAT_DEF_"), QStringLiteral("FILTER"), QStringLiteral("SIG_TYPE_REF_"),
    };
    return kws.contains(kw);
}

// Keywords that are recognised but carry nothing we use.
bool isIgnoredKeyword(const QString &kw)
{
    static const QSet<QString> kws = {
        QStringLiteral("VERSION"), QStringLiteral("BS_"), QStringLiteral("BS_:"),
        QStringLiteral("BU_"), QStringLiteral("BU_:"), QStringLiteral("NS_DESC_"),
    };
    return kws.contains(kw) || (isSemicolonStatement(kw) && kw != QLatin1String("CM_")
                                && kw != QLatin1String("VAL_"));
}

// Count of '"' that are not escaped, to know whether a ';' is inside a string.
bool endsStatement(const QString &text)
{
    bool inString = false;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (c == QLatin1Char('\\') && inString) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('"'))
            inString = !inString;
        else if (c == QLatin1Char(';') && !inString)
            return true;
    }
    return false;
}

constexpr std::uint32_t kIndependentSignalsMsg = 0xC0000000u; // Vector__INDEPENDENT_SIG_MSG

class Parser
{
public:
    explicit Parser(ParseResult &r) : m_result(r) {}

    void run(const QString &text)
    {
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i];
            if (line.endsWith(QLatin1Char('\r')))
                line.chop(1);
            const int lineNo = i + 1;
            const bool indented = !line.isEmpty() && line[0].isSpace();
            const QString trimmed = line.trimmed();

            if (m_inNamespaceBlock) {
                if (indented || trimmed.isEmpty())
                    continue;
                m_inNamespaceBlock = false;
            }
            if (trimmed.isEmpty() || trimmed.startsWith(QLatin1String("//")))
                continue;

            const QString keyword = firstWord(trimmed);

            if (isSemicolonStatement(keyword)) {
                // Gather continuation lines until the terminating ';'.
                QString stmt = trimmed;
                const int startLine = lineNo;
                while (!endsStatement(stmt) && i + 1 < lines.size()) {
                    ++i;
                    QString next = lines[i];
                    if (next.endsWith(QLatin1Char('\r')))
                        next.chop(1);
                    stmt += QLatin1Char('\n') + next;
                }
                if (!endsStatement(stmt)) {
                    error(startLine, QStringLiteral("%1 statement is missing its closing ';'").arg(keyword));
                    continue;
                }
                if (keyword == QLatin1String("CM_"))
                    parseComment(stmt, startLine);
                else if (keyword == QLatin1String("VAL_"))
                    parseValueTable(stmt, startLine);
                else if (keyword == QLatin1String("BA_"))
                    parseAttribute(stmt, startLine);
                continue; // other ';' statements are ignored on purpose
            }

            if (keyword == QLatin1String("NS_") || keyword == QLatin1String("NS_:")) {
                m_inNamespaceBlock = true;
                continue;
            }
            if (keyword == QLatin1String("BO_")) {
                parseMessage(trimmed, lineNo);
                continue;
            }
            if (keyword == QLatin1String("SG_")) {
                parseSignal(trimmed, lineNo);
                continue;
            }
            if (isIgnoredKeyword(keyword))
                continue;
            warning(lineNo, QStringLiteral("unsupported keyword '%1' ignored").arg(keyword));
        }
        finish();
    }

private:
    static QString firstWord(const QString &s)
    {
        qsizetype end = 0;
        while (end < s.size() && !s[end].isSpace())
            ++end;
        QString w = s.left(end);
        return w;
    }

    void error(int line, const QString &msg) { m_result.errors.push_back({line, msg}); }
    void warning(int line, const QString &msg) { m_result.warnings.push_back({line, msg}); }

    MessageDef *messageByRawId(quint64 rawId)
    {
        const bool ext = (rawId & 0x80000000u) != 0;
        const auto id = static_cast<std::uint32_t>(rawId & 0x1FFFFFFFu);
        for (auto &m : m_db.messages)
            if (m.id == id && m.extended == ext)
                return &m;
        return nullptr;
    }

    void parseMessage(const QString &text, int line)
    {
        m_current = nullptr;
        m_skipSignals = false;
        Cursor c(text, line);
        c.word(); // BO_
        quint64 rawId = 0;
        if (!c.unsignedInt(&rawId)) {
            error(line, QStringLiteral("BO_: message ID must be a decimal number"));
            return;
        }
        if (rawId == kIndependentSignalsMsg) { // tool-specific pseudo message
            m_skipSignals = true;
            return;
        }
        MessageDef m;
        m.line = line;
        m.extended = (rawId & 0x80000000u) != 0;
        m.id = static_cast<std::uint32_t>(rawId & ~0x80000000ull);
        if (m.extended ? m.id > 0x1FFFFFFFu : m.id > 0x7FFu) {
            error(line, QStringLiteral("BO_: ID %1 is out of range for a %2 frame")
                            .arg(rawId).arg(m.extended ? QStringLiteral("29-bit") : QStringLiteral("11-bit")));
            return;
        }
        m.name = c.word();
        if (m.name.isEmpty()) {
            error(line, QStringLiteral("BO_: message name is missing"));
            return;
        }
        QString err;
        if (!c.expectChar(QLatin1Char(':'), QStringLiteral("after the message name"), &err)) {
            error(line, QStringLiteral("BO_ %1: %2").arg(m.name, err));
            return;
        }
        qint64 dlc = 0;
        if (!c.integer(&dlc) || dlc < 0) {
            error(line, QStringLiteral("BO_ %1: DLC must be a number").arg(m.name));
            return;
        }
        if (dlc > 8) {
            error(line, QStringLiteral("BO_ %1: DLC %2 is CAN FD; only classic CAN (0-8 bytes) is supported")
                            .arg(m.name).arg(dlc));
            return;
        }
        m.dlc = static_cast<int>(dlc);
        m.transmitter = c.word();
        if (messageByRawId(rawId)) {
            error(line, QStringLiteral("BO_ %1: message ID %2 is defined twice").arg(m.name).arg(rawId));
            return;
        }
        m_db.messages.push_back(m);
        m_current = &m_db.messages.back();
    }

    void parseSignal(const QString &text, int line)
    {
        if (m_skipSignals)
            return;
        if (!m_current) {
            error(line, QStringLiteral("SG_ appears outside of a BO_ message block"));
            return;
        }
        Cursor c(text, line);
        c.word(); // SG_
        SignalDef s;
        s.line = line;
        s.name = c.word();
        if (s.name.isEmpty()) {
            error(line, QStringLiteral("SG_: signal name is missing"));
            return;
        }
        const QString where = QStringLiteral("SG_ %1").arg(s.name);
        QString err;

        // Optional multiplexer indicator: "M" or "m<value>".
        if (c.peek() != QLatin1Char(':')) {
            const QString mux = c.word();
            if (mux == QLatin1String("M")) {
                s.muxRole = SignalDef::MuxRole::Multiplexor;
            } else if (mux.size() > 1 && mux[0] == QLatin1Char('m')) {
                if (mux.endsWith(QLatin1Char('M'))) {
                    error(line, QStringLiteral("%1: extended multiplexing ('%2') is not supported").arg(where, mux));
                    return;
                }
                bool ok = false;
                s.muxValue = mux.mid(1).toInt(&ok);
                if (!ok || s.muxValue < 0) {
                    error(line, QStringLiteral("%1: invalid multiplexer indicator '%2'").arg(where, mux));
                    return;
                }
                s.muxRole = SignalDef::MuxRole::Multiplexed;
            } else {
                error(line, QStringLiteral("%1: expected ':' after the signal name, found '%2'").arg(where, mux));
                return;
            }
        }
        if (!c.expectChar(QLatin1Char(':'), QStringLiteral("after the signal name"), &err)) {
            error(line, QStringLiteral("%1: %2").arg(where, err));
            return;
        }

        qint64 start = 0, length = 0;
        if (!c.integer(&start)) {
            error(line, QStringLiteral("%1: start bit must be a number").arg(where));
            return;
        }
        if (!c.expectChar(QLatin1Char('|'), QStringLiteral("between start bit and length"), &err)) {
            error(line, QStringLiteral("%1: %2").arg(where, err));
            return;
        }
        // "8@1+" is read as: length "8", then '@', then "1+"
        if (!c.integer(&length)) {
            error(line, QStringLiteral("%1: length must be a number").arg(where));
            return;
        }
        if (!c.expectChar(QLatin1Char('@'), QStringLiteral("before the byte order"), &err)) {
            error(line, QStringLiteral("%1: %2").arg(where, err));
            return;
        }
        const QString orderSign = c.word();
        if (orderSign.size() != 2 || (orderSign[0] != QLatin1Char('0') && orderSign[0] != QLatin1Char('1'))
            || (orderSign[1] != QLatin1Char('+') && orderSign[1] != QLatin1Char('-'))) {
            error(line, QStringLiteral("%1: byte order/sign must be @0+, @0-, @1+ or @1- (found '@%2')")
                            .arg(where, orderSign));
            return;
        }
        s.byteOrder = orderSign[0] == QLatin1Char('1') ? ByteOrder::Intel : ByteOrder::Motorola;
        s.isSigned = orderSign[1] == QLatin1Char('-');

        if (!c.expectChar(QLatin1Char('('), QStringLiteral("before factor,offset"), &err)
            || !c.number(&s.factor)
            || !c.expectChar(QLatin1Char(','), QStringLiteral("between factor and offset"), &err)
            || !c.number(&s.offset)
            || !c.expectChar(QLatin1Char(')'), QStringLiteral("after factor,offset"), &err)) {
            error(line, QStringLiteral("%1: %2").arg(where, err.isEmpty() ? QStringLiteral("invalid (factor,offset)") : err));
            return;
        }
        err.clear();
        if (!c.expectChar(QLatin1Char('['), QStringLiteral("before min|max"), &err)
            || !c.number(&s.minimum)
            || !c.expectChar(QLatin1Char('|'), QStringLiteral("between min and max"), &err)
            || !c.number(&s.maximum)
            || !c.expectChar(QLatin1Char(']'), QStringLiteral("after min|max"), &err)) {
            error(line, QStringLiteral("%1: %2").arg(where, err.isEmpty() ? QStringLiteral("invalid [min|max]") : err));
            return;
        }
        if (!c.string(&s.unit)) {
            error(line, QStringLiteral("%1: unit must be a quoted string").arg(where));
            return;
        }
        while (!c.atEnd()) {
            const QString r = c.word();
            if (r.isEmpty()) {
                c.tryChar(QLatin1Char(','));
                continue;
            }
            s.receivers << r;
            c.tryChar(QLatin1Char(','));
        }

        if (length < 1 || length > 64) {
            error(line, QStringLiteral("%1: length %2 is out of range (1-64)").arg(where).arg(length));
            return;
        }
        if (start < 0 || start > 63) {
            error(line, QStringLiteral("%1: start bit %2 is out of range (0-63)").arg(where).arg(start));
            return;
        }
        s.startBit = static_cast<int>(start);
        s.length = static_cast<int>(length);
        if (!codec::fits(s.startBit, s.length, s.byteOrder, m_current->dlc)) {
            error(line, QStringLiteral("%1: bits %2..%3 (%4) do not fit into the %5-byte message %6")
                            .arg(where).arg(start).arg(length)
                            .arg(s.byteOrder == ByteOrder::Intel ? QStringLiteral("Intel") : QStringLiteral("Motorola"))
                            .arg(m_current->dlc).arg(m_current->name));
            return;
        }
        if (s.factor == 0.0)
            warning(line, QStringLiteral("%1: factor is 0, all values decode to the offset").arg(where));
        if (m_current->findSignal(s.name)) {
            error(line, QStringLiteral("%1: signal name is used twice in message %2").arg(where, m_current->name));
            return;
        }
        if (s.muxRole == SignalDef::MuxRole::Multiplexor && m_current->multiplexor()) {
            error(line, QStringLiteral("%1: message %2 already has a multiplexor").arg(where, m_current->name));
            return;
        }
        m_current->signalList.push_back(s);
    }

    void parseComment(const QString &stmt, int line)
    {
        Cursor c(stmt, line);
        c.word(); // CM_
        QString text;
        if (c.peek() == QLatin1Char('"')) {
            if (!c.string(&text))
                error(line, QStringLiteral("CM_: unterminated comment string"));
            else
                m_db.comment = text;
            return;
        }
        const QString kind = c.word();
        if (kind == QLatin1String("BO_") || kind == QLatin1String("SG_")) {
            quint64 rawId = 0;
            if (!c.unsignedInt(&rawId)) {
                error(line, QStringLiteral("CM_ %1: message ID must be a decimal number").arg(kind));
                return;
            }
            QString sigName;
            if (kind == QLatin1String("SG_"))
                sigName = c.word();
            if (!c.string(&text)) {
                error(line, QStringLiteral("CM_ %1: comment must be a quoted string").arg(kind));
                return;
            }
            m_pendingComments.push_back({line, rawId, sigName, text});
            return;
        }
        // BU_ / EV_ comments: syntactically fine, not used.
        c.word();
        if (!c.string(&text))
            error(line, QStringLiteral("CM_ %1: comment must be a quoted string").arg(kind));
    }

    void parseValueTable(const QString &stmt, int line)
    {
        Cursor c(stmt, line);
        c.word(); // VAL_
        const QString first = c.word();
        bool isId = false;
        const quint64 rawId = first.toULongLong(&isId, 10);
        if (!isId)
            return; // value table for an environment variable: not used
        PendingValues pv{line, rawId, c.word(), {}};
        while (!c.atEnd() && c.peek() != QLatin1Char(';')) {
            qint64 value = 0;
            if (!c.integer(&value)) {
                error(line, QStringLiteral("VAL_ %1 %2: value must be an integer").arg(rawId).arg(pv.signal));
                return;
            }
            QString text;
            if (!c.string(&text)) {
                error(line, QStringLiteral("VAL_ %1 %2: description must be a quoted string").arg(rawId).arg(pv.signal));
                return;
            }
            pv.values[value] = text;
        }
        m_pendingValues.push_back(pv);
    }

    // Only BA_ "GenMsgCycleTime" BO_ <id> <ms>; is used; other attributes are skipped.
    void parseAttribute(const QString &stmt, int line)
    {
        Cursor c(stmt, line);
        c.word(); // BA_
        QString name;
        if (!c.string(&name) || name != QLatin1String("GenMsgCycleTime"))
            return;
        if (c.word() != QLatin1String("BO_"))
            return;
        quint64 rawId = 0;
        double ms = 0;
        if (!c.unsignedInt(&rawId) || !c.number(&ms)) {
            warning(line, QStringLiteral("BA_ GenMsgCycleTime: expected a message ID and a number"));
            return;
        }
        m_pendingCycles.push_back({line, rawId, static_cast<int>(ms)});
    }

    void finish()
    {
        for (const auto &pc : m_pendingCycles) {
            if (MessageDef *m = messageByRawId(pc.rawId))
                m->cycleTimeMs = std::max(0, pc.ms);
        }
        for (const auto &pc : m_pendingComments) {
            if ((pc.rawId & 0xFFFFFFFFull) == kIndependentSignalsMsg)
                continue;
            MessageDef *m = messageByRawId(pc.rawId);
            if (!m) {
                warning(pc.line, QStringLiteral("CM_ refers to unknown message ID %1").arg(pc.rawId));
                continue;
            }
            if (pc.signal.isEmpty()) {
                m->comment = pc.text;
                continue;
            }
            auto it = std::find_if(m->signalList.begin(), m->signalList.end(),
                                   [&](const SignalDef &s) { return s.name == pc.signal; });
            if (it == m->signalList.end())
                warning(pc.line, QStringLiteral("CM_ refers to unknown signal %1 in %2").arg(pc.signal, m->name));
            else
                it->comment = pc.text;
        }
        for (const auto &pv : m_pendingValues) {
            if ((pv.rawId & 0xFFFFFFFFull) == kIndependentSignalsMsg)
                continue;
            MessageDef *m = messageByRawId(pv.rawId);
            auto *s = m ? const_cast<SignalDef *>(m->findSignal(pv.signal)) : nullptr;
            if (!s) {
                warning(pv.line, QStringLiteral("VAL_ refers to unknown signal %1 (message ID %2)")
                                     .arg(pv.signal).arg(pv.rawId));
                continue;
            }
            s->valueTable = pv.values;
        }
        for (const auto &m : m_db.messages) {
            bool hasMultiplexed = false;
            for (const auto &s : m.signalList)
                hasMultiplexed |= s.muxRole == SignalDef::MuxRole::Multiplexed;
            if (hasMultiplexed && !m.multiplexor())
                error(m.line, QStringLiteral("BO_ %1 has multiplexed signals but no multiplexor (M)").arg(m.name));
        }
        if (m_result.errors.empty()) {
            m_db.rebuildIndex();
            m_result.database = std::move(m_db);
        }
    }

    struct PendingComment
    {
        int line;
        quint64 rawId;
        QString signal;
        QString text;
    };
    struct PendingValues
    {
        int line;
        quint64 rawId;
        QString signal;
        std::map<std::int64_t, QString> values;
    };

    ParseResult &m_result;
    Database m_db;
    MessageDef *m_current = nullptr; // valid until the next push_back (we reset it on BO_)
    bool m_skipSignals = false;
    bool m_inNamespaceBlock = false;
    std::vector<PendingComment> m_pendingComments;
    std::vector<PendingValues> m_pendingValues;
    struct PendingCycle
    {
        int line;
        quint64 rawId;
        int ms;
    };
    std::vector<PendingCycle> m_pendingCycles;
};

} // namespace

ParseResult DbcParser::parse(const QString &text, const QString &sourceName)
{
    ParseResult result;
    Parser parser(result);
    parser.run(text);
    if (result.database) {
        result.database->sourceName = sourceName;
        if (result.database->messages.empty()) {
            result.errors.push_back({0, QStringLiteral("the file contains no messages (BO_)")});
            result.database.reset();
        }
    }
    return result;
}

ParseResult DbcParser::parseFile(const QString &path)
{
    QFile f(path);
    const QString name = QFileInfo(path).fileName();
    if (!f.open(QIODevice::ReadOnly)) {
        ParseResult r;
        r.errors.push_back({0, QStringLiteral("cannot open %1: %2").arg(name, f.errorString())});
        return r;
    }
    const QByteArray bytes = f.readAll();
    // DBC files are usually Latin-1/Windows-1252; accept UTF-8 when it is valid.
    QString text = QString::fromUtf8(bytes);
    if (text.contains(QChar::ReplacementCharacter))
        text = QString::fromLatin1(bytes);
    return parse(text, name);
}

std::shared_ptr<const Database> demoDatabase()
{
    static const std::shared_ptr<const Database> db = [] {
        ParseResult r = DbcParser::parse(demoDatabaseText(), QStringLiteral("demo_vehicle.dbc"));
        Q_ASSERT_X(r.ok(), "demoDatabase", "the bundled demo database must parse");
        return std::make_shared<const Database>(r.ok() ? std::move(*r.database) : Database{});
    }();
    return db;
}

QString demoDatabaseText()
{
    QFile f(QStringLiteral(":/can/demo_vehicle.dbc"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

} // namespace can
