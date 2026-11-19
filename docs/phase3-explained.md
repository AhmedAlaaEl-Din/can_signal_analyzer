# Phase 3 explained (plain English)

Notes for explaining the live plots in an interview.

## What changed

Phase 2 showed the *current* value of every signal. Phase 3 shows how signals *change over
time*: stacked plot lanes that share one time axis, so "the driver lifts the throttle, torque
drops, the gear changes" can be seen in one picture. You can follow the live end, zoom into a
few milliseconds, measure between two cursors, and export what you see.

## Why the store is separate from the UI

All plotted data lives in a `TimeSeriesStore`: one ring buffer per signal holding
(timestamp, physical value) for the last 10 minutes. It is plain C++ with no Qt Quick:

- **One source of truth.** Every lane, the crosshair tooltip, the cursors and the CSV export
  read the same store. A signal shown in two lanes is stored once.
- **Independent of what is on screen.** Data is recorded whether or not a plot is open, so
  adding a signal to a plot instantly shows its past 10 minutes.
- **Testable.** Decimation, trimming and ring wrap-around are unit-tested without a window,
  including a performance smoke test.
- **Pause and replay friendly.** The store is fed by the same 30 ms batches as the other
  views; pausing simply stops feeding it (frames are parked and delivered on resume, so
  nothing is lost), and a future replay source will feed it the same way.

## Why min/max decimation

A 1200-pixel-wide plot of 10 minutes at 100 Hz has 60,000 samples per signal, about 50 per
pixel column. Drawing all of them wastes time, and the obvious shortcut, "draw every 50th
sample", is dangerous: a one-sample spike (a glitch, an out-of-range value) disappears
completely and the plot lies.

Min/max decimation keeps, for every pixel column, the **smallest and largest** value of all
samples that fall into it (plus the first and last for continuity). Drawing a vertical band
from min to max per column looks exactly like drawing every sample, because a column cannot
show more detail than that, but costs one entry per pixel. A spike always survives because it
is the max (or min) of its column. The tests check this at many zoom levels and with the ring
buffer wrapping.

To make zoomed-out views cheap, the store also keeps a summary per 64 consecutive samples
(their min and max). When a pixel column covers hundreds of samples, whole blocks are taken
from the summaries instead of reading each sample. Result: 8 signals × 60,000 samples are
reduced to 1200 columns in about 3 ms.

When zoomed in far enough that there are fewer samples than pixels, the raw samples are drawn
directly (no band needed).

## How follow-live and zoom work

The time axis is owned by one `PlotController` shared by all lanes. It has two modes:

- **Follow live**: the view is `[now − window, now]`, with the window 5, 10, 30 or 60 s. Data
  arrives in batches every 30 ms, but the screen refreshes every 16.7 ms; moving the view only
  when data arrives would make it judder. So the controller keeps a "display now" that moves
  forward by the real frame time every frame and never passes the newest sample. If it falls
  more than 80 ms behind (start, resume after pause), it jumps close to the data.
- **Free view**: as soon as you zoom or pan, following stops and the view stays where you
  put it while new data keeps arriving off-screen. "Follow live" (button, **F**, or the pill
  at the bottom) returns.

**Zoom** keeps the time under the mouse fixed: if the mouse is at 70 % of the width, the new
view is computed so that the same time is still at 70 %. Zoom factors are clamped between
20 ms and the full history. **Pan** shifts the view by a fraction of its width and is
clamped to the recorded data. The maths (nice tick steps, padded autoscale ranges, zoom
around an anchor, pan, clamping, cursor deltas) is a small pure module with unit tests.

Each lane autoscales its Y axis to the visible data with a small margin. Signals with
different units get separate scales: the first unit uses the left axis, a second unit the
right axis. The displayed range eases towards the new range every frame instead of jumping.

## The plot engine choice and the benchmark

Two options were planned: Qt Graphs (Qt's newer charting module) or a custom scene-graph
item. Qt Graphs is not installed on the development machine and installing software was not
allowed, so the installed alternative, **Qt Charts**, was benchmarked instead, with the same
workload as the custom engine: 8 signals at 1000 Hz, a 10 s window (10,000 points per
signal), 4 lanes.

| Engine | Frames per second |
|---|---|
| Custom scene-graph item, with min/max decimation | **60** (the display's limit) |
| Custom scene-graph item, drawing all 80,000 points | 51 |
| Qt Charts with OpenGL | 36 |
| Qt Charts without OpenGL | 3 |

The custom item (`PlotCanvas`) wins clearly and also gives full control over the look. It
works in two steps:

1. **Prepare (GUI thread, `updatePolish`)**: for each visible signal, ask the store for the
   samples in view, raw or decimated, and compute the data range for autoscaling.
2. **Build geometry (`updatePaintNode`)**: turn the points into triangles. A line of width
   1.75 px is a strip of triangles along the path with mitered corners; dashed lines are
   short quads. The soft gradient under the main series is a strip whose colour fades to
   transparent at the bottom; the min/max band is a strip between the column minima and
   maxima. Anti-aliasing comes from 4× multisampling on the window.

Only about 12,000 points per frame reach the GPU with decimation, which is why it stays at the
display's 60 fps with headroom (about 3.5 ms CPU per frame).

## Measurement and readability details

- **Colours:** a colour-blind-safe palette (Okabe–Ito family) with separate light/dark tunings,
  every colour checked for at least 3:1 contrast against the plot background. The second
  series of a lane is dashed by default, so two signals can be told apart without colour.
- **Enumerations** (gear, brake pedal) are drawn as step plots with their state names on the
  axis.
- **Cursors A and B** show the time difference, the matching frequency (1 / Δt, useful for
  periods) and every signal's value difference.
- **Thresholds** are draggable horizontal lines with a value badge; if the range moves away,
  the badge stays at the edge with an arrow.
