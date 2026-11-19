#include "plotmodel.h"

#include "signalcodec.h"

#include <algorithm>

namespace can {

PlotModel::PlotModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int PlotModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QHash<int, QByteArray> PlotModel::roleNames() const
{
    return {
        {LaneIdRole, "laneId"},
        {SeriesRole, "series"},
        {WeightRole, "weight"},
        {YLockedRole, "yLocked"},
        {YMinRole, "yMin"},
        {YMaxRole, "yMax"},
        {ThresholdsRole, "thresholds"},
    };
}

QVariant PlotModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= count())
        return {};
    const Lane &l = m_lanes[static_cast<std::size_t>(index.row())];
    switch (role) {
    case LaneIdRole: return l.id;
    case SeriesRole: return seriesToVariant(l);
    case WeightRole: return l.weight;
    case YLockedRole: return l.yLocked;
    case YMinRole: return l.yMin;
    case YMaxRole: return l.yMax;
    case ThresholdsRole: {
        QVariantList t;
        for (double v : l.thresholds)
            t << v;
        return t;
    }
    default: return {};
    }
}

void PlotModel::setDatabase(std::shared_ptr<const Database> db)
{
    beginResetModel();
    m_db = std::move(db);
    m_signalIndex.clear();
    m_lanes.clear();
    if (m_db) {
        for (std::size_t mi = 0; mi < m_db->messages.size(); ++mi)
            for (std::size_t si = 0; si < m_db->messages[mi].signalList.size(); ++si)
                m_signalIndex.emplace_back(static_cast<int>(mi), static_cast<int>(si));
    }
    endResetModel();
    emit countChanged();
}

const SignalDef *PlotModel::def(int signalId) const
{
    if (!m_db || signalId < 0 || signalId >= static_cast<int>(m_signalIndex.size()))
        return nullptr;
    const auto [mi, si] = m_signalIndex[static_cast<std::size_t>(signalId)];
    return &m_db->messages[static_cast<std::size_t>(mi)].signalList[static_cast<std::size_t>(si)];
}

QVariantMap PlotModel::signalInfo(int signalId) const
{
    QVariantMap m;
    const SignalDef *d = def(signalId);
    if (!d)
        return m;
    const auto [mi, si] = m_signalIndex[static_cast<std::size_t>(signalId)];
    Q_UNUSED(si);
    m[QStringLiteral("signalId")] = signalId;
    m[QStringLiteral("name")] = d->name;
    m[QStringLiteral("unit")] = d->unit;
    m[QStringLiteral("message")] = m_db->messages[static_cast<std::size_t>(mi)].name;
    m[QStringLiteral("isEnum")] = !d->valueTable.empty();
    m[QStringLiteral("decimals")] = codec::decimalsFor(*d);
    QVariantList labels;
    for (const auto &[raw, text] : d->valueTable)
        labels << QVariantMap{{QStringLiteral("value"), codec::toPhysical(*d, raw)}, {QStringLiteral("text"), text}};
    m[QStringLiteral("valueLabels")] = labels;
    return m;
}

QVariantList PlotModel::seriesToVariant(const Lane &lane) const
{
    QVariantList list;
    for (const Series &s : lane.series) {
        QVariantMap m = signalInfo(s.signalId);
        m[QStringLiteral("color")] = s.color;
        m[QStringLiteral("visible")] = s.visible;
        m[QStringLiteral("dashed")] = s.dashed;
        list << m;
    }
    return list;
}

int PlotModel::seriesCount() const
{
    int n = 0;
    for (const auto &l : m_lanes)
        n += static_cast<int>(l.series.size());
    return n;
}

int PlotModel::nextColor() const
{
    std::vector<int> used(kPaletteSize, 0);
    for (const auto &l : m_lanes)
        for (const auto &s : l.series)
            ++used[static_cast<std::size_t>(s.color % kPaletteSize)];
    // Least-used colour, lowest index first.
    return static_cast<int>(std::min_element(used.begin(), used.end()) - used.begin());
}

void PlotModel::laneChanged(int laneIndex, const QList<int> &roles)
{
    const QModelIndex i = index(laneIndex);
    emit dataChanged(i, i, roles);
}

int PlotModel::addSignal(int signalId, int laneIndex)
{
    if (!def(signalId))
        return -1;
    if (laneIndex < 0 || laneIndex >= count())
        laneIndex = addLane();
    Lane &l = m_lanes[static_cast<std::size_t>(laneIndex)];
    if (std::any_of(l.series.begin(), l.series.end(), [&](const Series &s) { return s.signalId == signalId; }))
        return laneIndex;
    Series s;
    s.signalId = signalId;
    s.color = nextColor();
    s.dashed = l.series.size() % 2 == 1; // second series in a lane: dashed by default
    l.series.push_back(s);
    laneChanged(laneIndex, {SeriesRole});
    emit countChanged();
    return laneIndex;
}

void PlotModel::removeSignal(int laneIndex, int signalId)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    auto &v = m_lanes[static_cast<std::size_t>(laneIndex)].series;
    const auto before = v.size();
    v.erase(std::remove_if(v.begin(), v.end(), [&](const Series &s) { return s.signalId == signalId; }), v.end());
    if (v.size() != before) {
        laneChanged(laneIndex, {SeriesRole});
        emit countChanged();
    }
}

void PlotModel::toggleVisible(int laneIndex, int signalId)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    for (auto &s : m_lanes[static_cast<std::size_t>(laneIndex)].series)
        if (s.signalId == signalId)
            s.visible = !s.visible;
    laneChanged(laneIndex, {SeriesRole});
}

void PlotModel::setDashed(int laneIndex, int signalId, bool dashed)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    for (auto &s : m_lanes[static_cast<std::size_t>(laneIndex)].series)
        if (s.signalId == signalId)
            s.dashed = dashed;
    laneChanged(laneIndex, {SeriesRole});
}

int PlotModel::addLane()
{
    const int row = count();
    beginInsertRows({}, row, row);
    Lane l;
    l.id = m_nextLaneId++;
    m_lanes.push_back(l);
    endInsertRows();
    emit countChanged();
    return row;
}

void PlotModel::removeLane(int laneIndex)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    beginRemoveRows({}, laneIndex, laneIndex);
    m_lanes.erase(m_lanes.begin() + laneIndex);
    endRemoveRows();
    emit countChanged();
}

void PlotModel::moveLane(int from, int to)
{
    if (from < 0 || from >= count() || to < 0 || to >= count() || from == to)
        return;
    // Qt's move semantics: destination is the row *before which* to insert.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    Lane l = m_lanes[static_cast<std::size_t>(from)];
    m_lanes.erase(m_lanes.begin() + from);
    m_lanes.insert(m_lanes.begin() + to, l);
    endMoveRows();
}

void PlotModel::setWeight(int laneIndex, double weight)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    m_lanes[static_cast<std::size_t>(laneIndex)].weight = std::clamp(weight, 0.25, 4.0);
    laneChanged(laneIndex, {WeightRole});
}

void PlotModel::setYLock(int laneIndex, bool locked, double yMin, double yMax)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    Lane &l = m_lanes[static_cast<std::size_t>(laneIndex)];
    l.yLocked = locked;
    if (yMax < yMin)
        std::swap(yMin, yMax);
    if (yMax - yMin < 1e-12)
        yMax = yMin + 1;
    l.yMin = yMin;
    l.yMax = yMax;
    laneChanged(laneIndex, {YLockedRole, YMinRole, YMaxRole});
}

int PlotModel::addThreshold(int laneIndex, double value)
{
    if (laneIndex < 0 || laneIndex >= count())
        return -1;
    auto &t = m_lanes[static_cast<std::size_t>(laneIndex)].thresholds;
    t.push_back(value);
    laneChanged(laneIndex, {ThresholdsRole});
    return static_cast<int>(t.size()) - 1;
}

void PlotModel::setThreshold(int laneIndex, int thresholdIndex, double value)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    auto &t = m_lanes[static_cast<std::size_t>(laneIndex)].thresholds;
    if (thresholdIndex < 0 || thresholdIndex >= static_cast<int>(t.size()))
        return;
    t[static_cast<std::size_t>(thresholdIndex)] = value;
    laneChanged(laneIndex, {ThresholdsRole});
}

void PlotModel::removeThreshold(int laneIndex, int thresholdIndex)
{
    if (laneIndex < 0 || laneIndex >= count())
        return;
    auto &t = m_lanes[static_cast<std::size_t>(laneIndex)].thresholds;
    if (thresholdIndex < 0 || thresholdIndex >= static_cast<int>(t.size()))
        return;
    t.erase(t.begin() + thresholdIndex);
    laneChanged(laneIndex, {ThresholdsRole});
}

void PlotModel::clear()
{
    beginResetModel();
    m_lanes.clear();
    endResetModel();
    emit countChanged();
}

bool PlotModel::containsSignal(int signalId) const
{
    for (const auto &l : m_lanes)
        for (const auto &s : l.series)
            if (s.signalId == signalId)
                return true;
    return false;
}

QVariantList PlotModel::visibleSignals() const
{
    QVariantList ids;
    for (const auto &l : m_lanes)
        for (const auto &s : l.series)
            if (s.visible && !ids.contains(s.signalId))
                ids << s.signalId;
    return ids;
}

} // namespace can
