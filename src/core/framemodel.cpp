#include "framemodel.h"

#include <algorithm>

namespace can {

FrameModel::FrameModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_buffer(kDefaultCapacity)
{}

int FrameModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

int FrameModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

const CanFrame &FrameModel::frameAt(int row) const
{
    return m_buffer.bySeq(m_rows[static_cast<std::size_t>(row)]);
}

int FrameModel::rowForSeq(qulonglong seq) const
{
    // m_rows is sorted ascending: binary search.
    auto it = std::lower_bound(m_rows.begin(), m_rows.end(), static_cast<std::uint64_t>(seq));
    if (it == m_rows.end() || *it != seq)
        return -1;
    return static_cast<int>(it - m_rows.begin());
}

QVariant FrameModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const std::uint64_t seq = m_rows[static_cast<std::size_t>(index.row())];
    const CanFrame &f = m_buffer.bySeq(seq);

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case TimeColumn: role = TimeRole; break;
        case ChannelColumn: role = ChannelRole; break;
        case IdColumn: role = IdRole; break;
        case DlcColumn: role = DlcRole; break;
        case DataColumn: role = DataRole; break;
        default: return {};
        }
    }
    switch (role) {
    case TimeRole: return formatTimestamp(f.timestampUs);
    case ChannelRole: return int(f.channel);
    case IdRole: return formatId(f.id, f.extended);
    case ExtendedRole: return f.extended;
    case DlcRole: return int(f.dlc);
    case DataRole: return formatData(f);
    case SeqRole: return QVariant::fromValue<qulonglong>(seq);
    case OrdinalRole: return QVariant::fromValue<qulonglong>(m_removedFromFront + static_cast<std::uint64_t>(index.row()));
    case FreshRole: return seq >= m_freshFromSeq;
    case Qt::TextAlignmentRole:
        return index.column() == DataColumn || index.column() == IdColumn
                   ? int(Qt::AlignLeft | Qt::AlignVCenter)
                   : int(Qt::AlignRight | Qt::AlignVCenter);
    default: return {};
    }
}

QVariant FrameModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);
    switch (section) {
    case TimeColumn: return QStringLiteral("Time (s)");
    case ChannelColumn: return QStringLiteral("Ch");
    case IdColumn: return QStringLiteral("ID");
    case DlcColumn: return QStringLiteral("DLC");
    case DataColumn: return QStringLiteral("Data");
    default: return {};
    }
}

QHash<int, QByteArray> FrameModel::roleNames() const
{
    auto names = QAbstractTableModel::roleNames();
    names.insert({
        {TimeRole, "time"},
        {ChannelRole, "channel"},
        {IdRole, "frameId"},
        {ExtendedRole, "extended"},
        {DlcRole, "dlc"},
        {DataRole, "payload"},
        {SeqRole, "seq"},
        {OrdinalRole, "ordinal"},
        {FreshRole, "fresh"},
    });
    return names;
}

void FrameModel::setCapacity(int capacity)
{
    if (capacity < 1 || capacity == this->capacity())
        return;
    beginResetModel();
    m_buffer.setCapacity(static_cast<std::size_t>(capacity));
    m_rows.clear();
    m_removedFromFront = 0;
    endResetModel();
    emit capacityChanged();
    emit countChanged();
}

void FrameModel::append(const CanFrame *frames, std::size_t n)
{
    if (n == 0)
        return;
    const std::uint64_t firstNewSeq = m_buffer.nextSeq();
    for (std::size_t i = 0; i < n; ++i)
        m_buffer.push(frames[i]);
    m_freshFromSeq = firstNewSeq;

    // 1. Visible rows whose frame was evicted from the ring buffer.
    const std::uint64_t oldest = m_buffer.firstSeq();
    std::size_t evicted = 0;
    while (evicted < m_rows.size() && m_rows[evicted] < oldest)
        ++evicted;
    if (evicted > 0) {
        beginRemoveRows({}, 0, static_cast<int>(evicted) - 1);
        m_rows.erase(m_rows.begin(), m_rows.begin() + static_cast<std::ptrdiff_t>(evicted));
        m_removedFromFront += evicted;
        endRemoveRows();
    }

    // 2. New frames that pass the filter (and survived a batch larger than the buffer).
    std::vector<std::uint64_t> added;
    added.reserve(n);
    for (std::uint64_t seq = std::max(firstNewSeq, oldest); seq < m_buffer.nextSeq(); ++seq) {
        if (m_filter.matches(m_buffer.bySeq(seq)))
            added.push_back(seq);
    }
    if (!added.empty()) {
        const int first = count();
        beginInsertRows({}, first, first + static_cast<int>(added.size()) - 1);
        m_rows.insert(m_rows.end(), added.begin(), added.end());
        endInsertRows();
    }
    if (evicted > 0 || !added.empty())
        emit countChanged();
    if (evicted > 0)
        emit rowsEvicted(static_cast<int>(evicted));
}

void FrameModel::setFilter(const FrameFilter &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    rebuild();
}

void FrameModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    m_removedFromFront = 0;
    m_freshFromSeq = m_buffer.nextSeq(); // nothing is "fresh" after a re-filter
    for (std::uint64_t seq = m_buffer.firstSeq(); seq < m_buffer.nextSeq(); ++seq) {
        if (m_filter.matches(m_buffer.bySeq(seq)))
            m_rows.push_back(seq);
    }
    endResetModel();
    emit countChanged();
}

void FrameModel::clear()
{
    beginResetModel();
    m_buffer.clear();
    m_rows.clear();
    m_removedFromFront = 0;
    m_freshFromSeq = m_buffer.nextSeq();
    endResetModel();
    emit countChanged();
}

} // namespace can
