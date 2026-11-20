#pragma once

#include <QObject>

#include <cstdint>
#include <deque>
#include <vector>

namespace can {

struct TimeSample
{
    std::int64_t t; // microseconds
    double v;       // physical value
};

// Per-pixel-column summary returned by decimation. A column with count == 0
// has no data. min/max cover every sample in the column, so a one-sample
// spike always survives; first/last keep the line continuous.
struct ColumnStat
{
    int count = 0;
    double min = 0;
    double max = 0;
    double first = 0;
    double last = 0;
};

// Time-ordered samples of one signal in a growable ring buffer, plus block
// summaries (min/max per 64 consecutive samples) for fast zoomed-out queries.
class TimeSeries
{
public:
    static constexpr int kBlock = 64;

    void append(std::int64_t t, double v);
    void trimBefore(std::int64_t t);
    void clear();

    std::size_t size() const { return m_size; }
    bool empty() const { return m_size == 0; }
    const TimeSample &at(std::size_t i) const { return m_buf[(m_start + i) & (m_buf.size() - 1)]; }
    std::int64_t firstTime() const { return at(0).t; }
    std::int64_t lastTime() const { return at(m_size - 1).t; }
    std::size_t capacity() const { return m_buf.size(); }

    // Index of the first sample with time >= t (size() if none).
    std::size_t lowerBound(std::int64_t t) const;
    // Last sample at or before t.
    bool valueAt(std::int64_t t, double *value, std::int64_t *sampleTime = nullptr) const;

    // Summarise [t0, t1] into `columns` equal time slices.
    std::vector<ColumnStat> decimate(std::int64_t t0, std::int64_t t1, int columns) const;
    // Raw samples in [t0, t1], plus one neighbour on each side when present
    // (so lines run to the plot edges).
    void samples(std::int64_t t0, std::int64_t t1, std::vector<TimeSample> &out) const;

private:
    struct Block
    {
        std::int64_t t0, t1;
        double min, max, first, last;
    };
    void grow();
    std::uint64_t seqOf(std::size_t index) const { return m_firstSeq + index; }

    std::vector<TimeSample> m_buf; // capacity is a power of two
    std::size_t m_start = 0;
    std::size_t m_size = 0;
    std::uint64_t m_firstSeq = 0;  // absolute sequence of at(0)
    std::deque<Block> m_blocks;    // block k covers sequences [k*64, k*64+63]
    std::uint64_t m_firstBlock = 0;
};

// All signal histories of the session. Feeds: SignalListModel (decoded values).
// Readers: plot items, cursors, CSV export. No Qt Quick dependency.
class TimeSeriesStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 nowUs READ nowUs NOTIFY revisionChanged)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(qint64 historyUs READ historyUs WRITE setHistoryUs NOTIFY historyChanged)

public:
    static constexpr std::int64_t kDefaultHistoryUs = 600'000'000; // 10 minutes

    explicit TimeSeriesStore(QObject *parent = nullptr);

    void reset(int signalCount);
    void clear();
    void append(int signalId, std::int64_t t, double v);
    // Call once per batch: trims old samples and notifies readers.
    void commit();

    int signalCount() const { return static_cast<int>(m_series.size()); }
    const TimeSeries *series(int signalId) const;
    qint64 nowUs() const { return m_nowUs; }
    qulonglong revision() const { return m_revision; }
    qint64 historyUs() const { return m_historyUs; }
    void setHistoryUs(qint64 us);
    std::size_t totalSamples() const;

    Q_INVOKABLE QVariant valueAt(int signalId, qint64 timeUs) const;
    Q_INVOKABLE qint64 firstTimeUs() const;

signals:
    void revisionChanged();
    void historyChanged();

private:
    std::vector<TimeSeries> m_series;
    std::int64_t m_nowUs = 0;
    std::uint64_t m_revision = 0;
    std::int64_t m_historyUs = kDefaultHistoryUs;
    bool m_dirty = false;
};

} // namespace can
