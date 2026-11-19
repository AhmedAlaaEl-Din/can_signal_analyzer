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
  installer-registered kits/Qt versions/CMake point to a non-existent `C:\Qt\Qt6.12`, so the
  README describes a manual kit (Qt version, auto-detected VS 2026 compiler, Qt's CMake,
  Ninja via `CMAKE_MAKE_PROGRAM`). CDB is not installed, so debugging with the MSVC kit is
  not available until the Windows SDK debugging tools are added.
