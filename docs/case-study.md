# Case study: CAN Analyzer

*A desktop tool for watching, decoding, recording and replaying CAN bus traffic, built
from an empty folder to a packaged 1.0 release.*

## The problem

Modern vehicles and machines talk over CAN: hundreds of short messages per second, each an
ID and up to eight bytes. Raw bytes say little. An engineer needs to see what the bytes
*mean* (speed, torque, the selected gear), how those values move together over time, and
to keep a recording that can be studied again later, at a different speed, with notes on
the interesting moments.

The goal was a tool that does this smoothly at realistic bus rates, that a newcomer
understands in a minute, and that can be shown without any hardware or confidential data.

## What was built

- A **live trace** that keeps up with 2000 frames per second, with filters and a pause that
  never loses data.
- **DBC decoding**: an own parser for the common signal-database format turns bytes into
  named values with units and state names, and explains broken files line by line.
- A **signal list** with live values and sparklines, and a **plot view** of stacked lanes
  on one shared time axis, with measurement cursors, thresholds and export.
- **Logging and replay**: a compact log format, a recorder that runs on its own thread, and
  a replay that feeds the recording back through exactly the same views, with speed
  control, seeking, single steps, loop and named markers.
- **Search and statistics**: find frames by ID, by byte pattern with wildcards, or by a
  condition on a decoded signal (`VehicleSpeed > 100`); a per-ID summary of any log.
- A **portable Windows release**: one zip with the app, the Qt runtime, a sample recording,
  license notices and a version-stamped executable with its own icon.

A built-in vehicle simulator drives everything. It is deterministic: the same seed always
produces the same traffic, which made tests, screenshots and the demo video reproducible.

## Key decisions

**A custom plot engine, chosen by measurement.** The obvious route was the charting module
that ships with Qt. Both options were benchmarked with the same load: 8 signals, 10,000
visible points each, four lanes. The custom scene-graph component held the display's
60 fps (17.8 ms worst-case frame time for 95 % of frames). The Qt charting module reached
36 fps with hardware acceleration and 3 fps without. The custom engine also gave full
control over the look: thick anti-aliased lines, soft fills, dashed lines as a second cue
next to colour.

**Decimation that never hides a spike.** Ten minutes of a 100 Hz signal is 60,000 points
for a plot about 1,200 pixels wide. Drawing every 50th point would be fast but dishonest:
a single-sample glitch could vanish. Instead every pixel column keeps the *minimum and
maximum* of the samples it covers. That draws exactly what the eye could see from all
samples, at the cost of one entry per pixel, and an outlier always survives because it is
the minimum or maximum of its column. Pre-computed summaries per 64 samples make this take
about 3 ms per frame.

**One bit-mapping routine for everything.** CAN signals are packed at arbitrary bit
positions in two byte orders, and that is where tools usually go subtly wrong. A single
codec module does the packing and unpacking. The decoder uses it to read values, the
simulator uses it to write them, and the bit-layout view uses it to draw which bits a signal
occupies. Because the simulator encodes with the same code the decoder reads with, every
simulated frame is also a round-trip test of the decoder.

**A log format built for seeking and for crashes.** Frames are written in small,
self-contained blocks (one every 250 ms), each with its time range and a checksum. Those
block headers *are* the index: opening a 1,000,000-frame log reads only them and takes
13 ms, and a seek to any time costs about 30 µs. If the program stops abruptly, everything
up to the last complete block is still readable, and the reader says what was lost.

**One pipeline for live and recorded data.** Replay is just another frame source. The
trace, the decoder, the plots and the statistics do not know whether frames come from the
simulator or a file, so every feature works in replay without extra code, and every fix
applies to both.

## Challenges

- **Smooth at high rates.** Updating views for every frame at 2000 frames/s would freeze
  the interface. Frames are collected and handed over in batches every 30 ms, so the work
  per screen refresh stays bounded.
- **Honest numbers during replay.** At 8× speed the frame rate and bus load briefly read 8
  times too high. They now describe the recorded bus, not the playback speed.
- **A fuzzy line.** A gear signal drawn as a step line looked frayed. The cause was
  thousands of repeated, zero-length segments whose joins collapsed. The line is now drawn
  from its corners only.
- **A slow export.** The first CSV export of a million frames took 28 seconds. The cause was
  memory allocations per field. An allocation-free formatter brought it to 0.27 seconds.
- **Reproducible screenshots.** The demo is recorded frame by frame on a controlled
  clock: simulated time, the script and every animation advance exactly 1/12 s per image,
  and only the application window is captured.

## Results

- 17 automated test suites (log round trips, truncated and damaged files, seek accuracy on
  a million frames, replay timing at 0.25× to 8× with a fake clock, search rules, markers,
  decoding, plot maths and more), all passing, and a clean rebuild with zero compiler warnings
  at warning level 4 (`/W4`).
- 60 fps plots with 80,000 visible points. A million-frame log takes 12.6 MB on disk and
  11 MB of process memory while it is indexed, read and exported.
- A 35 MB portable release that runs from any folder without an installed Qt, checked by
  an automatic self-test in a clean copy.
- Three rounds of design review on screenshots of the running app, each fixing real
  issues (layout overflow at minimum window size, unclear error texts, misleading labels)
  in both the dark and the light theme.

## Technology

C++20, Qt 6 (Quick/QML for the interface, a custom scene-graph item for plots), CMake
presets, Qt Test with CTest, MSVC on Windows. The app has its own design system: colour
tokens for both themes, a colour-blind-safe plot palette, and its own control style and
icons.
