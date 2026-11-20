#include "markerstore.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace can {

MarkerStore::MarkerStore(QObject *parent)
    : QAbstractListModel(parent)
{}

int MarkerStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariant MarkerStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= count())
        return {};
    const Marker &m = m_markers[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TimeRole: return QVariant::fromValue(m.timeUs);
    case Qt::DisplayRole:
    case NameRole: return m.name;
    case NumberRole: return index.row() + 1;
    default: return {};
    }
}

QHash<int, QByteArray> MarkerStore::roleNames() const
{
    return {{TimeRole, "timeUs"}, {NameRole, "name"}, {NumberRole, "number"}};
}

QString MarkerStore::sidecarPathFor(const QString &logPath)
{
    return logPath + QStringLiteral(".markers.json");
}

bool MarkerStore::attach(const QString &logPath)
{
    beginResetModel();
    m_markers.clear();
    m_sidecar = sidecarPathFor(logPath);
    QString error;
    const bool ok = !QFile::exists(m_sidecar) || loadFrom(m_sidecar, m_markers, &error);
    m_nextNumber = count() + 1;
    endResetModel();
    setError(ok ? QString() : error);
    emit countChanged();
    return ok;
}

void MarkerStore::detach()
{
    beginResetModel();
    m_markers.clear();
    m_sidecar.clear();
    m_nextNumber = 1;
    endResetModel();
    emit countChanged();
}

int MarkerStore::add(qint64 timeUs, const QString &name)
{
    Marker m{timeUs, name.trimmed()};
    if (m.name.isEmpty())
        m.name = tr("Marker %1").arg(m_nextNumber);
    ++m_nextNumber;
    auto it = std::upper_bound(m_markers.begin(), m_markers.end(), timeUs,
                               [](qint64 t, const Marker &x) { return t < x.timeUs; });
    const int row = static_cast<int>(it - m_markers.begin());
    beginInsertRows({}, row, row);
    m_markers.insert(it, m);
    endInsertRows();
    changed();
    return row;
}

void MarkerStore::remove(int row)
{
    if (row < 0 || row >= count())
        return;
    beginRemoveRows({}, row, row);
    m_markers.erase(m_markers.begin() + row);
    endRemoveRows();
    changed();
}

void MarkerStore::rename(int row, const QString &name)
{
    if (row < 0 || row >= count() || name.trimmed().isEmpty())
        return;
    m_markers[static_cast<std::size_t>(row)].name = name.trimmed();
    emit dataChanged(index(row), index(row), {NameRole});
    changed();
}

void MarkerStore::clear()
{
    if (m_markers.empty())
        return;
    beginResetModel();
    m_markers.clear();
    endResetModel();
    changed();
}

qint64 MarkerStore::timeAt(int row) const
{
    return row >= 0 && row < count() ? m_markers[static_cast<std::size_t>(row)].timeUs : -1;
}

qint64 MarkerStore::nextAfter(qint64 timeUs) const
{
    auto it = std::upper_bound(m_markers.begin(), m_markers.end(), timeUs,
                               [](qint64 t, const Marker &x) { return t < x.timeUs; });
    return it == m_markers.end() ? -1 : it->timeUs;
}

qint64 MarkerStore::previousBefore(qint64 timeUs) const
{
    auto it = std::lower_bound(m_markers.begin(), m_markers.end(), timeUs,
                               [](const Marker &x, qint64 t) { return x.timeUs < t; });
    return it == m_markers.begin() ? -1 : std::prev(it)->timeUs;
}

QVariantList MarkerStore::all() const
{
    QVariantList out;
    for (const auto &m : m_markers)
        out.append(QVariantMap{{QStringLiteral("timeUs"), QVariant::fromValue(m.timeUs)},
                               {QStringLiteral("name"), m.name}});
    return out;
}

void MarkerStore::changed()
{
    emit countChanged();
    if (!m_sidecar.isEmpty() && !save())
        return;
    setError({});
}

bool MarkerStore::save() const
{
    QString error;
    if (!saveTo(m_sidecar, m_markers, &error)) {
        const_cast<MarkerStore *>(this)->setError(error);
        return false;
    }
    return true;
}

bool MarkerStore::saveTo(const QString &path, const std::vector<Marker> &markers, QString *error)
{
    QJsonArray list;
    for (const auto &m : markers)
        list.append(QJsonObject{{QStringLiteral("timeUs"), static_cast<double>(m.timeUs)},
                                {QStringLiteral("name"), m.name}});
    const QJsonObject root{{QStringLiteral("format"), QStringLiteral("can-analyzer-markers")},
                           {QStringLiteral("version"), 1},
                           {QStringLiteral("markers"), list}};
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error)
            *error = tr("Could not save markers: %1").arg(f.errorString());
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (error)
            *error = tr("Could not save markers: %1").arg(f.errorString());
        return false;
    }
    return true;
}

bool MarkerStore::loadFrom(const QString &path, std::vector<Marker> &markers, QString *error)
{
    markers.clear();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("Could not read %1").arg(QFileInfo(path).fileName());
        return false;
    }
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    const QJsonObject root = doc.object();
    if (pe.error != QJsonParseError::NoError
        || root.value(QStringLiteral("format")).toString() != QLatin1String("can-analyzer-markers")) {
        if (error)
            *error = tr("%1 is not a marker file; markers were not loaded.").arg(QFileInfo(path).fileName());
        return false;
    }
    for (const auto &v : root.value(QStringLiteral("markers")).toArray()) {
        const QJsonObject o = v.toObject();
        markers.push_back(Marker{static_cast<qint64>(o.value(QStringLiteral("timeUs")).toDouble()),
                                 o.value(QStringLiteral("name")).toString()});
    }
    std::stable_sort(markers.begin(), markers.end(),
                     [](const Marker &a, const Marker &b) { return a.timeUs < b.timeUs; });
    return true;
}

void MarkerStore::setError(const QString &e)
{
    if (e == m_error)
        return;
    m_error = e;
    emit errorChanged();
}

} // namespace can
