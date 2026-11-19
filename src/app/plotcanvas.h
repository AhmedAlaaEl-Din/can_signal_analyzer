#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickItem>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <vector>

namespace can {
class TimeSeriesStore;
}

// Scene-graph plot of several signals in one lane: thick anti-aliased lines
// (triangle strips, MSAA), a soft gradient under the first visible series, a
// min/max band where the view is zoomed out (from the store's decimation),
// step drawing for enumerations and an optional dash pattern per series.
//
// Data is prepared on the GUI thread in updatePolish() (store access,
// decimation, autoscale range); updatePaintNode() only turns the prepared
// points into geometry.
class PlotCanvas : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *store READ store WRITE setStore NOTIFY inputsChanged)
    Q_PROPERTY(QVariantList series READ series WRITE setSeries NOTIFY inputsChanged)
    Q_PROPERTY(QVariantList palette READ palette WRITE setPalette NOTIFY inputsChanged)
    Q_PROPERTY(double viewStartUs READ viewStartUs WRITE setViewStartUs NOTIFY inputsChanged)
    Q_PROPERTY(double viewEndUs READ viewEndUs WRITE setViewEndUs NOTIFY inputsChanged)
    Q_PROPERTY(double yMin READ yMin WRITE setYMin NOTIFY inputsChanged)
    Q_PROPERTY(double yMax READ yMax WRITE setYMax NOTIFY inputsChanged)
    Q_PROPERTY(qulonglong revision READ revision WRITE setRevision NOTIFY inputsChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY inputsChanged)
    Q_PROPERTY(qreal fillOpacity READ fillOpacity WRITE setFillOpacity NOTIFY inputsChanged)
    Q_PROPERTY(qreal bandOpacity READ bandOpacity WRITE setBandOpacity NOTIFY inputsChanged)
    Q_PROPERTY(bool decimate READ decimate WRITE setDecimate NOTIFY inputsChanged)
    Q_PROPERTY(bool hasData READ hasData NOTIFY dataRangeChanged)
    Q_PROPERTY(double dataMin READ dataMin NOTIFY dataRangeChanged)
    Q_PROPERTY(double dataMax READ dataMax NOTIFY dataRangeChanged)
    Q_PROPERTY(bool zoomedOut READ zoomedOut NOTIFY dataRangeChanged)

public:
    explicit PlotCanvas(QQuickItem *parent = nullptr);

    QObject *store() const;
    void setStore(QObject *s);
    QVariantList series() const { return m_seriesVar; }
    void setSeries(const QVariantList &s);
    QVariantList palette() const { return m_paletteVar; }
    void setPalette(const QVariantList &p);
    double viewStartUs() const { return m_t0; }
    void setViewStartUs(double v);
    double viewEndUs() const { return m_t1; }
    void setViewEndUs(double v);
    double yMin() const { return m_yMin; }
    void setYMin(double v);
    double yMax() const { return m_yMax; }
    void setYMax(double v);
    qulonglong revision() const { return m_revision; }
    void setRevision(qulonglong r);
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w);
    qreal fillOpacity() const { return m_fillOpacity; }
    void setFillOpacity(qreal o);
    qreal bandOpacity() const { return m_bandOpacity; }
    void setBandOpacity(qreal o);
    bool decimate() const { return m_decimate; }
    void setDecimate(bool on);
    bool hasData() const { return m_hasData; }
    double dataMin() const { return m_dataMin; }
    double dataMax() const { return m_dataMax; }
    bool zoomedOut() const { return m_zoomedOut; }

    // Accumulated preparation time (benchmark instrumentation).
    static std::atomic<qint64> &prepareNanos();
    static std::atomic<qint64> &geometryNanos();
    static std::atomic<qint64> &pointsDrawn();

signals:
    void inputsChanged();
    void dataRangeChanged();

protected:
    void updatePolish() override;
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Prepared
    {
        QColor color;
        bool dashed = false;
        bool step = false;
        bool fill = false;
        bool banded = false;
        std::vector<double> xs;      // device-independent x (item coordinates)
        std::vector<double> values;  // line values (data units)
        std::vector<double> bandMin; // per point when banded
        std::vector<double> bandMax;
    };
    void changed();

    QPointer<can::TimeSeriesStore> m_store;
    QVariantList m_seriesVar;
    QVariantList m_paletteVar;
    std::vector<QColor> m_palette;
    double m_t0 = 0;
    double m_t1 = 1;
    double m_yMin = 0;
    double m_yMax = 1;
    qulonglong m_revision = 0;
    qreal m_lineWidth = 1.75;
    qreal m_fillOpacity = 0.18;
    qreal m_bandOpacity = 0.22;
    bool m_decimate = true;
    bool m_hasData = false;
    double m_dataMin = 0;
    double m_dataMax = 1;
    bool m_zoomedOut = false;
    std::vector<Prepared> m_prepared;
    bool m_geometryDirty = true;
};
