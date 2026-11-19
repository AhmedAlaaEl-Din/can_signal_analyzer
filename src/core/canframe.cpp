#include "canframe.h"

namespace can {

namespace {
constexpr char kHex[] = "0123456789ABCDEF";
}

QString formatId(std::uint32_t id, bool extended)
{
    const int digits = extended ? 8 : 3;
    QString out(digits, Qt::Uninitialized);
    QChar *p = out.data();
    for (int i = digits - 1; i >= 0; --i) {
        p[i] = QLatin1Char(kHex[id & 0xF]);
        id >>= 4;
    }
    return out;
}

QString formatData(const CanFrame &frame)
{
    const int n = frame.dlc > CanFrame::kMaxDlc ? CanFrame::kMaxDlc : frame.dlc;
    if (n == 0)
        return {};
    QString out(n * 3 - 1, Qt::Uninitialized);
    QChar *p = out.data();
    for (int i = 0; i < n; ++i) {
        const auto b = frame.data[static_cast<std::size_t>(i)];
        *p++ = QLatin1Char(kHex[b >> 4]);
        *p++ = QLatin1Char(kHex[b & 0xF]);
        if (i + 1 < n)
            *p++ = QLatin1Char(' ');
    }
    return out;
}

QString formatTimestamp(std::int64_t timestampUs)
{
    const bool negative = timestampUs < 0;
    const std::uint64_t v = negative ? static_cast<std::uint64_t>(-timestampUs)
                                     : static_cast<std::uint64_t>(timestampUs);
    const std::uint64_t seconds = v / 1000000u;
    const std::uint64_t micros = v % 1000000u;
    QString frac = QString::number(micros).rightJustified(6, QLatin1Char('0'));
    return (negative ? QStringLiteral("-") : QString()) + QString::number(seconds)
           + QLatin1Char('.') + frac;
}

bool parseHexId(QStringView text, std::uint32_t &out)
{
    QStringView t = text.trimmed();
    if (t.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        t = t.mid(2);
    if (t.isEmpty() || t.size() > 8)
        return false;
    std::uint32_t v = 0;
    for (QChar c : t) {
        int d;
        if (c >= QLatin1Char('0') && c <= QLatin1Char('9'))
            d = c.unicode() - '0';
        else if (c >= QLatin1Char('a') && c <= QLatin1Char('f'))
            d = c.unicode() - 'a' + 10;
        else if (c >= QLatin1Char('A') && c <= QLatin1Char('F'))
            d = c.unicode() - 'A' + 10;
        else
            return false;
        v = (v << 4) | static_cast<std::uint32_t>(d);
    }
    if (v > CanFrame::kMaxExtendedId)
        return false;
    out = v;
    return true;
}

} // namespace can
