# CAN Analyzer @VERSION@ (Windows x64, portable)

A desktop CAN signal analyzer: live trace, message overview, DBC decoding,
signal list, multi-lane plots, logging to `.canlog`, replay with markers,
trace search and log summaries. The bus is **simulated** (an invented demo
vehicle); no CAN hardware is needed or supported.

## Run

1. Unzip the folder anywhere (no installation, no administrator rights).
2. Start `can_analyzer.exe`.
3. Click **Start with the demo**: the demo database is loaded, the simulator
   starts and the Plots tab opens with four lanes.

Nothing is written outside the folder you choose except the settings
(window size, theme, last database, plot layout) in the per-user registry
(`HKEY_CURRENT_USER\Software\CanAnalyzer`), and logs you record (default
folder `Documents\CAN Analyzer logs`).

## Try the sample log

`samples\demo_drive_60s.canlog` is a 60 s recording of the simulated demo
vehicle. Open it with **Open log** (Ctrl+Shift+O) or drag it onto the window.
Load `samples\demo_vehicle.dbc` (or click **Load demo database**) to decode
it. Then:

- play, pause (Space), change the speed (0.25x to 8x, or Max), loop;
- drag the seek bar, step one frame (period key), jump to start/end;
- add markers (M), jump between them ([ and ]); they are saved next to the log;
- search the trace (Ctrl+F in the Trace tab): `VehicleSpeed > 100`, `12 ?? FF`,
  `0C8`, then F3 / Shift+F3;
- open **Summary** for per-ID statistics and bus load, and export it as CSV.

## Command line

```
can_analyzer.exe --help
can_logtool.exe generate --out drive.canlog --seconds 120
can_logtool.exe info samples\demo_drive_60s.canlog
can_logtool.exe csv samples\demo_drive_60s.canlog frames.csv
```

## Licenses

CAN Analyzer: MIT (`LICENSE.txt`). Qt is used under the LGPLv3 and linked
dynamically; see `THIRD_PARTY\Qt\NOTICE.md` for the license texts, the Qt
source location and how to replace the Qt libraries with your own build.
