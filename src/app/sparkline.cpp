#include "sparkline.h"

#include "signallistmodel.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

Sparkline::Sparkline(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(false);
}

QObject *Sparkline::model() const
{
    return m_model.data();
}

void Sparkline::setModel(QObject *model)
{
    auto *m = qobject_cast<can::SignalListModel *>(model);
    if (m == m_model)
        return;
    m_model = m;
    emit modelChanged();
    update();
}

void Sparkline::setSignalId(int id)
{
    if (id == m_signalId)
        return;
    m_signalId = id;
    emit signalIdChanged();
    update();
}

void Sparkline::setRevision(qulonglong r)
{
    if (r == m_revision)
        return;
    m_revision = r;
    emit revisionChanged();
    update();
}

void Sparkline::setTimeUs(qint64 t)
{
    if (t == m_timeUs)
        return;
    m_timeUs = t;
    emit timeUsChanged();
    update();
}

void Sparkline::setWindowUs(qint64 w)
{
    if (w == m_windowUs || w <= 0)
        return;
    m_windowUs = w;
    emit windowUsChanged();
    update();
}

void Sparkline::setColor(const QColor &c)
{
    if (c == m_color)
        return;
    m_color = c;
    emit colorChanged();
    update();
}

void Sparkline::setAreaColor(const QColor &c)
{
    if (c == m_fill)
        return;
    m_fill = c;
    emit colorChanged();
    update();
}

void Sparkline::setGridColor(const QColor &c)
{
    if (c == m_grid)
        return;
    m_grid = c;
    emit colorChanged();
    update();
}

void Sparkline::setLineWidth(qreal w)
{
    if (qFuzzyCompare(w, m_lineWidth))
        return;
    m_lineWidth = w;
    emit colorChanged();
    update();
}

void Sparkline::setStepped(bool s)
{
    if (s == m_stepped)
        return;
    m_stepped = s;
    emit colorChanged();
    update();
}

void Sparkline::setShowDot(bool s)
{
    if (s == m_showDot)
        return;
    m_showDot = s;
    emit colorChanged();
    update();
}

void Sparkline::paint(QPainter *p)
{
    const qreal w = width();
    const qreal h = height();
    if (w < 4 || h < 4)
        return;
    p->setRenderHint(QPainter::Antialiasing, true);

    // Baseline (optional, very faint).
    if (m_grid.alpha() > 0) {
        p->setPen(QPen(m_grid, 1));
        p->drawLine(QPointF(0, h - 0.5), QPointF(w, h - 0.5));
    }
    if (!m_model || m_signalId < 0)
        return;

    static thread_local std::vector<can::SignalListModel::Sample> samples;
    m_model->history(m_signalId, samples);
    const qint64 end = m_timeUs;
    const qint64 start = end - m_windowUs;
    auto first = std::find_if(samples.begin(), samples.end(),
                              [start](const auto &s) { return s.timeUs >= start; });
    // Keep one sample before the window so the line starts at the left edge.
    if (first != samples.begin())
        --first;
    if (first == samples.end())
        return;

    // Scale to the samples inside the visible window only (the one sample kept
    // before the window just anchors the line at the left edge), so slow
    // signals still show their shape; add a small margin above and below.
    auto scaleFrom = std::find_if(first, samples.end(), [start](const auto &s) { return s.timeUs >= start; });
    if (scaleFrom == samples.end())
        scaleFrom = first;
    double lo = scaleFrom->min, hi = scaleFrom->max;
    for (auto it = scaleFrom; it != samples.end(); ++it) {
        lo = std::min(lo, it->min);
        hi = std::max(hi, it->max);
    }
    const qreal pad = m_lineWidth + 1.5;
    const qreal top = pad;
    const qreal bottom = h - pad;
    double span = hi - lo;
    if (span < 1e-12) { // flat signal: draw it in the middle
        lo -= 1;
        hi += 1;
        span = 2;
    } else {
        lo -= span * 0.08;
        hi += span * 0.08;
        span = hi - lo;
    }
    auto xOf = [&](qint64 t) {
        return std::clamp<qreal>((static_cast<qreal>(t - start) / static_cast<qreal>(m_windowUs)) * w, 0, w);
    };
    auto yOf = [&](double v) { return bottom - (v - lo) / span * (bottom - top); };
    // A bucket whose min..max covers a large part of the range changes faster
    // than one sample per bucket can show: draw it as a band, not as a line.
    auto wide = [&](const auto &s) { return (s.max - s.min) > 0.2 * span; };

    // 1) Envelope band (min..max per bucket) for fast-changing stretches.
    bool anyWide = false;
    for (auto it = first; it != samples.end(); ++it)
        anyWide |= wide(*it);
    if (anyWide) {
        QPainterPath band;
        std::vector<QPointF> lower;
        for (auto it = first; it != samples.end(); ++it) {
            const qreal x = xOf(it->timeUs);
            const QPointF up(x, yOf(it->max));
            if (band.elementCount() == 0)
                band.moveTo(up);
            else
                band.lineTo(up);
            lower.emplace_back(x, yOf(it->min));
        }
        band.lineTo(std::min(w, xOf(end)), band.currentPosition().y());
        band.lineTo(std::min(w, xOf(end)), lower.back().y());
        for (auto it = lower.rbegin(); it != lower.rend(); ++it)
            band.lineTo(*it);
        band.closeSubpath();
        QColor bandColor = m_color;
        bandColor.setAlphaF(0.28f);
        p->fillPath(band, bandColor);
    }

    // 2) Value line (only through narrow buckets) with a soft area below.
    QPainterPath line;
    QPointF prev;
    bool open = false;
    for (auto it = first; it != samples.end(); ++it) {
        const QPointF pt(xOf(it->timeUs), yOf(it->value));
        if (wide(*it)) {
            open = false;
            prev = pt;
            continue;
        }
        if (!open) {
            line.moveTo(pt);
            open = true;
        } else {
            if (m_stepped)
                line.lineTo(pt.x(), prev.y());
            line.lineTo(pt);
        }
        prev = pt;
    }
    const qreal xEnd = std::min(w, xOf(end));
    if (open && prev.x() < xEnd)
        line.lineTo(xEnd, prev.y());

    if (m_fill.alpha() > 0 && !anyWide && line.elementCount() > 0) {
        QPainterPath fillArea = line;
        fillArea.lineTo(line.currentPosition().x(), h);
        fillArea.lineTo(line.elementAt(0).x, h);
        fillArea.closeSubpath();
        QLinearGradient g(0, top, 0, h);
        QColor transparentFill = m_fill;
        transparentFill.setAlpha(0);
        g.setColorAt(0, m_fill);
        g.setColorAt(1, transparentFill);
        p->fillPath(fillArea, g);
    }
    QPen pen(m_color, m_lineWidth);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    p->drawPath(line);

    if (m_showDot && open) {
        p->setPen(Qt::NoPen);
        p->setBrush(m_color);
        p->drawEllipse(line.currentPosition(), m_lineWidth + 1.0, m_lineWidth + 1.0);
    }
}
