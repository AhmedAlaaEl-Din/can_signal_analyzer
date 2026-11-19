#include "framefilter.h"

#include <QRegularExpression>

#include <algorithm>

namespace can {

namespace {

QStringList tokenize(const QString &text)
{
    QString normalized = text.trimmed();
    // "100 - 1FF" -> "100-1FF" so ranges survive splitting on spaces.
    static const QRegularExpression dashSpaces(QStringLiteral("\\s*-\\s*"));
    normalized.replace(dashSpaces, QStringLiteral("-"));
    static const QRegularExpression separators(QStringLiteral("[,;\\s]+"));
    return normalized.split(separators, Qt::SkipEmptyParts);
}

template <typename ParseOne>
bool parseRanges(const QString &text, std::vector<FrameFilter::Range> &out, QString *error,
                 ParseOne parseOne, const QString &what)
{
    std::vector<FrameFilter::Range> result;
    for (const QString &token : tokenize(text)) {
        const qsizetype dash = token.indexOf(QLatin1Char('-'));
        std::uint32_t lo = 0;
        std::uint32_t hi = 0;
        bool ok;
        if (dash < 0) {
            ok = parseOne(QStringView(token), lo);
            hi = lo;
        } else {
            ok = parseOne(QStringView(token).left(dash), lo)
                 && parseOne(QStringView(token).mid(dash + 1), hi);
        }
        if (!ok) {
            if (error)
                *error = QStringLiteral("“%1” is not a valid %2").arg(token, what);
            return false;
        }
        if (lo > hi)
            std::swap(lo, hi);
        result.push_back({lo, hi});
    }
    out = std::move(result);
    return true;
}

bool parseDlc(QStringView text, std::uint32_t &out)
{
    bool ok = false;
    const int v = text.trimmed().toInt(&ok, 10);
    if (!ok || v < 0 || v > CanFrame::kMaxDlc)
        return false;
    out = static_cast<std::uint32_t>(v);
    return true;
}

bool inRanges(const std::vector<FrameFilter::Range> &ranges, std::uint32_t v)
{
    return std::any_of(ranges.begin(), ranges.end(),
                       [v](const FrameFilter::Range &r) { return v >= r.lo && v <= r.hi; });
}

} // namespace

bool FrameFilter::parseIdList(const QString &text, std::vector<Range> &out, QString *error)
{
    return parseRanges(text, out, error, &parseHexId, QStringLiteral("hex ID"));
}

bool FrameFilter::parseDlcList(const QString &text, std::vector<Range> &out, QString *error)
{
    return parseRanges(text, out, error, &parseDlc, QStringLiteral("length (0–8)"));
}

bool FrameFilter::setIdExpression(const QString &text, QString *error)
{
    return parseIdList(text, m_ids, error);
}

bool FrameFilter::setDlcExpression(const QString &text, QString *error)
{
    return parseDlcList(text, m_dlcs, error);
}

bool FrameFilter::matches(const CanFrame &frame) const
{
    if (m_format == Format::Standard && frame.extended)
        return false;
    if (m_format == Format::Extended && !frame.extended)
        return false;
    if (!m_ids.empty() && !inRanges(m_ids, frame.id))
        return false;
    if (!m_dlcs.empty() && !inRanges(m_dlcs, frame.dlc))
        return false;
    return true;
}

} // namespace can
