#pragma once

#include "busstatistics.h"
#include "canframe.h"
#include "dbc.h"

#include <memory>

#include <QAbstractTableModel>

#include <unordered_map>
#include <vector>

namespace can {

// One row per (ID, format) with count, measured period, last data and the time
// the ID was last seen. Rows stay sorted by the chosen column.
class MessageOverviewModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)

public:
    enum Column {
        IdColumn,
        ChannelColumn,
        DlcColumn,
        CountColumn,
        PeriodColumn,
        LastSeenColumn,
        DataColumn,
        ColumnCount
    };
    Q_ENUM(Column)
    enum Role {
        IdRole = Qt::UserRole + 1,
        ExtendedRole,
        ChannelRole,
        DlcRole,
        CountRole,
        PeriodRole,     // formatted ms, "—" until measured
        PeriodMsRole,   // double
        LastSeenRole,   // formatted seconds
        DataRole,       // "01 02 .."
        BytesRole,      // QStringList, one entry per byte
        ChangedMaskRole,// bit i set: byte i changed in the most recent update
        ChangeSerialRole,// increases whenever ChangedMask is refreshed for the row
        MessageNameRole, // from the loaded database ("" if unknown)
        DecodedRole,     // decoded signals of the last frame (QVariantList)
    };

    explicit MessageOverviewModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    int count() const { return static_cast<int>(m_entries.size()); }
    int sortColumn() const { return m_sortColumn; }
    bool sortAscending() const { return m_sortOrder == Qt::AscendingOrder; }

    // Click-on-header behaviour: same column toggles direction.
    Q_INVOKABLE void sortBy(int column);

    void update(const CanFrame *frames, std::size_t count);
    void update(const std::vector<CanFrame> &frames) { update(frames.data(), frames.size()); }
    void clear();
    void setDatabase(std::shared_ptr<const Database> db);

    // For tests.
    struct Entry
    {
        CanFrame last;
        std::uint64_t count = 0;
        PeriodEstimator period;
        std::uint8_t changedMask = 0;
        std::uint64_t changeSerial = 0;
    };
    const Entry *entryFor(std::uint32_t id, bool extended) const;

    // Coarse sort key for live numeric columns (two significant digits).
    static double sortBucket(double value);

signals:
    void countChanged();
    void sortChanged();

private:
    bool lessThan(const Entry &a, const Entry &b) const;
    void resort();

    std::vector<Entry> m_entries;               // row order
    std::unordered_map<std::uint32_t, int> m_rowByKey;
    int m_sortColumn = IdColumn;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    std::shared_ptr<const Database> m_db;
};

} // namespace can
