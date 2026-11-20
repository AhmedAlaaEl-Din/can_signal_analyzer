#include "plotcanvas.h"

#include "timeseriesstore.h"

#include <QElapsedTimer>
#include <QQuickWindow>
#include <QSGFlatColorMaterial>
#include <QSGGeometryNode>
#include <QSGVertexColorMaterial>

#include <algorithm>
#include <cmath>

namespace {

QSGGeometryNode *makeNode(QSGGeometry *geometry, QSGMaterial *material)
{
    auto *n = new QSGGeometryNode;
    n->setGeometry(geometry);
    n->setFlag(QSGNode::OwnsGeometry);
    n->setMaterial(material);
    n->setFlag(QSGNode::OwnsMaterial);
    return n;
}

inline void setColored(QSGGeometry::ColoredPoint2D &v, float x, float y, const QColor &c, float alpha)
{
    const float a = static_cast<float>(c.alphaF()) * alpha;
    v.set(x, y, static_cast<uchar>(c.red() * a), static_cast<uchar>(c.green() * a),
          static_cast<uchar>(c.blue() * a), static_cast<uchar>(255 * a));
}

// Thick polyline as a triangle strip with miter joins (limited to 2x).
void thickStrip(const std::vector<QPointF> &pts, float halfWidth, std::vector<QSGGeometry::Point2D> &out)
{
    out.clear();
    const std::size_t n = pts.size();
    if (n < 2)
        return;
    out.reserve(n * 2);
    auto normal = [](QPointF a, QPointF b) {
        QPointF d = b - a;
        const double len = std::hypot(d.x(), d.y());
        if (len < 1e-9)
            return QPointF(0, 0);
        return QPointF(-d.y() / len, d.x() / len);
    };
    for (std::size_t i = 0; i < n; ++i) {
        const QPointF nPrev = i > 0 ? normal(pts[i - 1], pts[i]) : normal(pts[i], pts[i + 1]);
        const QPointF nNext = i + 1 < n ? normal(pts[i], pts[i + 1]) : nPrev;
        QPointF m = nPrev + nNext;
        double len = std::hypot(m.x(), m.y());
        if (len < 1e-6) { // 180 degree turn: use the previous normal
            m = nPrev;
            len = 1;
        }
        m /= len;
        const double cosHalf = std::max(0.5, std::abs(m.x() * nPrev.x() + m.y() * nPrev.y()));
        const double scale = halfWidth / (cosHalf > 1e-6 ? cosHalf : 1.0);
        QSGGeometry::Point2D a, b;
        a.set(static_cast<float>(pts[i].x() + m.x() * scale), static_cast<float>(pts[i].y() + m.y() * scale));
        b.set(static_cast<float>(pts[i].x() - m.x() * scale), static_cast<float>(pts[i].y() - m.y() * scale));
        out.push_back(a);
        out.push_back(b);
    }
}

// Dashed polyline as independent quads (two triangles per dash piece).
void dashedTriangles(const std::vector<QPointF> &pts, float halfWidth, float on, float off,
                     std::vector<QSGGeometry::Point2D> &out)
{
    out.clear();
    const float period = on + off;
    float phase = 0; // distance along the dash pattern
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        const QPointF a = pts[i], b = pts[i + 1];
        const QPointF d = b - a;
        const double len = std::hypot(d.x(), d.y());
        if (len < 1e-9)
            continue;
        const QPointF u = d / len;
        const QPointF nrm(-u.y() * halfWidth, u.x() * halfWidth);
        double pos = 0;
        while (pos < len) {
            const float inPeriod = std::fmod(phase, period);
            const bool drawing = inPeriod < on;
            const double remaining = drawing ? on - inPeriod : period - inPeriod;
            const double step = std::min(remaining, len - pos);
            if (drawing) {
                const QPointF s = a + u * pos, e = a + u * (pos + step);
                QSGGeometry::Point2D v[4];
                v[0].set(float(s.x() + nrm.x()), float(s.y() + nrm.y()));
                v[1].set(float(s.x() - nrm.x()), float(s.y() - nrm.y()));
                v[2].set(float(e.x() + nrm.x()), float(e.y() + nrm.y()));
                v[3].set(float(e.x() - nrm.x()), float(e.y() - nrm.y()));
                out.push_back(v[0]); out.push_back(v[1]); out.push_back(v[2]);
                out.push_back(v[2]); out.push_back(v[1]); out.push_back(v[3]);
            }
            pos += step;
            phase += static_cast<float>(step);
        }
    }
}

} // namespace

std::atomic<qint64> &PlotCanvas::prepareNanos()
{
    static std::atomic<qint64> v{0};
    return v;
}
std::atomic<qint64> &PlotCanvas::geometryNanos()
{
    static std::atomic<qint64> v{0};
    return v;
}
std::atomic<qint64> &PlotCanvas::pointsDrawn()
{
    static std::atomic<qint64> v{0};
    return v;
}

PlotCanvas::PlotCanvas(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setClip(true);
}

QObject *PlotCanvas::store() const
{
    return m_store.data();
}

void PlotCanvas::changed()
{
    emit inputsChanged();
    polish();
    update();
}

void PlotCanvas::setStore(QObject *s)
{
    auto *st = qobject_cast<can::TimeSeriesStore *>(s);
    if (st == m_store)
        return;
    m_store = st;
    changed();
}

void PlotCanvas::setSeries(const QVariantList &s)
{
    m_seriesVar = s;
    changed();
}

void PlotCanvas::setColors(const QVariantList &p)
{
    m_paletteVar = p;
    m_palette.clear();
    for (const QVariant &v : p)
        m_palette.push_back(v.value<QColor>());
    changed();
}

#define CAN_SETTER(name, member)                     \
    void PlotCanvas::name(double v)                  \
    {                                                \
        if (v == member)                             \
            return;                                  \
        member = v;                                  \
        changed();                                   \
    }
CAN_SETTER(setViewStartUs, m_t0)
CAN_SETTER(setViewEndUs, m_t1)
#undef CAN_SETTER

void PlotCanvas::setYMin(double v)
{
    if (v == m_yMin)
        return;
    m_yMin = v;
    m_geometryDirty = true;
    emit inputsChanged();
    update();
}

void PlotCanvas::setYMax(double v)
{
    if (v == m_yMax)
        return;
    m_yMax = v;
    m_geometryDirty = true;
    emit inputsChanged();
    update();
}

void PlotCanvas::setRangesById(const QVariantMap &r)
{
    if (r == m_rangesById)
        return;
    m_rangesById = r;
    m_geometryDirty = true;
    emit inputsChanged();
    update();
}

void PlotCanvas::setRevision(qulonglong r)
{
    if (r == m_revision)
        return;
    m_revision = r;
    changed();
}

void PlotCanvas::setLineWidth(qreal w)
{
    if (qFuzzyCompare(w, m_lineWidth))
        return;
    m_lineWidth = w;
    changed();
}

void PlotCanvas::setFillOpacity(qreal o)
{
    if (qFuzzyCompare(o, m_fillOpacity))
        return;
    m_fillOpacity = o;
    changed();
}

void PlotCanvas::setBandOpacity(qreal o)
{
    if (qFuzzyCompare(o, m_bandOpacity))
        return;
    m_bandOpacity = o;
    changed();
}

void PlotCanvas::setDecimate(bool on)
{
    if (on == m_decimate)
        return;
    m_decimate = on;
    changed();
}

void PlotCanvas::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        polish();
        update();
    }
}

void PlotCanvas::updatePolish()
{
    QElapsedTimer timer;
    timer.start();
    m_prepared.clear();
    const double w = width();
    const auto t0 = static_cast<std::int64_t>(std::llround(m_t0));
    const auto t1 = static_cast<std::int64_t>(std::llround(m_t1));
    double lo = 1e300, hi = -1e300;
    bool any = false;
    bool zoomedOut = false;
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int columns = std::max(1, static_cast<int>(w * dpr));
    bool fillAssigned = false;
    QVariantList ranges;

    if (m_store && w > 1 && t1 > t0) {
        std::vector<can::TimeSample> raw;
        for (const QVariant &v : m_seriesVar) {
            const QVariantMap m = v.toMap();
            if (!m.value(QStringLiteral("visible"), true).toBool())
                continue;
            const can::TimeSeries *ts = m_store->series(m.value(QStringLiteral("signalId")).toInt());
            if (!ts || ts->empty())
                continue;
            Prepared p;
            p.signalId = m.value(QStringLiteral("signalId")).toInt();
            double sLo = 1e300, sHi = -1e300;
            const int ci = m.value(QStringLiteral("color")).toInt();
            p.color = m_palette.empty() ? QColor(Qt::gray) : m_palette[static_cast<std::size_t>(ci) % m_palette.size()];
            p.dashed = m.value(QStringLiteral("dashed")).toBool();
            p.step = m.value(QStringLiteral("isEnum")).toBool();
            p.fill = !fillAssigned && !p.step && m_fillOpacity > 0;
            fillAssigned = fillAssigned || p.fill;

            const std::size_t inView = ts->lowerBound(t1 + 1) - ts->lowerBound(t0);
            const double scale = w / static_cast<double>(t1 - t0);
            if (!m_decimate || inView <= static_cast<std::size_t>(columns) * 2) {
                ts->samples(t0, t1, raw);
                p.xs.reserve(raw.size());
                p.values.reserve(raw.size());
                for (const auto &s : raw) {
                    p.xs.push_back(static_cast<double>(s.t - t0) * scale);
                    p.values.push_back(s.v);
                    if (s.t >= t0 && s.t <= t1) {
                        sLo = std::min(sLo, s.v);
                        sHi = std::max(sHi, s.v);
                    }
                }
            } else {
                // Zoomed out: one entry per pixel column, with the min/max band.
                p.banded = true;
                zoomedOut = true;
                const auto cols = ts->decimate(t0, t1, columns);
                const double colW = w / columns;
                for (int c = 0; c < columns; ++c) {
                    const can::ColumnStat &cs = cols[static_cast<std::size_t>(c)];
                    if (cs.count == 0)
                        continue;
                    p.xs.push_back((c + 0.5) * colW);
                    p.values.push_back(cs.last);
                    p.bandMin.push_back(cs.min);
                    p.bandMax.push_back(cs.max);
                    sLo = std::min(sLo, cs.min);
                    sHi = std::max(sHi, cs.max);
                }
            }
            if (sLo <= sHi) {
                lo = std::min(lo, sLo);
                hi = std::max(hi, sHi);
                any = true;
                ranges << QVariantMap{{QStringLiteral("signalId"), p.signalId},
                                      {QStringLiteral("min"), sLo},
                                      {QStringLiteral("max"), sHi}};
            }
            pointsDrawn() += static_cast<qint64>(p.xs.size());
            m_prepared.push_back(std::move(p));
        }
    }
    m_geometryDirty = true;
    const bool rangesChanged = ranges != m_seriesRanges;
    m_seriesRanges = ranges;
    if (rangesChanged || any != m_hasData || (any && (lo != m_dataMin || hi != m_dataMax)) || zoomedOut != m_zoomedOut) {
        m_hasData = any;
        if (any) {
            m_dataMin = lo;
            m_dataMax = hi;
        }
        m_zoomedOut = zoomedOut;
        emit dataRangeChanged();
    }
    prepareNanos() += timer.nsecsElapsed();
}

QSGNode *PlotCanvas::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    QElapsedTimer timer;
    timer.start();
    QSGNode *root = old ? old : new QSGNode;
    // Rebuild all children (cheap compared with the data preparation).
    while (QSGNode *child = root->firstChild()) {
        root->removeChildNode(child);
        delete child;
    }
    const double h = height();
    if (m_yMax - m_yMin <= 0 || h <= 1) {
        m_geometryDirty = false;
        return root;
    }
    double yLo = m_yMin, ySpan = m_yMax - m_yMin;
    auto yOf = [&](double v) { return static_cast<float>(h - (v - yLo) / ySpan * h); };
    const float halfWidth = static_cast<float>(m_lineWidth / 2.0);
    std::vector<QPointF> pts;
    std::vector<QSGGeometry::Point2D> verts;

    for (const Prepared &p : m_prepared) {
        if (p.xs.empty())
            continue;
        // Per-series range (secondary axis) or the lane's main range.
        yLo = m_yMin;
        ySpan = m_yMax - m_yMin;
        const QVariantList own = m_rangesById.value(QString::number(p.signalId)).toList();
        if (own.size() == 2 && own[1].toDouble() > own[0].toDouble()) {
            yLo = own[0].toDouble();
            ySpan = own[1].toDouble() - yLo;
        }
        // Line points (step series repeat the previous value up to the next x).
        pts.clear();
        pts.reserve(p.xs.size() * (p.step ? 2 : 1));
        for (std::size_t i = 0; i < p.xs.size(); ++i) {
            if (p.step && i > 0)
                pts.emplace_back(p.xs[i], yOf(p.values[i - 1]));
            pts.emplace_back(p.xs[i], yOf(p.values[i]));
        }
        // Extend a step series to the right edge.
        if (p.step && pts.back().x() < width())
            pts.emplace_back(std::min(width(), pts.back().x() + width()), pts.back().y());

        // Gradient fill under the main series.
        if (p.fill && pts.size() >= 2) {
            auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), int(pts.size() * 2));
            g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            auto *v = g->vertexDataAsColoredPoint2D();
            for (std::size_t i = 0; i < pts.size(); ++i) {
                const float x = static_cast<float>(pts[i].x());
                setColored(v[2 * i], x, static_cast<float>(pts[i].y()), p.color, static_cast<float>(m_fillOpacity));
                setColored(v[2 * i + 1], x, static_cast<float>(h), p.color, 0.0f);
            }
            root->appendChildNode(makeNode(g, new QSGVertexColorMaterial));
        }

        // Min/max band when zoomed out.
        if (p.banded && m_bandOpacity > 0) {
            const std::size_t n = p.xs.size();
            auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), int(n * 2));
            g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            auto *v = g->vertexDataAsPoint2D();
            for (std::size_t i = 0; i < n; ++i) {
                const float x = static_cast<float>(p.xs[i]);
                float top = yOf(p.bandMax[i]);
                float bottom = yOf(p.bandMin[i]);
                if (bottom - top < 1.0f) { // keep at least 1 px so the band never vanishes
                    top -= 0.5f;
                    bottom += 0.5f;
                }
                v[2 * i].set(x, top);
                v[2 * i + 1].set(x, bottom);
            }
            auto *mat = new QSGFlatColorMaterial;
            QColor c = p.color;
            c.setAlphaF(m_bandOpacity);
            mat->setColor(c);
            root->appendChildNode(makeNode(g, mat));
        }

        // The line itself.
        if (p.dashed)
            dashedTriangles(pts, halfWidth, static_cast<float>(m_lineWidth * 3.5), static_cast<float>(m_lineWidth * 2.5), verts);
        else
            thickStrip(pts, halfWidth, verts);
        if (verts.size() >= 3) {
            auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), int(verts.size()));
            g->setDrawingMode(p.dashed ? QSGGeometry::DrawTriangles : QSGGeometry::DrawTriangleStrip);
            std::copy(verts.begin(), verts.end(), g->vertexDataAsPoint2D());
            auto *mat = new QSGFlatColorMaterial;
            mat->setColor(p.color);
            root->appendChildNode(makeNode(g, mat));
        }
    }
    m_geometryDirty = false;
    geometryNanos() += timer.nsecsElapsed();
    return root;
}
