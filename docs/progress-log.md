# Progress log

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
