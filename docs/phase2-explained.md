# Phase 2 explained (plain English)

Notes for explaining the signal database and decoding in an interview.

## What changed

In phase 1 the app showed raw bytes: `0C8  8  1D 17 7C 69 …`. That is what is on the wire,
but nobody can read "vehicle speed" out of it. A **signal database** (a DBC file) is the
dictionary that says which bits of which message mean what. Phase 2 reads such a file and
turns every frame into named values with units: `VehicleSpeed = 46.60 km/h`.

## How a signal is extracted from the bytes

A classic CAN frame carries up to 8 bytes = 64 bits. The DBC describes each signal with:

| Field | Meaning | Example |
|---|---|---|
| start bit | where the signal begins | `16` |
| length | how many bits | `16` |
| byte order | Intel (little endian) or Motorola (big endian) | `@1` = Intel |
| signedness | unsigned or two's complement | `-` = signed |
| factor, offset | how to turn the raw number into a real value | `(0.1, 0)` |
| min, max, unit | expected range and unit | `[-780|780] "deg"` |

**Bit numbering.** Bit *n* is bit `n % 8` of byte `n / 8`, where bit 0 is the least
significant bit of a byte. So bit 16 is the lowest bit of byte 2.

**Intel (little endian).** The start bit is the *least* significant bit of the signal and the
signal grows upward: bits 16, 17, … 31. Byte 2 holds the low half, byte 3 the high half. This
is how a PC stores numbers in memory.

**Motorola (big endian).** The start bit is the *most* significant bit and the signal grows
"downward" inside a byte, then jumps to the top bit of the *next* byte: 39, 38, …, 32, then
47, 46, 45, 44. People call this the "sawtooth" layout. The bytes are in reading order, most
significant first.

The app computes this list of occupied bits in exactly one function
(`codec::occupiedBits`). Reading a value, writing a value (in the simulator) and drawing the
bit layout all use that same list, so they can never disagree.

**From bits to a number.** Walk the occupied bits from MSB to LSB and shift them into an
integer. That is the **raw value**. If the signal is signed and its top bit is 1, the raw value
is negative (two's complement), e.g. 12 bits `0xABC` → −1348.

**From raw to physical.** `physical = raw × factor + offset`.
Example: raw −1348 with factor 0.5 → −674 Nm. Coolant temperature raw 80 with offset −40 →
40 °C. If the value is outside `[min, max]` the app marks it as **out of range** (red).
Enumerations come from `VAL_` tables: raw 2 of `EngineState` → "Running".

The tests check these steps with hand-computed vectors (Intel and Motorola, signed values,
scaling and offset, signals that cross byte boundaries, single bits, 64-bit values,
out-of-range values) and with 5000 random layouts that are written and read back.

## Why the parser is separate from the UI

- **Testability.** The parser and decoder are plain C++ (Qt Core only). The tests feed them
  text and bytes and check the results; no window is involved. A broken DBC file is a test
  case with an expected error message and line number.
- **One source of truth.** The parsed database is immutable and shared (a
  `std::shared_ptr<const Database>`). The trace, the messages view, the signal list, the
  simulator and the tests all read the same object, and nobody can modify it by accident.
- **Safety.** A file is either loaded completely or not at all. If anything is wrong the user
  gets a readable list of problems (`Line 5: SG_ Mode: start bit 70 is out of range`) and the
  previous database stays active; a half-parsed database never reaches the UI.
- **Reuse.** Later phases (logging, replay, plots) need the same decoding without the UI.

## How the sparklines are fed

1. Frames arrive in batches every 30 ms. For each frame whose ID is in the database, the
   `SignalListModel` decodes its signals and updates each signal's current value and min/max.
2. Each signal keeps a small **history ring**: one entry per 40 ms "bucket" with the newest
   value plus the min and max seen in that bucket, for about the last 12 seconds (300
   entries). Memory is fixed, whatever the bus rate.
3. The sparkline is a small C++ item. QML only gives it the model, the signal id and a
   *revision* number. When the revision changes, the item reads the history directly from the
   model (no copying through QML) and draws the last 10 seconds with antialiasing.
4. Buckets whose min..max range is wide compared to the visible range (for example an alive
   counter that wraps 0 → 15 several times per bucket) are drawn as a soft **band** instead
   of a jagged line, because a single sample per bucket cannot show such fast changes
   honestly.

## The demo vehicle

To make decoded values look real, the simulator has a "Demo vehicle" mode. A small physical
model runs a drive cycle (idle, accelerate, cruise, brake), computes engine speed from road
speed and the engaged gear, warms the coolant slowly, sweeps the steering wheel and so on.
For each message due, it encodes these physical values into bytes **with the same codec the
decoder uses**. A round-trip test generates frames, decodes them and checks that every value
matches what the model intended, within half a scaling step.

## Deterministic demo capture

The demo frames are rendered with a **virtual clock**: for each frame the simulator, the
scripted scene and every animation advance by exactly 1/12 s, then the window is rendered to
an image. That is why the 12 fps frame sequence plays back smoothly even if a frame took
longer than 83 ms to produce.
