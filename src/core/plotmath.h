#pragma once

#include <QString>

#include <cstdint>
#include <utility>
#include <vector>

namespace can::plotmath {

// "Nice" step (1, 2 or 5 x 10^n) so that roughly `target` ticks cover `range`.
double niceStep(double range, int target);
// Tick values inside [lo, hi] at a nice step.
std::vector<double> niceTicks(double lo, double hi, int target, double *stepOut = nullptr);
// Autoscale: data range plus a margin, widened when (almost) flat.
std::pair<double, double> paddedRange(double lo, double hi, double marginFraction = 0.08);

struct TimeRange
{
    std::int64_t t0;
    std::int64_t t1;
    std::int64_t span() const { return t1 - t0; }
};

// Zoom around the time at `anchorFraction` (0 = left edge, 1 = right edge):
// factor < 1 zooms in, > 1 zooms out. The anchor time stays at the same
// screen position. The span is clamped to [minSpan, maxSpan].
TimeRange zoomAt(TimeRange r, double anchorFraction, double factor, std::int64_t minSpan, std::int64_t maxSpan);
// Shift by a fraction of the span (positive = later).
TimeRange panBy(TimeRange r, double fraction);
// Keep the range inside [lo, hi] when possible (same span).
TimeRange clampTo(TimeRange r, std::int64_t lo, std::int64_t hi);

// Time <-> pixel mapping helpers.
double toPixel(std::int64_t t, TimeRange r, double width);
std::int64_t toTime(double x, TimeRange r, double width);

struct CursorDelta
{
    double dtSeconds = 0;  // B - A
    double frequencyHz = 0; // 1 / |dt|, 0 when dt == 0
};
CursorDelta cursorDelta(std::int64_t aUs, std::int64_t bUs);

// Label for a time tick (seconds since measurement start) at the given step.
QString formatSeconds(double seconds, double stepSeconds);
// Label for a value tick with as many decimals as the step needs.
QString formatValue(double value, double step);

} // namespace can::plotmath
