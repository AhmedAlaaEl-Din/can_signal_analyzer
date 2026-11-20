# Phase 1 explained (plain English)

Notes for explaining the design decisions in an interview.

## What the app does

It shows CAN bus traffic as it happens. A CAN bus carries short messages (frames). Each
frame has an identifier (what the message is), up to 8 data bytes, and a timestamp. The
app lists every frame in a scrolling "trace" and also keeps a per-identifier summary. In
phase 1 the frames come from a simulator instead of real hardware.

## Why a ring buffer?

A trace can run for hours. At 2000 frames per second that is 7 million frames an hour, so
we cannot keep everything in memory forever, and we must not slow down as data grows.

A ring buffer is a fixed-size array that wraps around: when it is full, each new frame
overwrites the oldest one. That gives us:

- **Fixed memory**: 100,000 frames is a few MB, no matter how long the app runs.
- **Constant-time work**: adding a frame is one array write; no reallocation, no shifting.
- **The most recent history**: which is what you look at in a live trace.

One extra trick: every frame gets a **sequence number** that only ever increases. The
table refers to frames by sequence number, not by array position. So when old frames are
overwritten, the table knows exactly which rows disappeared, the selected row stays
selected, and a user who scrolled up keeps looking at the same frames.

## Why batching?

The simulator produces up to 2000 frames per second, but a screen refreshes about 60 times
per second. Telling the UI about every single frame would mean 2000 model updates per
second, each one triggering layout work. That wastes CPU and makes scrolling stutter.

Instead:

1. The simulator thread collects frames and hands them over in small batches (every
   10 ms).
2. The GUI thread parks them, and every **30 ms** pushes one batch into the models with a
   single "rows inserted" notification.

So the UI does a bounded amount of work per screen refresh (about 33 updates per second)
whatever the bus rate is. 30 ms is below what people perceive as lag, and it lines up well
with a 60 Hz display.

Pause uses the same mechanism: while paused, batches keep arriving but are parked instead
of shown, and on resume they are all published at once, so nothing is lost.

## Why does the core have no QML dependency?

The core (`src/core/`) only uses Qt Core. It knows about frames, buffers, filters and
statistics, but nothing about windows, colors or QML. Reasons:

- **Testability**: the unit tests run the real core code with no GUI at all. They check
  formatting, determinism, ring-buffer behavior, filters and the period maths in under a
  second.
- **Separation of concerns**: the UI can be redesigned (or replaced by a command-line tool
  or a different front end) without touching the data logic.
- **Reuse for later phases**: logging and replay will reuse the same `FrameSource` /
  `FrameModel` pieces.

The models are `QAbstractTableModel` subclasses. That class lives in Qt Core, so the core
still provides proper table models that QML can display, without depending on QML. The QML
side gets access through one object (`Session`) that `main.cpp` registers.

## Why is the simulator deterministic?

The simulator uses a seed. Same seed means exactly the same frames, every time, on every
run. That makes bugs reproducible, lets tests compare whole frame sequences, and makes
demo recordings repeatable. It uses its own tiny random-number generator (SplitMix64)
because the random distributions in the C++ standard library are allowed to produce
different numbers on different compilers.

The engine is also independent of how time is sliced: advancing 3 seconds in one call or
in 2000 small calls gives the same frames. The thread and timer are a thin wrapper around
it.

## How the Theme works

`Theme.qml` is a QML singleton that holds every design decision as a named **token**:
colors, font families and sizes, spacing, corner radii, elevation and animation durations.
Components never hard-code a color or a size; they say `Theme.accent` or
`Theme.space.md`.

Both palettes (light and dark) are defined in full. Every public color is computed as a
blend between the light and the dark value, controlled by one number `t` (0 = light,
1 = dark). Switching themes animates `t` from 0 to 1 over 200 ms, so the whole UI
cross-fades smoothly with a single animation.

On top of the tokens sit two layers:

- **Components** (`CanUi`): Card, Chip, IconButton, EmptyState, and so on.
- **A custom Qt Quick Controls style** (`CanStyle`): our own Button, TextField, ComboBox,
  TabBar, ScrollBar, Switch and ToolTip. Because it is registered as a style, screens just
  use the normal control names and automatically get our look.

Text contrast was checked numerically against the WCAG formula for every text color on
every surface in both themes; all of them pass level AA.

## Other details worth mentioning

- **Bus load** = bits on the wire per second ÷ 500,000. Each frame's length is estimated
  from its format and data length (47 + 8×DLC bits for standard frames, 67 + 8×DLC for
  extended, including the gap between frames; bit stuffing is ignored).
- **Measured period** per ID uses an exponential moving average of the time between
  frames, so jitter is smoothed but rate changes show up within a few frames.
- **Stable sorting**: sorting by a constantly changing number (like the count) compares
  values rounded to two significant digits, so rows with practically equal values do not
  swap places on every update.
- **Fade-in for new rows** is only used when rows arrive slowly; at high rates it would
  make all rows semi-transparent all the time.
