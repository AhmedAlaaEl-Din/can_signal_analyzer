#include "timeseriesstore.h"

#include <QVariant>

#include <algorithm>
#include <cmath>

namespace can {

// ------------------------------------------------------------- TimeSeries

void TimeSeries::grow()
{
    const std::size_t newCap = m_buf.empty() ? 256 : m_buf.size() * 2;
    std::vector<TimeSample> n(newCap);
    for (std::size_t i = 0; i < m_size; ++i)
        n[i] = at(i);
    m_buf = std::move(n);
    m_start = 0;
}

void TimeSeries::append(std::int64_t t, double v)
{
    if (m_size > 0 && t < lastTime())
        t = lastTime(); // keep the series ordered (never expected, but cheap insurance)
    if (m_size == m_buf.size())
        grow();
    m_buf[(m_start + m_size) & (m_buf.size() - 1)] = {t, v};
    const std::uint64_t seq = seqOf(m_size);
    ++m_size;

    const std::uint64_t block = seq / kBlock;
    if (m_blocks.empty() || m_firstBlock + m_blocks.size() <= block) {
        if (m_blocks.empty())
            m_firstBlock = block;
        m_blocks.push_back({t, t, v, v, v, v});
    } else {
        Block &b = m_blocks.back();
        b.t1 = t;
        b.min = std::min(b.min, v);
        b.max = std::max(b.max, v);
        b.last = v;
    }
}

void TimeSeries::trimBefore(std::int64_t t)
{
    std::size_t drop = lowerBound(t);
    if (drop == 0)
        return;
    m_start = (m_start + drop) & (m_buf.size() - 1);
    m_size -= drop;
    m_firstSeq += drop;
    // Blocks entirely before the first remaining sequence are dropped; a
    // partially trimmed first block is ignored by decimate().
    while (!m_blocks.empty() && (m_firstBlock + 1) * kBlock <= m_firstSeq) {
        m_blocks.pop_front();
        ++m_firstBlock;
    }
}

void TimeSeries::clear()
{
    m_firstSeq += m_size;
    m_size = 0;
    m_start = 0;
    m_blocks.clear();
    m_firstBlock = m_firstSeq / kBlock;
}

std::size_t TimeSeries::lowerBound(std::int64_t t) const
{
    std::size_t lo = 0, hi = m_size;
    while (lo < hi) {
        const std::size_t mid = (lo + hi) / 2;
        if (at(mid).t < t)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

bool TimeSeries::valueAt(std::int64_t t, double *value, std::int64_t *sampleTime) const
{
    if (m_size == 0 || t < firstTime())
        return false;
    std::size_t i = lowerBound(t + 1); // first sample after t
    const TimeSample &s = at(i - 1);
    if (value)
        *value = s.v;
    if (sampleTime)
        *sampleTime = s.t;
    return true;
}

void TimeSeries::samples(std::int64_t t0, std::int64_t t1, std::vector<TimeSample> &out) const
{
    out.clear();
    if (m_size == 0 || t1 < t0)
        return;
    std::size_t i = lowerBound(t0);
    std::size_t j = lowerBound(t1 + 1);
    if (i > 0)
        --i;              // neighbour before the range
    if (j < m_size)
        ++j;              // neighbour after the range
    out.reserve(j - i);
    for (std::size_t k = i; k < j; ++k)
        out.push_back(at(k));
}

std::vector<ColumnStat> TimeSeries::decimate(std::int64_t t0, std::int64_t t1, int columns) const
{
    std::vector<ColumnStat> cols(static_cast<std::size_t>(std::max(columns, 0)));
    if (columns <= 0 || m_size == 0 || t1 <= t0)
        return cols;
    const std::size_t i0 = lowerBound(t0);
    const std::size_t i1 = lowerBound(t1 + 1); // exclusive
    if (i0 >= i1)
        return cols;
    const double colUs = static_cast<double>(t1 - t0) / columns;
    auto columnOf = [&](std::int64_t t) {
        const int c = static_cast<int>(static_cast<double>(t - t0) / colUs);
        return std::clamp(c, 0, columns - 1);
    };
    auto add = [&](int c, double mn, double mx, double first, double last, int n) {
        ColumnStat &s = cols[static_cast<std::size_t>(c)];
        if (s.count == 0) {
            s.min = mn;
            s.max = mx;
            s.first = first;
        } else {
            s.min = std::min(s.min, mn);
            s.max = std::max(s.max, mx);
        }
        s.last = last;
        s.count += n;
    };

    // Use block summaries when a column holds many blocks' worth of samples.
    const bool useBlocks = static_cast<double>(i1 - i0) / columns >= 2.0 * kBlock;
    std::size_t i = i0;
    while (i < i1) {
        const std::uint64_t seq = seqOf(i);
        if (useBlocks && seq % kBlock == 0 && i + kBlock <= i1 && seq / kBlock >= m_firstBlock) {
            const Block &b = m_blocks[static_cast<std::size_t>(seq / kBlock - m_firstBlock)];
            const int c0 = columnOf(b.t0);
            const int c1 = columnOf(b.t1);
            if (c0 == c1) { // whole block inside one column
                add(c0, b.min, b.max, b.first, b.last, kBlock);
                i += kBlock;
                continue;
            }
        }
        const TimeSample &s = at(i);
        add(columnOf(s.t), s.v, s.v, s.v, s.v, 1);
        ++i;
    }
    return cols;
}

// -------------------------------------------------------- TimeSeriesStore

TimeSeriesStore::TimeSeriesStore(QObject *parent)
    : QObject(parent)
{}

void TimeSeriesStore::reset(int signalCount)
{
    m_series.assign(static_cast<std::size_t>(std::max(0, signalCount)), TimeSeries{});
    m_nowUs = 0;
    ++m_revision;
    emit revisionChanged();
}

void TimeSeriesStore::clear()
{
    for (auto &s : m_series)
        s.clear();
    m_nowUs = 0;
    ++m_revision;
    emit revisionChanged();
}

void TimeSeriesStore::append(int signalId, std::int64_t t, double v)
{
    if (signalId < 0 || signalId >= signalCount())
        return;
    m_series[static_cast<std::size_t>(signalId)].append(t, v);
    m_nowUs = std::max(m_nowUs, t);
    m_dirty = true;
}

void TimeSeriesStore::commit()
{
    if (!m_dirty)
        return;
    m_dirty = false;
    const std::int64_t cutoff = m_nowUs - m_historyUs;
    for (auto &s : m_series)
        if (!s.empty() && s.firstTime() < cutoff)
            s.trimBefore(cutoff);
    ++m_revision;
    emit revisionChanged();
}

const TimeSeries *TimeSeriesStore::series(int signalId) const
{
    if (signalId < 0 || signalId >= signalCount())
        return nullptr;
    return &m_series[static_cast<std::size_t>(signalId)];
}

void TimeSeriesStore::setHistoryUs(qint64 us)
{
    us = std::max<qint64>(1'000'000, us);
    if (us == m_historyUs)
        return;
    m_historyUs = us;
    emit historyChanged();
}

std::size_t TimeSeriesStore::totalSamples() const
{
    std::size_t n = 0;
    for (const auto &s : m_series)
        n += s.size();
    return n;
}

QVariant TimeSeriesStore::valueAt(int signalId, qint64 timeUs) const
{
    double v = 0;
    const TimeSeries *s = series(signalId);
    if (!s || !s->valueAt(timeUs, &v))
        return {};
    return v;
}

qint64 TimeSeriesStore::firstTimeUs() const
{
    qint64 first = m_nowUs;
    for (const auto &s : m_series)
        if (!s.empty())
            first = std::min<qint64>(first, s.firstTime());
    return first;
}

} // namespace can
