#pragma once

#include "dbc.h"

#include <QAbstractListModel>
#include <QVariantList>

#include <memory>
#include <vector>

namespace can {

// The plot layout: an ordered list of lanes, each holding several signals.
// Signals are referenced by their global signal id (same numbering as
// SignalListModel: messages in database order, signals in message order).
class PlotModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int seriesCount READ seriesCount NOTIFY countChanged)
    Q_PROPERTY(double totalWeight READ totalWeight NOTIFY weightsChanged)

public:
    static constexpr int kPaletteSize = 8;

    enum Role {
        LaneIdRole = Qt::UserRole + 1,
        SeriesRole,      // QVariantList of maps (see seriesToVariant)
        WeightRole,      // relative height
        YLockedRole,
        YMinRole,
        YMaxRole,
        ThresholdsRole,  // QVariantList of doubles
    };

    struct Series
    {
        int signalId = -1;
        int color = 0;          // palette index
        bool visible = true;
        bool dashed = false;    // second visual cue besides colour
    };
    struct Lane
    {
        int id = 0;
        std::vector<Series> series;
        double weight = 1.0;
        bool yLocked = false;
        double yMin = 0;
        double yMax = 1;
        std::vector<double> thresholds;
    };

    explicit PlotModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDatabase(std::shared_ptr<const Database> db);
    int count() const { return static_cast<int>(m_lanes.size()); }
    int seriesCount() const;
    double totalWeight() const;
    const Lane &lane(int i) const { return m_lanes[static_cast<std::size_t>(i)]; }

    // Adds a signal to lane `laneIndex` (-1 = a new lane at the end). Returns
    // the lane index, or -1 if the signal id is invalid.
    Q_INVOKABLE int addSignal(int signalId, int laneIndex = -1);
    Q_INVOKABLE void removeSignal(int laneIndex, int signalId);
    Q_INVOKABLE void toggleVisible(int laneIndex, int signalId);
    Q_INVOKABLE void setDashed(int laneIndex, int signalId, bool dashed);
    Q_INVOKABLE int addLane();
    Q_INVOKABLE void removeLane(int laneIndex);
    Q_INVOKABLE void moveLane(int from, int to);
    Q_INVOKABLE void setWeight(int laneIndex, double weight);
    Q_INVOKABLE void setYLock(int laneIndex, bool locked, double yMin, double yMax);
    Q_INVOKABLE int addThreshold(int laneIndex, double value);
    Q_INVOKABLE void setThreshold(int laneIndex, int thresholdIndex, double value);
    Q_INVOKABLE void removeThreshold(int laneIndex, int thresholdIndex);
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool containsSignal(int signalId) const;
    // Unique ids of all visible signals, in lane order (for export).
    Q_INVOKABLE QVariantList visibleSignals() const;
    // Display data for a signal id: name, unit, message, isEnum, valueLabels.
    Q_INVOKABLE QVariantMap signalInfo(int signalId) const;

signals:
    void countChanged();
    void weightsChanged();

private:
    const SignalDef *def(int signalId) const;
    QVariantList seriesToVariant(const Lane &lane) const;
    int nextColor() const;
    void laneChanged(int laneIndex, const QList<int> &roles = {});

    std::shared_ptr<const Database> m_db;
    std::vector<std::pair<int, int>> m_signalIndex; // global id -> (message, signal)
    std::vector<Lane> m_lanes;
    int m_nextLaneId = 1;
};

} // namespace can
