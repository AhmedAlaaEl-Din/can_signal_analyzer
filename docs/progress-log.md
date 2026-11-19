# Progress log

## Summary (Phase 4, unattended run, 2026-11-19 12:43 → 15:17)

**Done** (branch `phase4-release`, from `phase3-plots`; local tag `v1.0.0`; nothing pushed or merged)
- **Logging**: `.canlog` format (versioned header with start time and channel JSON, CRC-checked
  blocks of frames with relative timestamps, about 13 bytes per frame), documented in
  `docs/log-format.md`. Recorder on its own thread, a block every 250 ms (crash-safe: a
  truncated file reads up to the last complete block). Record/Stop in the toolbar, REC chip
  with duration and size in the toolbar and status bar, date-named file in a save dialog.
  CSV export and import of frames (documented columns, line-numbered errors).
- **Replay** through the same pipeline: play/pause, 0.25×–8× and Max, loop, seek bar with
  time preview, jump to start/end, single-frame step, 10 s pre-roll after seeks; rate and bus
  load scaled to the recorded bus. Index = block headers: 1,000,000 frames open in 13 ms,
  seek 27 µs average, 11 MiB peak memory.
- **Markers**: named markers at the replay time, on the seek bar and as lines under the plot
  lanes, jump with `[` `]`, stored in a JSON sidecar next to the log.
- **Trace search**: ID / message name, data pattern with `??` wildcards, signal conditions
  (`VehicleSpeed > 100`, value-table text); interpretation chip, highlighted matches,
  F3 / Shift+F3.
- **Log summary dialog**: duration, frames, first/last timestamp, average and peak bus load,
  per-ID count/share/rate/period (mean, min, max)/DLC/load, CSV export.
- **Polish**: follow-live chip docked under the value axis (never over a lane); step signals
  as clean step lines without band (fuzzy-line bug fixed); quantisation hint instead of
  smoothing; welcome screen with one-click demo (database + four lanes + simulator + Plots);
  settings persisted (theme, window, tab, database, plot layout, filters; missing database
  handled); About dialog (version, build, Qt version, LGPLv3 notice and links).
- **Release**: version in one place (CMake) → `version.h`, exe version resource, About, zip
  name; own SVG app icon rendered to `.ico` at build time; `package_zip` target →
  `dist/CANSignalAnalyzer-1.0.0-win64.zip` (35.4 MiB) with exe, Qt runtime, MSVC runtime,
  demo DBC, 60 s sample log, README, LICENSE (MIT), THIRD_PARTY (Qt LGPLv3/GPLv3 texts,
  source location, how to replace Qt). No debug files. Verified from a clean copy with no
  Qt on PATH (`--smoke-test`: exit 0, 23,248 frames replayed). `can_logtool` for sample
  generation, info, CSV and the benchmark.
- **Tests**: 17 suites via CTest, all passing, 242 checks (new: `tst_canlog` 16,
  `tst_replay` 19, `tst_framesearch` 9, `tst_plotmodel` +1). Clean rebuild: **0 warnings**.
- **Demo capture**: deterministic 60 s showcase, 721 JPEG frames at 12 fps (window only,
  1280 × 800, quality 80), 8 PNG screenshots, `frames.zip` 52.6 MiB.
- **Design review**: 3 rounds on the screenshots (decisions.md); final capture after round 3.
- **Docs**: README rewritten as the project page, `docs/case-study.md`,
  `docs/phase4-explained.md`, `docs/log-format.md`, `docs/building.md` (build details moved
  out of the README), decisions, needs-me.
- **Scan** (tracked files and every file in the release zip, including strings in the
  executables): no machine paths, names, e-mails, secrets or employer names. Remaining hits
  are Qt's own data (locale/timezone names, a PC vendor table) and not from this project.

**Blocked**: nothing.

**Needs me** (`docs/needs-me.md` 15–22): review the branch and tag; assemble the video;
confirm the MIT license / copyright holder; decide on a Qt source offer for public binary
releases; no installer (no installer tool available); try the native dialogs and
drag-and-drop by hand once; optional README screenshots; settings registry key.

**Where things are**
- Release zip: `dist/CANSignalAnalyzer-1.0.0-win64.zip` (35.4 MiB, git-ignored)
- Hand-off copies (folder outside the repo, path given in the chat): `frames.zip` (52.6 MiB),
  `screenshots/01-welcome-dark.png` … `08-about-light.png`
- In the repo (git-ignored): `evidence/frames/frame_0000.jpg` … `frame_0720.jpg`,
  `evidence/frames.zip`, `evidence/screenshots/`, `evidence/checks/` (minimum-size and edge
  cases), `evidence/log-bench.txt`, `evidence/demo_recording.canlog`

**How to build and run**
```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1                 # build, deploy, start
cmake --build --preset local-release --target package_zip               # release zip in dist\
build\local-release\can_analyzer.exe --demo --rate 1000                 # re-capture the showcase
powershell -ExecutionPolicy Bypass -File tools\zip-frames.ps1           # zip the frames
build\local-release\can_logtool.exe bench --report evidence\log-bench.txt
```
Release: unzip, start `can_analyzer.exe`, click **Start with the demo**; open
`samples\demo_drive_60s.canlog` for replay.

**Risks / known limitations**
- The native file dialogs and drag-and-drop were not operated by hand (unattended); the
  same code paths were exercised programmatically (record, open, export, summary CSV).
- License and LGPL source-offer wording need the owner's confirmation (needs-me 17–18).
- The MSVC runtime is copied from the developer environment's redist folder because CMake
  3.30 does not know the VS 18 toolset; building the package outside a VS developer
  environment would warn and omit it.
- Replay "Max" speed is a fixed 4000 frames per tick, not adaptive.
- CSV import requires rows in time order (reported with the line number).
- Plots in replay show an empty region before the start of the log when the window is
  longer than the elapsed log time (the view does not clamp at the log start).
- No installer; the app stores settings in the registry (`HKCU\Software\CanAnalyzer`).

---

## Summary (Phase 3, unattended run, 2026-11-19 10:50 → 11:45)

**Done** (branch `phase3-plots`, from `phase2-decoding`; not pushed, not merged)
- **Simulator fixes:** torque follows throttle and an rpm torque curve (engine braking when
  coasting), load follows torque, throttle varies while accelerating and cruising, nothing
  sits clamped at a range limit; new tests check that every analog signal varies over 60 s and
  never stays at a DBC limit for more than 6 s. Sparklines scale to their visible range.
- **Time-series store** (core, Qt Core only): ring buffer per signal, 10 min history, min/max
  decimation per pixel column (spikes never lost) with 64-sample block summaries.
- **Plot engine:** custom scene-graph item `PlotCanvas` (thick AA lines via triangle strips +
  4× MSAA, gradient fill, min/max band, step plots, dash cue), chosen by benchmark.
- **Plots tab:** stacked lanes on one time axis; add via +, drag and drop, or Plot buttons in
  the Signals tab/detail panel; remove/reorder/resize lanes; per-unit left/right axes;
  colour-blind-safe palette; legend with live values (toggle, dash cue, remove); enum step
  plots with state labels; follow live 5/10/30/60 s, wheel zoom, drag pan, double-click
  reset, Y lock; crosshair tooltip; cursors A/B with Δt, frequency and value deltas;
  draggable thresholds; pause without data loss; PNG and CSV export; empty states,
  friendly export errors, shortcuts (F, +, −, 0, C, Esc, Ctrl+4); short animations.
- **Tests:** 14 suites via CTest, **all passing, 197 checks** (new: `tst_timeseries` 15 incl.
  performance smoke test, `tst_plotmath` 13, `tst_plotmodel` 8, `tst_vehiclesim` +2).
  Clean rebuild with `/W4`: **0 warnings**.
- **Demo capture:** deterministic 12 fps Plots scene, 541 JPEG frames (application window only,
  1280 × 800, quality 80), 6 named PNG screenshots, `evidence/frames.zip` = 40.6 MB.
- **Design review:** 3 rounds (`docs/decisions.md`); found and fixed real bugs (real mouse
  leaking into the capture, cursors/tooltip not following scrolling, a Behavior that froze the
  axis while following live, mixed units on one axis). Final capture after round 3.
- **Docs:** README (plot features, architecture with store and engine, performance table,
  screenshot placeholders, demo, roadmap), `docs/phase3-explained.md`.
- **Scan** of tracked files: no machine paths, names, emails or secrets.

**Plot engine and benchmark** (8 signals × 10,000 visible points, 4 lanes, 1280 × 800 at
125 %, D3D11, 4× MSAA; `can_analyzer --plot-bench`, `can_charts_bench`):

| Engine | fps | p95 frame |
|---|---|---|
| **Custom scene graph + min/max decimation (chosen)** | **60.0** (vsync) | 17.8 ms |
| Custom scene graph, all 80k raw points | 51.5 | 32.0 ms |
| Qt Charts, useOpenGL | 36.2 | 36.2 ms |
| Qt Charts, raster | 2.7 | 439 ms |

Qt Graphs is **not installed** on this machine (installing was not allowed), so Qt Charts was
the installed comparison (see needs-me).

**Blocked**: nothing.

**Needs me**: see `docs/needs-me.md` (review branch, video from the frames, optional Qt
Graphs comparison, optional README screenshots, try drag-and-drop and the export dialogs by
hand).

**Where things are**
- Screenshots: `evidence/screenshots/01-plots-live-dark.png` … `06-plots-tooltip-light.png`
- Frames: `evidence/frames/frame_0000.jpg` … `frame_0540.jpg` (12 fps); zip `evidence/frames.zip`
- Benchmarks: `evidence/plot-bench-sg.txt`, `evidence/plot-bench-charts.txt`
- Edge cases: `evidence/checks/*.png` (incl. `export-plot.png`), `evidence/export-visible-range.csv`

**How to build and run**
```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1          # build, deploy, start
build\local-release\can_analyzer.exe --demo --rate 1000         # re-capture the demo
build\local-release\can_analyzer.exe --plot-bench bench.txt     # plot benchmark
powershell -ExecutionPolicy Bypass -File tools\zip-frames.ps1    # zip the frames
```
In the app: *Load demo database* → Plots tab (Ctrl+4) → click + next to signals.

**Risks / known limitations**
- Qt Graphs was not benchmarked (not installed).
- Drag and drop and the PNG/CSV save dialogs could not be operated by hand in an unattended
  run; the drop handlers and both exports were exercised programmatically (layout check).
- Per-unit scaling supports a left and a right axis; a third unit in one lane is scaled on its
  own without an axis.
- Quantised signals (e.g. throttle in 0.4 % steps) show visible stairs when zoomed in far;
  that is the real data.
- Demo capture JPEG quality is 80 (85 exceeded 60 MB).
- The store keeps 10 minutes per signal; at 2000 fr/s with many plotted-or-not signals memory
  grows to tens of MB (all signals of the database are recorded).

---

## Summary (Phase 2, unattended run, 2026-11-19 09:16 → 09:55)

**Done** (branch `phase2-decoding`, from `phase1-foundation`; not pushed, not merged)
- **DBC parser** (own code, Qt Core only): `BO_`, `SG_` (Intel/Motorola, signed/unsigned,
  factor/offset, min/max, unit, receivers), simple multiplexing (`M`/`mN`, stretch goal done),
  `VAL_`, `CM_` (multi-line), `BA_ "GenMsgCycleTime"`; standard and extended IDs; every error
  has a line number and a plain-English message; files with errors are rejected as a whole.
- **Decoder**: raw value, physical value, unit, value-table text, out-of-range flag; one
  bit-mapping routine shared by decoding, encoding and the bit-layout view.
- **demo_vehicle.dbc** (invented): 9 messages, 43 signals, bundled as a resource.
- **Simulator "Demo vehicle" mode**: deterministic physics (drive cycle, rpm from speed and
  gear, slow coolant warm-up, steering sweep, brake events, battery, multiplexed trip
  computer); "Raw patterns" (phase 1) still selectable.
- **UI**: load demo / open / unload database (file dialog), friendly error banner; Names |
  Hex toggle; decoded expansion in Trace (selected frame) and Messages (expandable rows); new
  **Signals** tab (grouped, live value, unit, enum chip, 10 s sparkline, min/max seen, search,
  collapse, pin); **signal detail** (live value + chart, bit layout of the 8 bytes with MSB/LSB
  and live bit values, definition, comment, value table). Same design system as phase 1.
- **Tests**: 11 suites via CTest, **all passing, 159 checks** (new: `tst_dbcparser` 27,
  `tst_decoder` 24, `tst_vehiclesim` 9 incl. generate → decode → compare round trip,
  `tst_signalmodel` 7). Clean rebuild with `/W4`: 0 warnings.
- **Demo capture**: deterministic 12 fps (virtual clock for simulator, script and
  animations), 541 JPEG frames of the application window only, 6 named PNG screenshots,
  `evidence/frames.zip` = 38.2 MB.
- **Design review**: 3 rounds (`docs/decisions.md`), incl. a real layout bug fixed at the
  960 × 600 minimum size; final frames/screenshots captured after round 3.
- **Docs**: README (features, architecture diagram with parser/decoder, loading databases,
  supported DBC subset and limitations, screenshot placeholders), `docs/phase2-explained.md`.
- **Scan** of tracked files: no machine paths, names, emails or secrets.

**Blocked**: nothing.

**Needs me**: see `docs/needs-me.md` (review the branch; make the video from the 12 fps
frames; optionally copy phase 2 screenshots into `docs/img/`).

**Where things are**
- Screenshots: `evidence/screenshots/01-trace-decoded-dark.png` … `06-signal-detail-light.png`
- Frames: `evidence/frames/frame_0000.jpg` … `frame_0540.jpg` (12 fps, 1280 × 800, q85);
  zip: `evidence/frames.zip` (38.2 MB)
- Edge cases: `evidence/checks/*.png`; capture log: `evidence/demo-log.txt`

**How to build and run**
```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1            # build, deploy, start
build\local-release\can_analyzer.exe --demo --rate 1000           # re-capture the demo
powershell -ExecutionPolicy Bypass -File tools\zip-frames.ps1      # zip the frames
```
In the app: *Load demo database* (tab row, right) → Signals tab (Ctrl+3).

**Risks / known limitations**
- DBC subset only: no CAN FD, extended multiplexing, float signals, named value tables.
- JPEG quality was lowered from 90 to 85 (and frames stored at logical 1280 × 800 instead of
  1600 × 1000 device pixels) to stay below the 60 MB budget.
- The deterministic capture relies on Qt's "basic" render loop and a custom animation driver
  (`--demo` only); the normal app uses Qt's default render loop.
- Sparklines repaint with every 30 ms batch; fine for 43 signals, may need throttling for
  databases with thousands of signals.
- The file dialog (Qt Quick Dialogs) could not be exercised interactively in this unattended
  run; loading was tested through the same `loadFile` path in tests and in `--layout-check`.

---

## Summary (Phase 1, unattended run, 2026-11-19 04:23 → 05:17)

**Done**
- C++20 core (Qt Core only): `CanFrame` + formatting, `RingBuffer` (100k, sequence
  numbers), `FrameSource` interface, deterministic `SimulatorEngine` (12 invented IDs,
  10–1000 ms periods, counters / sines / random walks / bit flags / variable DLC, bursts,
  rate up to 2000+ fr/s) running in its own thread (`SimulatedSource`), `FrameModel` (ring
  buffer + filter index, 30 ms batches, per-ID Δt), `MessageOverviewModel` (count, period,
  last data, last seen, change masks, stable sorting), `FrameFilter` (ID single/list/range,
  DLC, standard/extended), `BusStatistics` (fr/s, bus load at 500 kbit/s, totals),
  `CaptureSession` (batching, pause without loss, clear).
- Design system: `Theme` token singleton (dark-first + light, one iris-violet accent,
  semantic colors, type/spacing/radius/elevation/motion scales, WCAG AA verified),
  components (Card, Chip, IconButton, SectionHeader, EmptyState, Kbd, Divider, ExtTag,
  FocusRing), a real Controls style `CanStyle` (Button, ToolButton, TextField, ComboBox,
  TabBar, TabButton, ScrollBar, Switch, ToolTip), 18 hand-drawn SVG icons.
  Documented in `docs/design.md`.
- UI: toolbar (Start/Stop, Pause, Clear, rate, bursts, theme), status chips, Trace tab
  (filter bar, sticky header, zebra, hover, selection, right-aligned numbers, follow/
  detach with "Jump to live", rate-gated fade-in, paused pill), Messages tab (sortable,
  changed bytes flash), animated theme/tab/state changes, shortcuts, empty states,
  friendly filter errors, 960×600 minimum size.
- Tests: 7 Qt Test suites via CTest, **all passing** (92 checks incl. data rows).
- `--demo` capture: 224 window-only frames at 5 fps (45 s) + 5 named screenshots;
  `--layout-check` edge-case screenshots; frames zipped.
- 3 design-review rounds (logged in `docs/decisions.md`), which also found and fixed two
  real bugs (auto-scroll stopping once the buffer is full; detached view drifting on
  eviction).
- README (features, mermaid architecture, build with presets/`QT_ROOT`, demo, roadmap,
  licensing), `docs/phase1-explained.md`, secrets scan.

**Blocked**: nothing.

**Needs me**: see `docs/needs-me.md` (review/push the branch, make the video from the
frames).

**Compiler kit**: MSVC, Visual Studio 18 Community (v145 toolset), Qt 6.12.0
`msvc2022_64`, CMake + Ninja from the Qt installation. Clean rebuild at `/W4 /permissive-`:
0 warnings (C4702 from Qt headers disabled, see decisions). MinGW fallback was not needed.

**How to build**
```bat
copy CMakeUserPresets.json.example CMakeUserPresets.json   & rem then set QT_ROOT + Ninja path
cmake --preset local-release
cmake --build --preset local-release
ctest --preset local-release
cmake --build --preset local-release --target deploy        & rem copies the Qt runtime
build\local-release\can_analyzer.exe --demo --rate 2000
powershell -ExecutionPolicy Bypass -File tools\zip-frames.ps1
```
(MSVC builds need a shell where `vcvars64.bat` has run.)

**Where things are**
- Screenshots (final round): `evidence/screenshots/01…05-*.png`
- Frames: `evidence/frames/frame_0000…0223.png`, zipped as `evidence/frames.zip` (≈41 MB)
- Edge-case checks: `evidence/checks/*.png`; run log: `evidence/demo-log.txt`
- `evidence/` is git-ignored.

**Risks / known limitations**
- Render rate measured at ~50–54 fps at 2000 fr/s *while* grabbing PNGs; it is not
  profiled on slower GPUs or very high-DPI screens.
- Bus load ignores bit stuffing (documented); real load is up to ~20 % higher.
- The period of the bursty ID 0x5B0 is an average over bursts and gaps, so it reads oddly
  (documented behaviour of the EMA).
- Mouse-wheel detach relies on `WheelHandler { blocking: false }`; it was exercised in
  code paths but not with a physical wheel during this unattended run.
- Screenshots are at 125 % scaling (1600×1000); window title is the generic "CAN Analyzer".
- Fonts assume Windows 11 (Segoe UI Variable, Cascadia Mono); Windows 10 falls back to
  Segoe UI / Consolas.

## Log

| Time (local) | What finished |
|---|---|
| 2026-11-19 04:23 | Surveyed toolchain: Qt 6.12.0 msvc2022_64 + mingw_64 kits, CMake, Ninja, VS 18 Community (v145 toolset) found via vswhere. |
| 2026-11-19 04:26 | Git repo initialised, branch `phase1-foundation`, CLAUDE.md, .claude/settings.json, .gitignore. |
| 2026-11-19 04:35 | Core library + 7 test suites written; first MSVC build clean; all tests pass. Committed. |
| 2026-11-19 04:44 | Palette contrast verified (WCAG AA). Theme, components, icons, CanStyle controls, screens, demo director written. |
| 2026-11-19 04:51 | App builds and runs; first `--demo` capture (225 frames, 5 screenshots). Fixed `onAccent` token name and IconImage `name` clash. |
| 2026-11-19 04:56 | Design review round 1: rate-gated fade, Δt column, EXT tag, calmer highlights, stable count sort, full-width tab divider. Render rate ~50 fps at 2000 fr/s. |
| 2026-11-19 04:59 | Tests extended (Δt, sort buckets); all pass. Committed UI + round 1. |
| 2026-11-19 05:02 | Design review round 2: ghost highlights, header units, paused pill. |
| 2026-11-19 05:09 | Design review round 3: `--layout-check` mode; fixed follow stopping on a full buffer and detached-view drift on eviction; pill margin; dark highlight. Committed. |
| 2026-11-19 05:11 | Final demo capture, frames zipped (224 frames, 40.5 MB). |
| 2026-11-19 05:14 | design.md, README, phase1-explained.md; `deploy` target verified (deployed exe runs without PATH changes). |
| 2026-11-19 05:16 | Secrets scan clean; clean rebuild with 0 warnings; summary written. |
| 2026-11-19 | Machine-specific toolchain allow rules moved to git-ignored `.claude/settings.local.json`; `settings.json` keeps generic rules only. |
| 2026-11-19 07:59 | README screenshots: the 5 final window-only demo screenshots copied to `docs/img/` and linked in the README. |
| 2026-11-19 09:16 | Phase 2 started: branch `phase2-decoding` from `phase1-foundation`. |
| 2026-11-19 09:24 | DBC parser, codec/decoder, demo_vehicle.dbc, simulator vehicle mode; 10/10 suites pass. Committed. |
| 2026-11-19 09:30 | SignalDatabase, SignalListModel, decoded trace/overview roles, manual clock; 11/11 suites pass. Committed. |
| 2026-11-19 09:40 | Signals tab, detail panel, bit layout, sparklines, database bar, error banner; deterministic 12 fps capture working. |
| 2026-11-19 09:44 | Design review rounds 1–2: sparkline envelope, bit layout above the fold, demo pause, chevrons, JPEG budget. |
| 2026-11-19 09:47 | Design review round 3: minimum-size Signals layout fixed; final capture 541 frames, zip 38.2 MB. Committed. |
| 2026-11-19 09:55 | Docs (README, phase2-explained, decisions), clean rebuild 0 warnings, 11/11 suites (159 checks), scan clean. |
| 2026-11-19 10:50 | Phase 3 started: branch `phase3-plots` from `phase2-decoding`; Qt Graphs found not installed. |
| 2026-11-19 10:56 | Simulator realism fix + variation tests; sparkline scaling. Committed. |
| 2026-11-19 11:00 | TimeSeriesStore with decimation, plotmath, PlotModel, PlotController + 3 test suites (14 suites pass). Committed. |
| 2026-11-19 11:06 | PlotCanvas scene-graph engine; benchmarks: 60.0 fps custom vs 36.2 fps Qt Charts (OpenGL). Committed. |
| 2026-11-19 11:13 | Plots tab UI (lanes, picker, cursors, thresholds, export); deterministic plots demo. |
| 2026-11-19 11:18 | Design review round 1: picker, per-unit axes, enum ticks. |
| 2026-11-19 11:20 | Design review round 2: measurement bar, tick/unit collisions, lane controls, JPEG q80. |
| 2026-11-19 11:30 | Design review round 3: demo mode ignores the real mouse, threshold badges, cursor/tooltip bindings, axis smoothing fix. |
| 2026-11-19 11:37 | Final capture (541 frames, zip 40.6 MB), clean rebuild 0 warnings, 14/14 suites. Committed. |
| 2026-11-19 11:45 | Export check, docs (README, phase3-explained, decisions), scan clean, summary. |
| 2026-11-19 12:43 | Phase 4 started: branch `phase4-release` from `phase3-plots`. |
| 2026-11-19 13:01 | Core: .canlog writer/reader with index, recorder, replay source, markers, search, summary, CSV; 3 new test suites (17/17 pass). Committed. |
| 2026-11-19 14:53 | App: record/replay/search/summary/About UI, welcome screen, settings, icon, version resource, packaging target, can_logtool; builds with 0 warnings. |
| 2026-11-19 14:58 | First 60 s capture; design review round 1 (toolbar overflow, fuzzy step line, chip labels, F3 start, replay bus load). |
| 2026-11-19 15:00 | App layer committed. |
| 2026-11-19 15:02 | Round 2 (incomplete-condition error, data chip text, status bar at min size, replay pill); CSV export 28 s → 0.27 s; 1M-frame benchmark. |
| 2026-11-19 15:05 | Release zip built; MSVC runtime and qmltooling fixes; clean-copy smoke test passed without Qt on PATH. |
| 2026-11-19 15:08 | Round 3 (markers under lanes, Follow label); final layout check + capture (721 frames, zip 52.6 MiB); handoff copies. |
| 2026-11-19 15:12 | Settings round trip and missing-database test (registry cleaned up afterwards). |
| 2026-11-19 15:17 | Clean rebuild 0 warnings, 17/17 suites, final package, scan clean, docs, tag v1.0.0. |
