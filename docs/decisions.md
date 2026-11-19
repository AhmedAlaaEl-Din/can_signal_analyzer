# Decisions

Decisions taken autonomously during the unattended Phase 1 run. Each entry: what, why.

## Toolchain
- **Compiler kit: MSVC (Visual Studio 18 Community, v145 toolset) with Qt `msvc2022_64`.**
  vswhere found the C++ workload, so the preferred kit is used. The v145 toolset is
  ABI-compatible with the msvc2022 Qt binaries (MSVC 2015+ binary compatibility).
- **Build environment:** `vcvars64.bat` is called inside a throw-away `cmd` per build; the
  global PATH is never modified. CMake and Ninja are referenced by full path via
  `CMakeUserPresets.json` (git-ignored).
- **Running from the build folder:** tests get the Qt `bin` folder prepended to their own
  PATH through the CTest `ENVIRONMENT_MODIFICATION` property (derived from the Qt target,
  nothing hard-coded). The app is made runnable with `windeployqt` into the build folder.
- **Run-time PATH:** to launch the app and tests from the build tree, the Qt `bin` folder is
  prepended to PATH only inside the launching process (tests: CTest property; app: a local
  wrapper script). An optional `deploy` CMake target runs `windeployqt` for a standalone folder.

## Architecture
- **Core is Qt Core only** (`can_core` static lib: CanFrame, RingBuffer, SimulatorEngine,
  SimulatedSource, FrameFilter, BusStatistics, FrameModel, MessageOverviewModel,
  CaptureSession). Models derive from `QAbstractTableModel`, which lives in Qt Core.
- **Simulator split in two:** `SimulatorEngine` is pure, deterministic logic (own SplitMix64
  PRNG, because `std::` distributions differ between standard libraries); `SimulatedSource`
  runs it in a worker `QThread` on a 10 ms precise timer and emits batches. Output is
  independent of how time is chunked, which the tests verify.
- **Rate control:** the 12 invented messages have nominal periods (10…1000 ms, ≈366 fr/s
  total). A target rate scales all periods by the same factor, so relative timing stays
  realistic up to 2000 fr/s (max 5000).
- **Batching:** frames from the source thread are queued and pushed into the models every
  30 ms (`CaptureSession`), one insert notification per batch.
- **Pause:** freezes both views but not the capture; frames received while paused are parked
  (bounded by the trace capacity) and published on resume, so nothing is lost.
- **Trace rows are referenced by sequence number** (ring buffer index that never repeats), so
  eviction and filtering stay O(batch) / O(buffer) and selection survives eviction.
- **Filter scope:** the filter applies to the Trace only; Messages always shows every ID.
- **Bus load** uses nominal frame lengths without bit stuffing (47 + 8·DLC bits standard,
  67 + 8·DLC extended, incl. 3-bit interframe space) over a 1 s sliding window at 500 kbit/s.
- **QML access to the core:** `CaptureSession` is registered as the `Session` singleton
  instance (`CanAnalyzer.Core`) from `main.cpp`, so the core stays free of QML macros.

## Design system
- **Accent: iris violet** (`#5A43D6` light / `#A594FF` dark). Bus tools mostly use blue or
  grey; violet is distinctive and never collides with the ok/warning/error hues.
- **Fonts:** Segoe UI Variable (Text/Display) for UI, Cascadia Mono for hex/IDs/times; both
  ship with Windows 11. Fallbacks: Segoe UI, Consolas. Picked at runtime from
  `Qt.fontFamilies()`. Numbers in chips use the `tnum` OpenType feature.
- **Theme switch animation:** every color token is a blend of the light and dark palette,
  driven by one animated value (200 ms, InOutCubic).
- **Custom Controls style** `CanStyle` (Button, ToolButton, TextField, ComboBox, TabBar,
  TabButton, ScrollBar, Switch, ToolTip) selected with `QQuickStyle::setStyle`; unlisted
  controls fall back to Basic.
- **Contrast** of every text token against every surface was computed (WCAG formula):
  lowest is 4.79:1 (light `textFaint` on `bg`), so all pass AA.
- `onAccent` token renamed to `textOnAccent`: QML parses `on` + capital letter as a signal
  handler.

## Design review — round 1 (first screenshots)
Findings → changes:
1. Live trace looked washed out: at ≥ 1000 fr/s every visible row is younger than the 200 ms
   fade, so all rows were permanently semi-transparent. → Fade-in is now rate-gated: rows fade
   in only while fewer than ~1.5 screens of rows arrive per second (e.g. narrow filters);
   high-rate streams render directly.
2. Messages: change highlights were loud (beige blocks everywhere), stuck on placeholder
   cells after re-sorting, cells too tight, "··" placeholders added noise. → Calmer tint
   (15–22 % alpha), flashes reset when a delegate is re-assigned to another ID, 4 px gaps,
   absent bytes left blank.
3. Sorting by Count made rows with nearly equal counts swap on every update (jitter and lost
   highlights). → Live numeric sort keys compare at two significant digits; ties fall back
   to ID order (unit-tested).
4. ID column too wide and the "x" suffix for 29-bit IDs was cryptic. → Narrower column, small
   outlined `EXT` tag.
5. Empty band to the right of short payloads and no timing info per ID. → New Δt (ms) column
   (time since the previous frame with the same ID), computed on arrival (unit-tested).
6. Tab underline border stopped at the last tab. → One full-width divider under the tab row.

## Design review — round 2
Findings → changes:
1. Messages showed ghost highlight boxes on cells that no longer hold a byte (ID 0x6F0 has a
   varying DLC). → Highlights are hidden and stopped when a byte cell becomes empty.
2. Upper-casing header titles turned units into nonsense ("Δt (ms)" → "ΔT (MS)"). → Titles
   are written in caps in the column definitions and units stay lower case: `TIME (s)`,
   `Δt (ms)`, `PERIOD (ms)`.
3. Pause was only visible in the status chip and the toolbar. → A floating pill on the table
   ("Paused · N new frames waiting · Resume") on both tabs; it doubles as "Jump to live" when
   the user has scrolled away from the live end.
4. Demo scene extended with a slow ID range (0x601–0x6FF) so the rate-gated fade-in is
   visible in the frames.

## Design review — round 3
A `--layout-check` mode was added that captures edge cases into `evidence/checks/`
(empty states, the 960×600 minimum window, a filter error, a no-match filter, a wide window,
detached and paused views; `--capacity` shrinks the ring buffer to force eviction).
Findings → changes:
1. The floating pill covered the newest rows. → The list gets a bottom margin while the pill
   is visible; `positionViewAtEnd()` ignores that margin, so the follow logic adds it.
2. **Bug:** once the ring buffer was full, every batch removed as many rows as it added, so
   `count` stopped changing and live auto-scroll silently stopped (after 50 s at
   2000 fr/s). → Follow mode now reacts to the model's `rowsInserted`.
3. **Bug:** while scrolled away from the live end, eviction made the visible rows drift. A
   pixel counter-scroll fought ListView's own bookkeeping, so the view is now anchored by
   frame sequence number: the top visible frame is remembered and restored with
   `positionViewAtIndex` after each eviction (verified: identical rows 0.9 s and 1,500
   evicted frames apart).
4. Dark-mode change highlight looked muddy (amber over blue-grey). → Lower alpha in dark.
5. Verified: no layout breakage at 960×600 (toolbar, filter bar, chips and both tables fit),
   friendly inline filter error, clear empty states.

## Housekeeping
- **`design_refs/` does not exist**, so no reference images were used; the look was derived
  from first principles (see `docs/design.md`).
- **Claude Code permissions are split:** `.claude/settings.json` (tracked) holds only
  generic rules (`cmake`, `ctest`, `ninja`, git status/diff/add/commit, ask on push/merge,
  deny `.env`). The allow rules with this machine's toolchain paths live in
  `.claude/settings.local.json`, which is git-ignored (listed in `.gitignore`).
- **Demo timing:** the scene runs at `--rate 2000` for the delivered frames, to show the
  UI under the maximum specified load. The default interactive rate is 1000 fr/s; the app
  starts stopped, showing the empty state.
- **Test isolation:** each test's PATH and `QT_QPA_PLATFORM=offscreen` are set via the CTest
  `ENVIRONMENT_MODIFICATION` property only.
- **Line endings:** `.gitattributes` normalises text to LF (batch files CRLF).
- The shell wrapper used during the session mis-parsed apostrophes inside heredocs; files
  were written with the editor tool instead (no impact on the project).

## Run script and Qt Creator (follow-up)
- **`scripts/run.ps1`** builds (local-release, else release + `QT_ROOT`), deploys and starts
  the app. It loads `vcvars64.bat` into its own process only and prefers the CMake/Ninja
  shipped with Qt over the copies Visual Studio puts on PATH, so a build folder is never
  configured by two different CMake versions. No machine paths are stored in the script.
- **QML module output moved from `<build>/qml` to `<build>/qmlmodules`:** the `deploy`
  target (windeployqt) copies Qt's own QML modules into `<build>/qml`. Sharing that folder
  made the next configure scan half-deployed Qt files ("Cannot open input file …").
  The project's modules are compiled into the exe, so their output folder only matters
  to tooling.
- **Qt Creator 21 setup documented in the README** after checking this machine: the
  installer-registered kits/Qt versions/CMake point to a drive where Qt is not installed, so the
  README describes a manual kit (Qt version, auto-detected VS 2026 compiler, Qt's CMake,
  Ninja via `CMAKE_MAKE_PROGRAM`). CDB is not installed, so debugging with the MSVC kit is
  not available until the Windows SDK debugging tools are added.

---

# Phase 2: signal database, decoding, signal list

## Core
- **Own DBC parser** (`src/core/dbc.*`), statement-oriented: `BO_`/`SG_` per line, `;`-terminated
  statements (`CM_`, `VAL_`, `BA_`, ...) may span lines (multi-line comments work). Errors carry
  the line number of the statement start and a plain-English message. A file with any error is
  rejected as a whole (no half-loaded database); warnings (unknown references, unsupported
  keywords) do not block loading.
- **Supported subset:** `BO_` (standard + extended IDs via bit 31), `SG_` (Intel/Motorola,
  signed/unsigned, factor/offset, min/max, unit, receivers), simple multiplexing (`M` / `mN`),
  `VAL_`, `CM_` (global, message, signal), `BA_ "GenMsgCycleTime"` (used as the message period).
  **Not supported:** CAN FD (DLC > 8 is reported as an error), extended multiplexing (`m1M`,
  `SG_MUL_VAL_`), float/double signals (`SIG_VALTYPE_`), signal groups, environment variables,
  attribute definitions other than the cycle time, value tables by name (`VAL_TABLE_`).
- **Strict geometry check:** a signal whose bits do not fit into its message's DLC is an error,
  because it could never be decoded.
- **Codec** (`signalcodec.*`): one routine lists the occupied data bits MSB→LSB for both byte
  orders (DBC "sawtooth" numbering for Motorola); extraction, insertion and the bit-layout view
  all use it, so they cannot disagree. Physical = raw × factor + offset; encoding rounds and
  clamps to the representable raw range.
- **Value formatting:** decimals derived from factor/offset (0.01 → 2, 0.125 → 3, capped at 4).
- **Demo database** `demo_vehicle.dbc`: 9 invented messages, 43 signals, bundled as a Qt
  resource of the core library. Deliberately covers Intel + Motorola, signed values, signals
  that cross byte boundaries, value tables, a multiplexed message and an extended ID.
- **Simulator vehicle mode:** a deterministic `VehicleModel` (drive cycle idle → accelerate →
  cruise → brake, rpm from speed and gear ratios, first-order lags, coolant warm-up with a
  70 s time constant, steering sweep, battery, trip computer) is advanced to each frame's
  timestamp and encodes signals through the same codec. Raw mode (phase 1 patterns) is still
  selectable ("Source: Raw patterns"); bursts only exist in raw mode.
- **Signal list model:** flat list (headers + signals) rebuilt on search/collapse/pin; live
  values only emit `dataChanged` (one notification per 30 ms batch). Pinned signals move to a
  "Pinned" group at the top. History: one bucket per 40 ms with value + min/max, ~12 s ring.
- **Messages "decoded" role** decodes on demand, so only expanded rows cost anything.
- **Trace expansion:** the selected frame expands to show its decoded signals (frames are
  immutable, so decoding happens once on selection).

## UI
- New Signals tab (grouped list, search, collapse, pin, sparkline, min/max seen, enum chip)
  and a detail panel (live value + 10 s chart, bit layout, definition, comment, value table).
- **Sparklines** are a small C++ `QQuickPaintedItem` that reads the history directly from the
  model (no QVariant lists per frame); QML only passes model, signal id and a revision.
- Database controls live in the tab row (contextual), the toolbar stays capture-related; a
  "Source" selector was added there. Names | Hex is a segmented toggle (Ctrl+N).
- Bad database files: dismissible error banner with the first problems and line numbers; the
  previously loaded database stays active.

## Demo capture (Phase 2)
- **Deterministic 12 fps:** `--demo` switches the session to a manual clock and installs a
  custom `QAnimationDriver`; per frame the scene, the simulator (exactly 1/12 s) and all QML
  animations advance, then `grabWindow()` renders the window. Qt's single-threaded "basic"
  render loop is selected for this process so the driver controls all animation time.
- **Size budget:** the first capture (JPEG quality 90 at 1600 × 1000 device pixels, Windows
  scaling 125 %) was 95.8 MB. Frames are now stored at the window's logical size
  (1280 × 800, smooth downscale) and quality 90 still gave 64.7 MB, so the quality was lowered
  to **85** → 541 frames, 54.6 MB, **frames.zip 38.2 MB**.
- In the scene the trace is **paused** before a row is selected: at 1000 fr/s a selected row
  scrolls away within one batch while following the live end (a real user would pause too).

## Design review — Phase 2, round 1
1. Alive counters (0–15 every 16 frames) rendered as an aliased hatch in the sparklines. →
   History buckets keep min/max; buckets wider than 20 % of the visible range are drawn as a
   soft band instead of a line.
2. The bit layout (key feature) was below the fold in the detail panel. → Moved directly under
   the live value; chart height 64 → 52 px.
3. The trace screenshot showed no expanded row (it scrolled away). → Demo pauses first.
4. Messages: the expand chevron sat on the card edge next to the accent bar. → Inside the name
   column, ID column widened when a database is loaded.
5. `Sparkline.fillColor` shadowed `QQuickPaintedItem::fillColor`. → Renamed `areaColor`.
6. Frames 95.8 MB. → Logical-size frames (see above).

## Design review — Phase 2, round 2
1. Guide lines under decoded values read as underlines (busy). → Lower alpha.
2. Frames still 64.7 MB. → JPEG quality 85.
3. `DelegateModel::cancel` warnings while typing in search (reused delegates during model
   resets). → No delegate reuse in the (small) signal list.
4. Verified: Motorola bit layout (EngineTorque 39|12 → B4 7..0 + B5 7..4, MSB solid, LSB
   outlined), counters as calm bands, paused trace with decoded expansion.

## Design review — Phase 2, round 3 (layout check)
1. **Bug:** at the 960 × 600 minimum size with the detail panel open, the list's implicit
   width pushed the 384 px panel off-screen. → List may shrink (min 300 px), panel width
   adapts (40 % of the page, 300–384 px), lower-priority columns drop out (MIN–MAX below
   760 px, STATE below 600 px), Collapse/Expand become icon-only.
2. Verified: bad-DBC banner with line numbers, no-database empty state with actions,
   multiplexed TripComputer decoding only the current page, wide window, raw mode.
3. Final frames and screenshots were re-captured after this round.

---

# Phase 3: live signal plots

## Simulator and sparkline fixes (from the phase 2 demo)
- **Torque was clamped at 180 Nm, throttle/load flat.** Torque now follows
  `throttle × full-load curve(rpm) − friction(rpm)` (engine braking when coasting in gear,
  small positive at idle); load follows torque; the pedal varies while accelerating (eases off
  near the target, small corrections) and while cruising (road load + grade); idle-air throttle
  (~3 %) and residual brake pressure (~0.4 bar) are never exactly 0; steering amplitude shrinks
  with speed so lateral acceleration never clips; slow signals (SoC, fuel, battery temperature,
  ambient) move visibly within a minute (fuel use is demo-accelerated).
- **Tests:** every analog signal (no value table, has a unit, ≥ 8 bits, not a counter) must
  vary by ≥ 2 quantisation steps over 60 s and never sit at a DBC range limit for > 6 s;
  throttle/torque/load must change within every 4 s window.
- **Sparklines** scale to the samples inside the visible window only (+8 % margin); the
  min/max band for fast counters is unchanged.

## Time-series store
- One growable power-of-two ring per signal (`TimeSeries`), time-ordered, trimmed to the
  configured history (default 10 min). Memory follows the real rate instead of a guessed
  capacity.
- **Decimation:** `decimate(t0, t1, columns)` returns per-pixel-column `count/min/max/first/
  last`. Every sample lands in exactly one column, so a one-sample spike is never lost.
  When a column holds ≥ 128 samples, 64-sample block summaries are used instead of raw
  samples (blocks are only used when fully inside the data and inside one column).
  Measured: 8 signals × 60k samples → 1200 columns in 3.3 ms per frame.
- Fed by `SignalListModel` (decoded values) once per 30 ms batch, so pause/resume and clear
  behave exactly like the other views (pause parks frames; nothing is lost).

## Plot engine choice
- **Qt Graphs is not installed** on the development machine (only Qt Charts), and installing
  modules was not allowed. The benchmark therefore compares the custom scene-graph item
  against **Qt Charts**, the installed alternative. See needs-me for a Qt Graphs comparison.
- Workload: 8 signals × 1000 Hz (8000 samples/s, the decoded output of ~2000 frames/s),
  10 s window = 10,000 visible points per signal, 4 lanes, 1280 × 800 (DPR 1.25), 4× MSAA,
  Direct3D 11. Commands: `can_analyzer --plot-bench <file>`, `can_charts_bench <file>`.

| Engine | fps | p50 | p95 | CPU per frame |
|---|---|---|---|---|
| Custom scene graph, min/max decimation (used) | **60.0** | 16.7 ms | 17.8 ms | 1.9 ms prep + 1.5 ms geometry |
| Custom scene graph, all 80k raw points | 51.5 | 17.2 ms | 32.0 ms | 1.5 + 8.5 ms |
| Qt Charts, useOpenGL: true | 36.2 | 28.6 ms | 36.2 ms | 10.1 ms feeding series |
| Qt Charts, raster | 2.7 | 426 ms | 439 ms | 233 ms feeding series |

  → **Custom scene-graph item** (`PlotCanvas`): vsync-locked 60 fps with headroom, full
  control over the look (gradient fill, min/max band, dash cue, steps), no extra module.
- **Anti-aliasing:** thick lines are triangle strips with miter joins (dashes as quads);
  smooth edges come from 4× MSAA on the window (default surface format) rather than
  per-vertex feathering. Line width 1.75 px.
- Data preparation (store access, decimation, autoscale ranges) runs in `updatePolish()` on
  the GUI thread; `updatePaintNode()` only builds geometry.
- **Smooth follow-live:** data arrives every 30 ms but the view should move every frame. A
  `FrameAnimation` advances a "display now" by the frame time, never past the newest sample,
  snapping only if it falls > 80 ms behind.

## Plot UI decisions
- Lanes share one time axis; each lane groups its series **by unit**: the first unit uses the
  left axis (and the grid), a second unit gets its own scale on a right axis (labels tinted
  like that series), further units are scaled on their own. Enumerations form their own
  group with every integer state labelled (value-table text where available).
- Second visual cue: the second series in a lane is dashed by default; clicking the line
  sample in a legend chip toggles solid/dashed.
- Measurement readout lives in a slim bar above the plot (not over the data).
- Picker on the Plots tab supports click and drag-and-drop (dragging from the Signals tab is
  impossible because the two tabs are never visible together; the Signals tab and the detail
  panel have a Plot button instead).
- PNG export grabs the plot card (`grabToImage`); CSV exports the visible time range, one
  column per visible signal, one row per distinct sample time.
- JPEG quality for the plots demo: **80** (85 produced 62.6 MB > 60 MB budget); final
  541 frames = 53.4 MB, zip 40.6 MB.

## Design review — Phase 3, round 1
1. Signal picker was empty (built before the plot model had the database). → Picker derived
   from a binding on the signal count; the plot model is updated before the signal model.
2. Speed and rpm shared one y-axis (speed flattened, axis labelled km/h but showing rpm). →
   Per-unit scaling with left/right axes, axis labels tinted per series.
3. Enumerations showed only labelled states (Neutral, Reverse). → Every integer state is a tick.
4. `setWindowSeconds` was called as a method (property setter) and `PlotCanvas.palette`
   shadowed `QQuickItem::palette`. → Property assignment; renamed to `colors`.

## Design review — Phase 3, round 2
1. The cursor delta card covered the data, cursor B's handle and the right axis. → Moved to
   a measurement bar above the plot card.
2. Bottom tick labels collided with unit labels ("1500" over "rpm"). → Labels within 14 px of
   the bottom edge are dropped.
3. Lane controls overlapped the right axis labels. → Placed inside the plot region.
4. Frames 62.6 MB. → JPEG quality 80.

## Design review — Phase 3, round 3
1. **Bug:** the real mouse leaked into the deterministic capture (crosshair at the physical
   cursor position in every frame). → The director sets `demoMode` on the window; hover
   handlers are disabled in demo mode.
2. Demo threshold was outside the lane's left (throttle %) axis and invisible. → Demo uses a
   throttle value; thresholds outside the range now show a badge pinned to the edge (▲/▼).
3. **Bug:** cursor lines and the tooltip time did not follow scrolling (bindings calling
   `xOfTime()`/`timeAtX()` had no dependency on the view range). → View range added to those
   bindings.
4. **Bug:** at the minimum window size (wall-clock run) the left axis stayed stale: a
   `Behavior` retargeted every frame restarts at t = 0 and never advances. → Axis ranges ease
   towards their targets in a per-lane `FrameAnimation` (exponential smoothing, τ = 90 ms).
5. Verified: 960 × 600 Plots layout, empty states, PNG and CSV export from the app
   (layout check), both themes. Final frames/screenshots re-captured after this round.
