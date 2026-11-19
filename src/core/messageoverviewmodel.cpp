#include "messageoverviewmodel.h"

#include <algorithm>
#include <numeric>

namespace can {

MessageOverviewModel::MessageOverviewModel(QObject *parent)
    : QAbstractTableModel(parent)
{}

int MessageOverviewModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

int MessageOverviewModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

const MessageOverviewModel::Entry *MessageOverviewModel::entryFor(std::uint32_t id, bool extended) const
{
    CanFrame probe;
    probe.id = id;
    probe.extended = extended;
    auto it = m_rowByKey.find(probe.key());
    return it == m_rowByKey.end() ? nullptr : &m_entries[static_cast<std::size_t>(it->second)];
}

QVariant MessageOverviewModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const Entry &e = m_entries[static_cast<std::size_t>(index.row())];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case IdColumn: role = IdRole; break;
        case ChannelColumn: role = ChannelRole; break;
        case DlcColumn: role = DlcRole; break;
        case CountColumn: role = CountRole; break;
        case PeriodColumn: role = PeriodRole; break;
        case LastSeenColumn: role = LastSeenRole; break;
        case DataColumn: role = DataRole; break;
        default: return {};
        }
    }
    switch (role) {
    case IdRole: return formatId(e.last.id, e.last.extended);
    case ExtendedRole: return e.last.extended;
    case ChannelRole: return int(e.last.channel);
    case DlcRole: return int(e.last.dlc);
    case CountRole: return QVariant::fromValue<qulonglong>(e.count);
    case PeriodRole:
        return e.period.samples() >= 2 ? QString::number(e.period.periodMs(), 'f', 1)
                                       : QStringLiteral("—");
    case PeriodMsRole: return e.period.periodMs();
    case LastSeenRole: {
        const QString s = formatTimestamp(e.last.timestampUs);
        return s.left(s.size() - 3); // millisecond resolution is enough here
    }
    case DataRole: return formatData(e.last);
    case BytesRole: {
        QStringList bytes;
        const QString text = formatData(e.last);
        for (int i = 0; i < e.last.dlc; ++i)
            bytes << text.mid(i * 3, 2);
        return bytes;
    }
    case ChangedMaskRole: return int(e.changedMask);
    case ChangeSerialRole: return QVariant::fromValue<qulonglong>(e.changeSerial);
    default: return {};
    }
}

QVariant MessageOverviewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case IdColumn: return QStringLiteral("ID");
    case ChannelColumn: return QStringLiteral("Ch");
    case DlcColumn: return QStringLiteral("DLC");
    case CountColumn: return QStringLiteral("Count");
    case PeriodColumn: return QStringLiteral("Period (ms)");
    case LastSeenColumn: return QStringLiteral("Last seen (s)");
    case DataColumn: return QStringLiteral("Data");
    default: return {};
    }
}

QHash<int, QByteArray> MessageOverviewModel::roleNames() const
{
    auto names = QAbstractTableModel::roleNames();
    names.insert({
        {IdRole, "frameId"},
        {ExtendedRole, "extended"},
        {ChannelRole, "channel"},
        {DlcRole, "dlc"},
        {CountRole, "frameCount"},
        {PeriodRole, "period"},
        {PeriodMsRole, "periodMs"},
        {LastSeenRole, "lastSeen"},
        {DataRole, "payload"},
        {BytesRole, "bytes"},
        {ChangedMaskRole, "changedMask"},
        {ChangeSerialRole, "changeSerial"},
    });
    return names;
}

bool MessageOverviewModel::lessThan(const Entry &a, const Entry &b) const
{
    auto idKey = [](const Entry &e) {
        // Standard IDs first, then extended, each ascending.
        return (std::uint64_t(e.last.extended) << 32) | e.last.id;
    };
    auto cmp = [&](auto x, auto y) {
        if (x == y)
            return idKey(a) < idKey(b); // stable tie-break
        return m_sortOrder == Qt::AscendingOrder ? x < y : x > y;
    };
    switch (m_sortColumn) {
    case ChannelColumn: return cmp(a.last.channel, b.last.channel);
    case DlcColumn: return cmp(a.last.dlc, b.last.dlc);
    case CountColumn: return cmp(a.count, b.count);
    case PeriodColumn: return cmp(a.period.periodMs(), b.period.periodMs());
    case LastSeenColumn: return cmp(a.last.timestampUs, b.last.timestampUs);
    case DataColumn: return cmp(a.last.data, b.last.data);
    case IdColumn:
    default:
        return m_sortOrder == Qt::AscendingOrder ? idKey(a) < idKey(b) : idKey(a) > idKey(b);
    }
}

void MessageOverviewModel::resort()
{
    std::vector<int> order(m_entries.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [this](int x, int y) {
        return lessThan(m_entries[static_cast<std::size_t>(x)], m_entries[static_cast<std::size_t>(y)]);
    });
    if (std::is_sorted(order.begin(), order.end()))
        return; // already in order: no layout change, no flicker

    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    const QModelIndexList oldPersistent = persistentIndexList();
    std::vector<int> newRowOf(order.size());
    std::vector<Entry> sorted;
    sorted.reserve(m_entries.size());
    for (std::size_t newRow = 0; newRow < order.size(); ++newRow) {
        newRowOf[static_cast<std::size_t>(order[newRow])] = static_cast<int>(newRow);
        sorted.push_back(std::move(m_entries[static_cast<std::size_t>(order[newRow])]));
    }
    m_entries = std::move(sorted);
    m_rowByKey.clear();
    for (std::size_t i = 0; i < m_entries.size(); ++i)
        m_rowByKey[m_entries[i].last.key()] = static_cast<int>(i);
    QModelIndexList newPersistent;
    for (const QModelIndex &idx : oldPersistent)
        newPersistent << index(newRowOf[static_cast<std::size_t>(idx.row())], idx.column());
    changePersistentIndexList(oldPersistent, newPersistent);
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);
}

void MessageOverviewModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= ColumnCount)
        return;
    if (column == m_sortColumn && order == m_sortOrder)
        return;
    m_sortColumn = column;
    m_sortOrder = order;
    resort();
    emit sortChanged();
}

void MessageOverviewModel::sortBy(int column)
{
    if (column == m_sortColumn)
        sort(column, m_sortOrder == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
    else // counts/periods/times are most useful largest-first
        sort(column, column == IdColumn || column == ChannelColumn ? Qt::AscendingOrder
                                                                  : Qt::DescendingOrder);
}

void MessageOverviewModel::update(const CanFrame *frames, std::size_t n)
{
    if (n == 0)
        return;
    std::vector<bool> touched(m_entries.size(), false);
    std::vector<std::uint8_t> masks(m_entries.size(), 0);
    bool inserted = false;

    for (std::size_t i = 0; i < n; ++i) {
        const CanFrame &f = frames[i];
        auto it = m_rowByKey.find(f.key());
        if (it == m_rowByKey.end()) {
            const int row = count();
            beginInsertRows({}, row, row);
            Entry e;
            e.last = f;
            e.count = 1;
            e.period.add(f.timestampUs);
            m_entries.push_back(e);
            m_rowByKey.emplace(f.key(), row);
            endInsertRows();
            touched.push_back(false); // new rows need no dataChanged
            masks.push_back(0);
            inserted = true;
            continue;
        }
        const auto row = static_cast<std::size_t>(it->second);
        Entry &e = m_entries[row];
        for (int b = 0; b < CanFrame::kMaxDlc; ++b) {
            const auto bi = static_cast<std::size_t>(b);
            if (b < f.dlc && (b >= e.last.dlc || e.last.data[bi] != f.data[bi]))
                masks[row] |= static_cast<std::uint8_t>(1u << b);
        }
        e.last = f;
        ++e.count;
        e.period.add(f.timestampUs);
        touched[row] = true;
    }

    int firstTouched = -1;
    int lastTouched = -1;
    for (std::size_t row = 0; row < touched.size(); ++row) {
        if (!touched[row])
            continue;
        Entry &e = m_entries[row];
        e.changedMask = masks[row];
        if (masks[row])
            ++e.changeSerial;
        if (firstTouched < 0)
            firstTouched = static_cast<int>(row);
        lastTouched = static_cast<int>(row);
    }
    if (firstTouched >= 0)
        emit dataChanged(index(firstTouched, 0), index(lastTouched, ColumnCount - 1));
    if (inserted)
        emit countChanged();
    resort();
}

void MessageOverviewModel::clear()
{
    beginResetModel();
    m_entries.clear();
    m_rowByKey.clear();
    endResetModel();
    emit countChanged();
}

} // namespace can
