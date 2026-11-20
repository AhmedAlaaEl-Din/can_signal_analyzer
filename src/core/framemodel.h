#pragma once

#include "canframe.h"
#include "framefilter.h"
#include "ringbuffer.h"
#include "dbc.h"

#include <memory>

#include <QAbstractTableModel>

#include <deque>
#include <unordered_map>
#include <vector>

namespace can {

// Trace model: every received frame is stored in a fixed-size ring buffer
// (oldest frames are evicted). The model exposes the subset that passes the
// current FrameFilter, oldest first. Rows are referenced by the frame's
// sequence number so that eviction and filtering stay cheap: appending a batch
// costs O(batch), re-filtering costs O(buffer).
class FrameModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int bufferedCount READ bufferedCount NOTIFY countChanged)
    Q_PROPERTY(int capacity READ capacity WRITE setCapacity NOTIFY capacityChanged)

public:
    enum Column { TimeColumn, ChannelColumn, IdColumn, DeltaColumn, DlcColumn, DataColumn, ColumnCount };
    enum Role {
        TimeRole = Qt::UserRole + 1,
        ChannelRole,
        IdRole,
        ExtendedRole,
        DlcRole,
        DataRole,
        SeqRole,     // stable identity of the row (sequence number)
        OrdinalRole, // stable running index inside the filtered view (zebra striping)
        FreshRole,   // true for rows added by the most recent append()
        DeltaRole,   // time since the previous frame with the same ID, ms text
        MessageNameRole, // name from the loaded database ("" if unknown)
    };
    Q_ENUM(Role)

    static constexpr int kDefaultCapacity = 100000;

    explicit FrameModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_rows.size()); }
    int bufferedCount() const { return static_cast<int>(m_buffer.size()); }
    int capacity() const { return static_cast<int>(m_buffer.capacity()); }
    void setCapacity(int capacity);

    void append(const CanFrame *frames, std::size_t count);
    void append(const std::vector<CanFrame> &frames) { append(frames.data(), frames.size()); }
    void setFilter(const FrameFilter &filter);
    const FrameFilter &filter() const { return m_filter; }
    void clear();

    // Direct access for tests and C++ callers. row is a view row.
    const CanFrame &frameAt(int row) const;
    // Microseconds since the previous frame with the same ID (-1 for the first one).
    std::int64_t deltaAt(int row) const;
    Q_INVOKABLE int rowForSeq(qulonglong seq) const; // -1 if not visible

    void setDatabase(std::shared_ptr<const Database> db);
    // Decoded signals of a view row (empty without a database / unknown ID).
    Q_INVOKABLE QVariantList decodedSignals(int row) const;

signals:
    void countChanged();
    void capacityChanged();
    // Emitted after a batch removed rows from the top (eviction). Views that
    // are not following the live end use it to keep their visible rows still.
    void rowsEvicted(int count);

private:
    void rebuild();

    struct Entry
    {
        CanFrame frame;
        std::int64_t deltaUs = -1;
    };

    RingBuffer<Entry> m_buffer;
    std::unordered_map<std::uint32_t, std::int64_t> m_lastSeenUs; // per ID key
    std::deque<std::uint64_t> m_rows; // sequence numbers of visible frames
    std::uint64_t m_removedFromFront = 0; // for stable ordinals
    std::uint64_t m_freshFromSeq = 0;
    FrameFilter m_filter;
    std::shared_ptr<const Database> m_db;
};

} // namespace can
