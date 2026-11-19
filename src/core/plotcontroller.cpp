#include "plotcontroller.h"

#include "timeseriesstore.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>

namespace can {

PlotController::PlotController(QObject *parent)
    : QObject(parent)
{}

void PlotController::setStore(TimeSeriesStore *store)
{
    m_store = store;
    m_displayNowUs = 0;
    applyFollow();
}

std::int64_t PlotController::newestUs() const
{
    return m_store ? m_store->nowUs() : 0;
}

void PlotController::applyFollow()
{
    if (!m_follow)
        return;
    const auto end = static_cast<std::int64_t>(std::llround(m_displayNowUs));
    const plotmath::TimeRange v{end - m_windowSeconds * 1'000'000LL, end};
    if (v.t0 != m_view.t0 || v.t1 != m_view.t1) {
        m_view = v;
        emit viewChanged();
    }
}

void PlotController::advance(double dtSeconds)
{
    const double newest = static_cast<double>(newestUs());
    // Advance with the display, never beyond the newest sample; if we fell far
    // behind (start, resume, clear), jump close to it.
    m_displayNowUs = std::min(m_displayNowUs + std::max(0.0, dtSeconds) * 1e6, newest);
    if (m_displayNowUs < newest - 80'000 || m_displayNowUs > newest)
        m_displayNowUs = std::max(0.0, newest - 30'000);
    applyFollow();
}

void PlotController::setFollowLive(bool follow)
{
    if (follow == m_follow)
        return;
    m_follow = follow;
    if (m_follow)
        m_displayNowUs = static_cast<double>(newestUs());
    emit viewChanged();
    applyFollow();
}

void PlotController::follow()
{
    setFollowLive(true);
}

void PlotController::setWindowSeconds(int seconds)
{
    seconds = std::clamp(seconds, 1, 600);
    if (seconds == m_windowSeconds && m_follow)
        return;
    m_windowSeconds = seconds;
    if (!m_follow) { // keep the right edge, change the span
        m_view.t0 = m_view.t1 - seconds * 1'000'000LL;
    }
    emit viewChanged();
    applyFollow();
}

void PlotController::zoomAt(double anchorFraction, double factor)
{
    const std::int64_t maxSpan = m_store ? m_store->historyUs() : 600'000'000LL;
    m_view = plotmath::zoomAt(m_view, anchorFraction, factor, kMinSpanUs, maxSpan);
    m_follow = false;
    emit viewChanged();
}

void PlotController::panByFraction(double fraction)
{
    m_view = plotmath::panBy(m_view, fraction);
    if (m_store) // stay within the recorded data (a little air after "now")
        m_view = plotmath::clampTo(m_view, m_store->firstTimeUs(), newestUs() + m_view.span() / 10);
    m_follow = false;
    emit viewChanged();
}

void PlotController::resetView()
{
    m_windowSeconds = kDefaultWindowSeconds;
    m_follow = true;
    m_displayNowUs = static_cast<double>(newestUs());
    emit viewChanged();
    applyFollow();
}

void PlotController::setView(double t0Us, double t1Us)
{
    if (t1Us - t0Us < kMinSpanUs)
        return;
    m_view = {std::llround(t0Us), std::llround(t1Us)};
    m_follow = false;
    emit viewChanged();
}

void PlotController::setCursor(int which, double timeUs)
{
    (which == 0 ? m_cursorA : m_cursorB) = timeUs;
    emit cursorsChanged();
}

void PlotController::placeCursors()
{
    m_cursorA = static_cast<double>(m_view.t0) + m_view.span() / 3.0;
    m_cursorB = static_cast<double>(m_view.t0) + m_view.span() * 2.0 / 3.0;
    emit cursorsChanged();
}

void PlotController::clearCursors()
{
    m_cursorA = m_cursorB = -1;
    emit cursorsChanged();
}

QVariantMap PlotController::cursorDelta() const
{
    QVariantMap m;
    if (!hasCursors())
        return m;
    const auto d = plotmath::cursorDelta(std::llround(m_cursorA), std::llround(m_cursorB));
    m[QStringLiteral("dtSeconds")] = d.dtSeconds;
    const double absDt = std::abs(d.dtSeconds);
    m[QStringLiteral("dtText")] = absDt < 1.0 ? QStringLiteral("%1 ms").arg(d.dtSeconds * 1000.0, 0, 'f', 1)
                                              : QStringLiteral("%1 s").arg(d.dtSeconds, 0, 'f', 3);
    m[QStringLiteral("freqText")] = d.frequencyHz <= 0 ? QStringLiteral("—")
                                    : d.frequencyHz >= 1.0 ? QStringLiteral("%1 Hz").arg(d.frequencyHz, 0, 'f', 2)
                                                           : QStringLiteral("%1 mHz").arg(d.frequencyHz * 1000.0, 0, 'f', 1);
    return m;
}

QVariantList PlotController::timeTicks(double widthPx) const
{
    QVariantList out;
    if (widthPx <= 0 || m_view.span() <= 0)
        return out;
    const double t0 = static_cast<double>(m_view.t0) / 1e6;
    const double t1 = static_cast<double>(m_view.t1) / 1e6;
    double step = 1;
    const auto ticks = plotmath::niceTicks(t0, t1, std::max(2, static_cast<int>(widthPx / 110)), &step);
    for (double t : ticks) {
        if (t < 0)
            continue;
        out << QVariantMap{{QStringLiteral("x"), plotmath::toPixel(std::llround(t * 1e6), m_view, widthPx)},
                           {QStringLiteral("label"), plotmath::formatSeconds(t, step)}};
    }
    return out;
}

QVariantList PlotController::valueTicks(double lo, double hi, double heightPx) const
{
    QVariantList out;
    if (heightPx <= 0 || !(hi > lo))
        return out;
    double step = 1;
    const auto ticks = plotmath::niceTicks(lo, hi, std::max(2, static_cast<int>(heightPx / 34)), &step);
    for (double v : ticks)
        out << QVariantMap{{QStringLiteral("y"), (hi - v) / (hi - lo) * heightPx},
                           {QStringLiteral("value"), v},
                           {QStringLiteral("label"), plotmath::formatValue(v, step)}};
    return out;
}

QVariantList PlotController::paddedRange(double lo, double hi) const
{
    const auto [a, b] = plotmath::paddedRange(lo, hi);
    return {a, b};
}

double PlotController::timeAtX(double x, double widthPx) const
{
    return static_cast<double>(plotmath::toTime(x, m_view, widthPx));
}

double PlotController::xOfTime(double timeUs, double widthPx) const
{
    return plotmath::toPixel(std::llround(timeUs), m_view, widthPx);
}

QString PlotController::formatTime(double timeUs) const
{
    return QString::number(timeUs / 1e6, 'f', 3) + QStringLiteral(" s");
}

QString PlotController::formatValue(double value, int decimals) const
{
    return QString::number(value, 'f', std::clamp(decimals, 0, 6));
}

bool PlotController::exportCsv(const QUrl &fileUrl, const QVariantList &signalIds, const QStringList &names)
{
    QList<int> ids;
    for (const QVariant &v : signalIds)
        ids << v.toInt();
    return writeCsv(fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString(), ids, names);
}

bool PlotController::writeCsv(const QString &path, const QList<int> &signalIds, const QStringList &names)
{
    auto fail = [this](const QString &msg) {
        m_lastError = msg;
        emit lastErrorChanged();
        return false;
    };
    if (!m_store)
        return fail(QStringLiteral("Nothing to export yet."));
    if (signalIds.isEmpty())
        return fail(QStringLiteral("Add a signal to a plot before exporting."));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return fail(QStringLiteral("Could not write the file: %1").arg(f.errorString()));

    // Gather the samples of each signal inside the view.
    std::vector<std::vector<TimeSample>> cols;
    for (int id : signalIds) {
        std::vector<TimeSample> s;
        if (const TimeSeries *ts = m_store->series(id)) {
            ts->samples(m_view.t0, m_view.t1, s);
            s.erase(std::remove_if(s.begin(), s.end(),
                                   [&](const TimeSample &x) { return x.t < m_view.t0 || x.t > m_view.t1; }),
                    s.end());
        }
        cols.push_back(std::move(s));
    }
    QTextStream out(&f);
    out << "time_s";
    for (int i = 0; i < signalIds.size(); ++i)
        out << ',' << (i < names.size() ? names[i] : QStringLiteral("signal_%1").arg(signalIds[i]));
    out << '\n';
    // k-way merge on time: one row per distinct sample time.
    std::vector<std::size_t> pos(cols.size(), 0);
    for (;;) {
        std::int64_t t = std::numeric_limits<std::int64_t>::max();
        for (std::size_t c = 0; c < cols.size(); ++c)
            if (pos[c] < cols[c].size())
                t = std::min(t, cols[c][pos[c]].t);
        if (t == std::numeric_limits<std::int64_t>::max())
            break;
        out << QString::number(static_cast<double>(t) / 1e6, 'f', 6);
        for (std::size_t c = 0; c < cols.size(); ++c) {
            out << ',';
            if (pos[c] < cols[c].size() && cols[c][pos[c]].t == t) {
                out << QString::number(cols[c][pos[c]].v, 'g', 12);
                ++pos[c];
            }
        }
        out << '\n';
    }
    out.flush();
    if (f.error() != QFile::NoError)
        return fail(QStringLiteral("Could not write the file: %1").arg(f.errorString()));
    m_lastError.clear();
    emit lastErrorChanged();
    return true;
}

} // namespace can
