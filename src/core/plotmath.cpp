#include "plotmath.h"

#include <algorithm>
#include <cmath>

namespace can::plotmath {

double niceStep(double range, int target)
{
    if (!(range > 0) || target < 1)
        return 1.0;
    const double raw = range / target;
    const double mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double norm = raw / mag;
    // Nearest of 1, 2, 5, 10 (so the tick count stays close to the target).
    double nice;
    if (norm < 1.5)
        nice = 1.0;
    else if (norm < 3.0)
        nice = 2.0;
    else if (norm < 7.0)
        nice = 5.0;
    else
        nice = 10.0;
    return nice * mag;
}

std::vector<double> niceTicks(double lo, double hi, int target, double *stepOut)
{
    std::vector<double> ticks;
    if (hi < lo)
        std::swap(lo, hi);
    const double step = niceStep(hi - lo, target);
    if (stepOut)
        *stepOut = step;
    if (!(hi > lo))
        return ticks;
    const double first = std::ceil(lo / step - 1e-9) * step;
    for (double v = first; v <= hi + step * 1e-9; v += step) {
        ticks.push_back(std::abs(v) < step * 1e-9 ? 0.0 : v);
        if (ticks.size() > 200)
            break;
    }
    return ticks;
}

std::pair<double, double> paddedRange(double lo, double hi, double marginFraction)
{
    if (hi < lo)
        std::swap(lo, hi);
    double span = hi - lo;
    if (span < 1e-9 * std::max(1.0, std::abs(hi))) {
        const double pad = std::max(1.0, std::abs(hi) * 0.05);
        return {lo - pad, hi + pad};
    }
    return {lo - span * marginFraction, hi + span * marginFraction};
}

TimeRange zoomAt(TimeRange r, double anchorFraction, double factor, std::int64_t minSpan, std::int64_t maxSpan)
{
    anchorFraction = std::clamp(anchorFraction, 0.0, 1.0);
    const double span = static_cast<double>(r.span());
    const double anchor = static_cast<double>(r.t0) + span * anchorFraction;
    const double newSpan = std::clamp(span * factor, static_cast<double>(minSpan), static_cast<double>(maxSpan));
    const double t0 = anchor - newSpan * anchorFraction;
    return {std::llround(t0), std::llround(t0 + newSpan)};
}

TimeRange panBy(TimeRange r, double fraction)
{
    const auto d = static_cast<std::int64_t>(std::llround(static_cast<double>(r.span()) * fraction));
    return {r.t0 + d, r.t1 + d};
}

TimeRange clampTo(TimeRange r, std::int64_t lo, std::int64_t hi)
{
    const std::int64_t span = r.span();
    if (hi - lo <= span)
        return {hi - span, hi};
    if (r.t0 < lo)
        return {lo, lo + span};
    if (r.t1 > hi)
        return {hi - span, hi};
    return r;
}

double toPixel(std::int64_t t, TimeRange r, double width)
{
    if (r.span() <= 0)
        return 0;
    return static_cast<double>(t - r.t0) / static_cast<double>(r.span()) * width;
}

std::int64_t toTime(double x, TimeRange r, double width)
{
    if (width <= 0)
        return r.t0;
    return r.t0 + std::llround(x / width * static_cast<double>(r.span()));
}

CursorDelta cursorDelta(std::int64_t aUs, std::int64_t bUs)
{
    CursorDelta d;
    d.dtSeconds = static_cast<double>(bUs - aUs) / 1e6;
    d.frequencyHz = d.dtSeconds != 0 ? 1.0 / std::abs(d.dtSeconds) : 0.0;
    return d;
}

static int decimalsForStep(double step)
{
    if (!(step > 0))
        return 0;
    const int d = static_cast<int>(std::ceil(-std::log10(step) - 1e-9));
    return std::clamp(d, 0, 6);
}

QString formatSeconds(double seconds, double stepSeconds)
{
    return QString::number(seconds, 'f', decimalsForStep(stepSeconds)) + QStringLiteral(" s");
}

QString formatValue(double value, double step)
{
    if (std::abs(value) < step * 1e-6)
        value = 0;
    return QString::number(value, 'f', decimalsForStep(step));
}

} // namespace can::plotmath
