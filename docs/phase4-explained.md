# Phase 4 explained (plain English)

Notes for explaining logging, replay and the release in an interview.

## What changed

Up to phase 3 everything was live: frames came from the simulator, were shown, and were
gone once the buffer filled up. Phase 4 adds memory to the tool. You can **record** the
bus to a file, **replay** that file later through exactly the same views, **search** the
trace, put **markers** on interesting moments and get a **summary** of a whole log. It also
turns the project into a **release**: one zip that runs on any Windows PC.

## The log format design

The file is a header followed by **blocks**.

- **Header**: a magic word ("CANLOG" + CR LF, so a file damaged by a text-mode copy is
  detected), a version number (major.minor), the wall-clock start time and a small JSON
  block that describes the channels (name, bit rate) and the program that wrote it.
  Readers reject an unknown major version and accept newer minor versions, so the format
  can grow without breaking old files.
- **Blocks**: the recorder collects frames for 250 ms and writes them as one block. Each
  block starts with a fixed 32-byte header: the number of frames, the first and last
  timestamp, the payload length and a checksum (CRC-32). Then come the frames.
- **Frames are small**: each stores the time since the previous frame as a variable-length
  number (usually 1–2 bytes instead of 8), a flags byte, the channel, the length, the ID in
  2 bytes (4 for extended IDs) and the data. That is 14 bytes for a full 8-byte frame and
  13.2 bytes on average for the demo traffic.

Why blocks? They solve three problems at once:

1. **Crash safety.** A block is written in one go and flushed. If the program dies, every
   complete block is still valid. The reader stops at the first incomplete block and
   reports how many bytes it ignored. At most 250 ms of data is lost.
2. **Damage detection.** The checksum tells whether a block was corrupted. A bad block is
   skipped and reported; the rest of the file stays usable.
3. **An index for free.** The block headers already say which time range each block
   covers. That is all a seek needs.

## How seeking works with the index

Opening a file does not read the frames. It hops from block header to block header
(read 32 bytes, skip the payload, repeat) and keeps one small entry per block: where it
is in the file, its first and last timestamp, how many frames it has and the number of
its first frame. A 1,000,000-frame log has 2000 blocks, so the whole index is about
94 KB, built in 13 ms.

To jump to time *t*:

1. **Binary search** the index for the first block whose last timestamp is at or after
   *t* (about 11 comparisons for 2000 blocks).
2. Read and decode **that one block** (a few hundred frames, checksum verified).
3. Binary search **inside** the block for the first frame at or after *t*.

That costs about 30 µs on average, independent of the file size. Memory stays flat
because only the two most recently decoded blocks are kept: the benchmark peaked at 11 MB
of process memory while writing, indexing, reading and exporting a million frames.

After a seek the views are cleared, and the 10 seconds before the target are delivered at
once (the **pre-roll**). The plots and the trace then show context immediately instead of
starting empty.

## How replay timing works

Replay is just another frame source, so the trace, decoder, plots and statistics do not
know whether frames come from the simulator or from a file.

Timing is a simple loop that runs every 10 ms:

```
position += (clock now − clock at the previous tick) × speed
deliver every frame with timestamp ≤ position
```

- **Speed** changes take effect immediately without a jump, because only the *increment*
  is scaled.
- **Pause** stops the loop. On resume, the clock reference is reset, so the paused time
  is not counted.
- **As fast as possible** ignores the clock and delivers a fixed 4000 frames per tick, so
  the views keep up and the window stays responsive.
- **Step** delivers exactly the next frame (and any frame with the same timestamp).
- **Loop** clears the views at the end and starts again from the first frame.

The clock is a plug-in function. In tests it is a fake clock that the test advances by
hand, which proves the timing exactly: after 1 s of fake time at 0.25× the position is
exactly 0.25 s, and 250 frames 1 ms apart have been delivered. The deterministic demo uses
the same trick with its frame clock.

One detail: at 8× the status bar would show 8 times the real bus load. Rate and load are
divided by the replay speed, so they describe the recorded bus, not the playback.

## Search, markers and the summary

- **Search** guesses what you typed. `1A4` is an ID, `EngineData` is a message,
  `12 ?? FF` is a byte pattern from byte 0 (`??` means any byte), and
  `VehicleSpeed > 100` is a condition on a decoded value. A chip shows how the text was
  understood, so there is no hidden mode. Equality on decoded values allows half a
  quantisation step, because 88.8 km/h is stored as 8880 × 0.01.
- **Markers** are stored next to the log in a small JSON file, so the log itself is never
  modified. They appear on the seek bar and as lines in the plots.
- **The summary** reads the whole log once, in the background: per-ID count, rate, period
  (mean, min, max), length and bus load, plus the peak load over any one-second window.

## The release

- The **version** is written once in the top-level CMake file. CMake generates the C++
  header, the Windows version resource (Explorer › Properties › Details) and the zip name
  from it.
- The **icon** is an SVG drawn for the project. A small build-time tool renders it into a
  Windows `.ico`.
- The **package target** copies the app, lets Qt's deployment tool add the Qt DLLs, plugins
  and QML modules, adds the Visual C++ runtime and removes debug files. It generates a 60 s
  sample log with the simulator and adds the README, the MIT license and the Qt LGPLv3
  texts with instructions for replacing Qt (the app links Qt dynamically, which is what
  makes that possible).
- The zip was checked in a clean folder with **no Qt on the PATH**: a built-in self-test
  loads the demo database, replays the sample log and exits successfully.
