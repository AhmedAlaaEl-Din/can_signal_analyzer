# CAN Analyzer

A Windows desktop app that shows CAN bus traffic live, like the trace view of a
professional bus tool. C++20 core, Qt 6 Quick/QML front end, custom design system.

There is **no hardware** in this project: frames come from a built-in, seeded simulator.
Phase 2 adds a signal database: load a DBC file (or the bundled, invented
`demo_vehicle.dbc`) and frames are decoded into named signals with physical values, units
and value-table text. Later phases add plots, logging and replay.

## Screenshots

Captured by the app itself in `--demo` mode (application window only, 1280×800 at 125 %
display scaling, 2000 simulated frames/s).

| | |
|---|---|
| ![Live trace, dark theme](docs/img/trace-dark.png) | ![Live trace, light theme](docs/img/trace-light.png) |
| **Trace, dark**: live stream following the newest frame | **Trace, light**: same view in the light theme |
| ![Trace filtered by ID and paused](docs/img/trace-filtered-paused.png) | ![Messages overview, dark theme](docs/img/messages-dark.png) |
| **Filtered and paused**: ID filter `123, 1A4`, selected row, frames held while paused | **Messages, dark**: one row per ID, sorted by count, changed bytes highlighted |

![Messages overview, light theme](docs/img/messages-light.png)

**Messages, light**: the same overview in the light theme.

**Phase 2 (signal database):** screenshots are produced by `--demo` in
`evidence/screenshots/` (`01-trace-decoded-dark` … `06-signal-detail-light`).

| | |
|---|---|
| ![Signals tab, dark theme](docs/img/signals-dark.png) | ![Signal detail with bit layout, dark theme](docs/img/signal-detail-dark.png) |
| **Signals**: grouped by message, live values, 10 s sparklines, pinned signals on top | **Signal detail**: live value and chart, bit layout of the 8 data bytes (MSB solid, LSB outlined), definition |

Demo video: _link to be added_

## Features

- **Live trace**: time, channel, ID, per-ID Δt, DLC and hex data; follows the live end and
  stops following as soon as you scroll up ("Jump to live" brings it back, or press End).
- **Messages overview**: one row per ID with count, measured period, last-seen time and the
  latest payload; changed bytes flash briefly; sortable by any column.
- **Filtering**: IDs (single, list or range, e.g. `123, 1A0-1AF`), data length (`8`, `0-4`)
  and frame format (standard/extended). Invalid input shows a friendly inline message and
  keeps the last valid filter.
- **Pause / resume** that freezes the view but keeps capturing; nothing is lost.
- **Statistics**: frames/s, estimated bus load at 500 kbit/s, total frames, buffer fill.
- **Simulator**: "Demo vehicle" (the messages of `demo_vehicle.dbc`) or "Raw patterns"
  (12 invented IDs with counters, sines, random walks, bit flags, variable DLC, optional
  bursts); adjustable total rate up to 2000 fr/s; fully deterministic for a given seed.
- **Performance**: 100,000-frame ring buffer, 30 ms batched model updates; about 50 rendered
  frames/s at 2000 bus frames/s while capturing demo PNGs.
- **Signal database (DBC)**: own parser for the common DBC subset, with clear, line-numbered
  error messages for broken files. Load the bundled demo database with one click or open any
  `.dbc` file; unload at any time.
- **Decoding**: Intel and Motorola byte order, signed/unsigned, factor/offset, min/max range
  checks, value tables, simple multiplexing, standard and extended IDs.
- **Names or hex**: Trace and Messages show message names (or raw IDs, Ctrl+N); the selected
  trace frame and expanded Messages rows show their decoded signals.
- **Signals tab**: every signal grouped by message with live physical value, unit,
  enumeration text, a 10 s sparkline and min/max seen; search, collapse, pin to the top.
- **Signal detail**: live value and chart, start bit, length, byte order, scaling, range,
  sender/receivers, comment, value table and a **bit layout** of the 8 data bytes showing which
  bits the signal occupies (MSB/LSB marked, live bit values).
- **Demo vehicle**: the simulator can drive the demo database with plausible physics (speed
  ramps, rpm follows speed and gear, slow coolant warm-up, steering sweep, brake events);
  the phase 1 random patterns remain available as "Raw patterns".
- **Design**: dark and light themes with an animated switch, custom-styled controls,
  hand-drawn icons, WCAG AA contrast, keyboard shortcuts, empty states.

### Keyboard shortcuts

| Key | Action |
|---|---|
| F5 | Start / stop the simulator |
| Space or F6 | Pause / resume the view |
| Ctrl+L | Clear trace and statistics |
| Ctrl+F | Focus the ID filter (Signals tab: the search box) |
| Esc | Leave a filter field / reset the filter |
| End | Jump to the live end of the trace |
| Ctrl+1 / Ctrl+2 / Ctrl+3 | Trace / Messages / Signals tab |
| Ctrl+O | Open a DBC file |
| Ctrl+N | Toggle message names / hex IDs |
| Ctrl+T | Toggle dark / light theme |

## Architecture

```mermaid
flowchart LR
    subgraph worker["Simulator thread"]
        E["SimulatorEngine<br/>(seeded, deterministic)"] --> W["SimWorker<br/>10 ms precise timer"]
        VM["VehicleModel<br/>(demo vehicle physics)"] --> E
    end
    DBC[("demo_vehicle.dbc<br/>or user .dbc")] --> P["DbcParser"] --> DB["Database<br/>(immutable, shared)"]
    DB --> VM
    W -- "framesReady(batch)<br/>queued signal" --> S
    subgraph gui["GUI thread"]
        S["CaptureSession<br/>collects + flushes every 30 ms"]
        S --> ST["BusStatistics"]
        S --> FM["FrameModel<br/>ring buffer 100k + filter"]
        S --> MO["MessageOverviewModel"]
        S --> SL["SignalListModel<br/>live values + 10 s history"]
        DB --> D["Decoder<br/>(signal codec)"]
        D --> FM
        D --> MO
        D --> SL
        FM --> QT["Trace"]
        MO --> QM["Messages"]
        SL --> QS["Signals + sparklines<br/>+ detail / bit layout"]
    end
```

- `src/core/` — **no Qt Quick dependency** (Qt Core only): `CanFrame`, `RingBuffer`,
  `FrameSource` / `SimulatedSource` / `SimulatorEngine` / `VehicleModel`, `FrameFilter`,
  `BusStatistics`, `FrameModel`, `MessageOverviewModel`, `CaptureSession`, and for phase 2
  `DbcParser` / `Database` (`dbc.*`), the signal codec and `Decoder` (`signalcodec.*`),
  `SignalDatabase` and `SignalListModel`. The demo database is a resource of this library.
- `src/app/ui/` — module `CanUi`: the `Theme` token singleton, icons and components.
- `src/app/style/` — module `CanStyle`: the custom Qt Quick Controls style.
- `src/app/qml/` — module `CanAnalyzer`: the screens.
- `tests/` — Qt Test suites run by CTest.
- `docs/` — design system, decisions, progress log, plain-English explainer.

## Signal databases

**Load:** on the right of the tab row, *Load demo database* loads the bundled
`demo_vehicle.dbc`; *Open DBC…* (Ctrl+O) opens any file. The pill shows the loaded file and
its message/signal counts; the × unloads it. If a file cannot be loaded, a banner lists the
problems with line numbers (for example `Line 5: SG_ Mode: start bit 70 is out of range
(0-63)`) and the previously loaded database stays active.

**Supported DBC subset**

| Keyword | Supported |
|---|---|
| `BO_` | messages: ID (standard, or extended via bit 31), name, DLC 0–8, sender |
| `SG_` | start bit, length 1–64, Intel (`@1`) / Motorola (`@0`), signed (`-`) / unsigned (`+`), factor, offset, min, max, unit, receivers |
| `SG_ … M` / `mN` | simple multiplexing (one multiplexor per message) |
| `VAL_` | value tables (enumerations) for signals |
| `CM_` | comments: global, message, signal (may span lines) |
| `BA_ "GenMsgCycleTime"` | message period |
| `VERSION`, `NS_`, `BS_`, `BU_`, other `BA_*`, `VAL_TABLE_`, … | recognised and ignored |

**Not supported (known limitations):** CAN FD (DLC > 8 is reported as an error), extended
multiplexing (`m1M`, `SG_MUL_VAL_`), IEEE float/double signals (`SIG_VALTYPE_`), named value
tables (`VAL_TABLE_` references), signal groups and environment variables. A file with any
error is rejected as a whole; warnings (e.g. a comment for an unknown signal) do not block
loading.

## Building

Requirements: Qt 6.5+ (developed with Qt 6.12.0, MSVC 2022 64-bit kit), CMake ≥ 3.25,
Ninja, and a C++20 compiler (MSVC from Visual Studio 2022 or newer, or the MinGW kit that
ships with Qt).

Machine-specific paths are never committed. `CMakePresets.json` reads the Qt kit from the
**`QT_ROOT`** environment variable; your own paths go in a git-ignored
`CMakeUserPresets.json`.

### Option A: environment variable

From a "x64 Native Tools Command Prompt" (MSVC) with CMake and Ninja on PATH:

```bat
set QT_ROOT=C:\Qt\6.12.0\msvc2022_64
cmake --preset release
cmake --build --preset release
ctest --preset release
```

### Option B: user preset (recommended)

Copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json`, fill in your Qt kit and
Ninja paths, then:

```bat
cmake --preset local-release
cmake --build --preset local-release
ctest --preset local-release
```

### Build, deploy and run in one step (recommended)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run.ps1
```

`scripts/run.ps1` picks the `local-release` preset (or `release` + `QT_ROOT` when there is no
user preset), loads the MSVC environment with `vswhere`/`vcvars64.bat` inside its own
process, uses the CMake and Ninja that ship with Qt, builds, runs the `deploy` target
(windeployqt) and starts the app, printing each step. Options: `-NoLaunch` (build and
deploy only), `-Preset <name>`, `-AppArgs '--rate','2000'`.

After the deploy step the executable is:

```
<repo>\build\local-release\can_analyzer.exe
```

That folder then also holds the Qt DLLs, plugins and QML modules, so the exe can be
started by double-clicking it. With the generic `release` preset the folder is
`build\release\` instead.

### Running manually

The build tree does not contain the Qt DLLs. Either put `%QT_ROOT%\bin` on PATH for the
current shell, or deploy the runtime next to the executable once:

```bat
cmake --build --preset local-release --target deploy
build\local-release\can_analyzer.exe
```

Command-line options: `--rate <fps>` (initial rate, `0` = nominal), `--seed <n>`,
`--capacity <frames>` (ring buffer size), `--demo`, `--layout-check`, `--evidence-dir <dir>`.
The simulator starts in "Demo vehicle" mode; switch to "Raw patterns" in the toolbar.

## Opening the project in Qt Creator 21

Placeholders: `<QtDir>` is the folder you installed Qt into (the one that contains
`6.12.0\` and `Tools\`, e.g. `C:\Qt`), `<repo>` is your clone of this repository. The
versions are the ones the project was developed and checked with.

| What | Path / value |
|---|---|
| Qt Creator 21 | `<QtDir>\Tools\Qt Creator 21.0.0-rc1\bin\qtcreator.exe` (21.0.0-rc1). If both are installed, `<QtDir>\Tools\QtCreator\bin\qtcreator.exe` may be an older release (20.0.2 on the development machine). |
| Qt version (qmake) | `<QtDir>\6.12.0\msvc2022_64\bin\qmake.exe` (Qt 6.12.0) |
| CMake | `<QtDir>\Tools\CMake_64\bin\cmake.exe` (3.30.5) |
| Ninja | `<QtDir>\Tools\Ninja\ninja.exe` (1.12.1) |
| MSVC | Visual Studio Community 2026 18.12, MSVC toolset 14.52.37025; `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat` |

**Known issue:** on the development machine the Qt installer registered its kits, Qt
versions, CMake and MinGW entries under a different drive than the one Qt was actually
installed on. The pre-made kit **"Desktop Qt 6.12.0 MSVC2022 64bit"** then shows a red
error (Qt version and CMake not found). If yours shows that error, do not use it; set up
the pieces once as below. If it is fine, you can pick it and skip to "Open, build, run"
(still add the Ninja line from step 4).

### One-time setup: Edit → Preferences → Kits

1. **Qt Versions** tab → **Add…** → select
   `<QtDir>\6.12.0\msvc2022_64\bin\qmake.exe` → it appears as
   "Qt 6.12.0 MSVC2022 64bit" under *Manual* → **Apply**.
2. **Compilers** tab → check *Auto-detected* → **C++** for an entry named
   "Microsoft Visual C++ Compiler 18.12… (amd64)". Qt Creator finds Visual Studio itself,
   so normally nothing is needed. If it is missing: **Add → MSVC → C++**, Initialization:
   `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat`,
   argument `amd64` → **Apply**. Repeat for **C** if you like (only C++ is used).
3. **CMake** tab (in some versions: *Preferences → CMake → Tools*) → **Add** →
   Path `<QtDir>\Tools\CMake_64\bin\cmake.exe`, name e.g. "CMake 3.30.5 (Qt, manual)"
   → **Apply**. If the auto-detected "CMake 3.30.5 (Qt)" entry shows an error, ignore it.
4. **Kits** tab → **Add** → fill in:
   - Name: `Qt 6.12.0 MSVC 64-bit (manual)`
   - Compiler C++: the *Microsoft Visual C++ Compiler 18.12… (amd64)* entry from step 2
     (C: the matching C entry, or leave it)
   - Qt version: *Qt 6.12.0 MSVC2022 64bit* from step 1
   - CMake Tool: the entry from step 3
   - CMake generator: **Ninja**
   - CMake Configuration → **Change…** → add the line
     `CMAKE_MAKE_PROGRAM:FILEPATH=<QtDir>/Tools/Ninja/ninja.exe` (with forward slashes)
     (Ninja is not on PATH, so CMake must be told where it is)
   - Debugger: none is available (see below)
   → **Apply** / **OK**. The kit must show no red error icon.

### Open, build, run

1. **File → Open File or Project…** → select `<repo>\CMakeLists.txt`.
2. On the *Configure Project* page tick only the kit **Qt 6.12.0 MSVC 64-bit (manual)** and
   its **Release** build (Debug is fine too). Qt Creator also lists the CMake presets from
   `CMakePresets.json` / `CMakeUserPresets.json` (`release`, `debug`, `local-release`)
   as extra entries; leave those unticked. Click **Configure Project**.
3. **Build → Build Project** (Ctrl+B), then **Run** (Ctrl+R). Running from Qt Creator
   needs no deploy step: Qt Creator adds the kit's Qt `bin` folder to the run
   environment. Command-line options (e.g. `--rate 2000`) go in
   *Projects → Run → Command line arguments*.
4. The unit tests appear under **Tools → Tests** (CTest) and can be run from there.

**Debugging:** the development machine has no CDB (the "Debugging Tools for Windows" feature of the
Windows SDK is not installed), so the MSVC kit can build and run but not debug. Installing
that Windows SDK feature adds `cdb.exe`, which Qt Creator then detects automatically.

## Demo mode

```bat
can_analyzer.exe --demo --rate 1000
powershell -ExecutionPolicy Bypass -File tools\zip-frames.ps1
```

`--demo` plays a scripted, **deterministic** scene of 45 s: live trace, loading the demo
database, a decoded trace frame, expanded Messages rows, the Signals tab with search and
pins, the signal detail with its bit layout, a theme switch and back. It saves:

- `evidence/frames/frame_NNNN.jpg` — 541 JPEG frames at exactly **12 fps** (1280 × 800,
  quality 85). Simulated time, the script and every QML animation advance by exactly 1/12 s
  per frame (manual clock + custom animation driver), so playback is smooth even though
  capturing runs at whatever speed the machine renders.
- `evidence/screenshots/01…06-*.png` — six named screenshots of the main screens.
- `evidence/demo-log.txt` — progress and sizes.

Only the application window is captured (`QQuickWindow::grabWindow` renders the window's
own scene graph), never the desktop. `tools/zip-frames.ps1` packs the frames into
`evidence/frames.zip` (about 38 MB). Assemble a video with any tool, e.g.
`ffmpeg -framerate 12 -i frame_%04d.jpg -c:v libx264 -pix_fmt yuv420p demo.mp4`.
`--layout-check` captures edge cases (no database, a broken DBC file, minimum window size,
multiplexed message, raw mode) into `evidence/checks/`. `evidence/` is git-ignored.

## Roadmap

1. ~~**Decoding**~~ — done in phase 2: DBC subset, decoder, signal list, bit layout.
2. **Plots**: live multi-signal charts with cursors and zoom (the signal history and
   sparkline renderer are the starting point).
3. **Logging**: record to an open log format with triggers and pre-trigger buffer.
4. **Replay**: a `FrameSource` that replays logs at 1×/n× speed; the rest of the app is
   already source-agnostic.
5. More DBC: extended multiplexing, float signals, CAN FD; hardware adapters behind the same
   `FrameSource` interface.

## Licensing

- Qt is used under the open-source **GNU LGPL v3**, **dynamically linked** (the Qt DLLs
  are separate files and can be replaced by the user). No Qt source code is modified.
- Qt modules used: **Qt Core, Qt Gui, Qt Qml, Qt Quick, Qt Quick Controls 2** (incl.
  Templates and the Basic fallback style), **Qt Quick Dialogs** (file dialog), **Qt Quick
  Effects** (shadows, via the `QtQuick.Effects` QML module), **Qt Svg** (icon rendering), and
  **Qt Test** (tests only).
- Fonts are not shipped: the app uses Segoe UI Variable / Cascadia Mono already installed
  with Windows.
- No proprietary code, data, DBC files, captures or configurations are included. All CAN
  IDs, message and signal names and payloads are invented and generated by the simulator;
  `demo_vehicle.dbc` is an invented example written for this project (the DBC text format
  itself is a public, widely documented format). The DBC parser is original code. The icons
  are original.
