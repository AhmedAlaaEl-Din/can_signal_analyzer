# The `.canlog` file format (version 1.0)

`.canlog` is the native log format of CAN Analyzer. Design goals:

- **Compact**: 14 bytes for a classic CAN frame with 8 data bytes, 13.2 bytes on average
  for the demo traffic (12.6 MiB for 1,000,000 frames).
- **Streaming and crash-safe**: the writer appends self-contained blocks. If
  the writer stops abruptly (crash, power loss), every complete block can
  still be read, and the reader reports how many bytes it ignored at the end.
- **Instant seeking in large files**: the block headers form the index.
  Opening a file reads only the headers, not the frames.
- **Versioned and self-describing**: a header with a major/minor version, the
  start time, and JSON metadata about the channels.

All integers are **little endian**. Timestamps are in **microseconds**.

## Layout

```
file   = header, block*
header = fixed part (28 bytes), metadata JSON (UTF-8), padding up to header length
block  = block header (32 bytes), payload
```

### Header

| Offset | Size | Field | Notes |
|---:|---:|---|---|
| 0 | 8 | magic | `43 41 4E 4C 4F 47 0D 0A` = `"CANLOG\r\n"`. The CR LF detects files damaged by text-mode transfers. |
| 8 | 2 | version major | `1`. A reader rejects other major versions. |
| 10 | 2 | version minor | `0`. Readers accept higher minor versions (additions only). |
| 12 | 4 | header length | Offset of the first block. Readers skip unknown header bytes. |
| 16 | 8 | start time | Wall-clock start of the recording, milliseconds since 1970-01-01 UTC (signed). `0` = unknown. |
| 24 | 4 | metadata length | Size of the JSON that follows. |
| 28 | n | metadata | UTF-8 JSON object, see below. |

Metadata example:

```json
{
  "application": "CAN Analyzer 1.0.0",
  "source": "Demo vehicle · seed 0xC0FFEE",
  "channels": [ { "number": 1, "name": "CAN 1 (simulated)", "bitrate": 500000 } ]
}
```

Unknown keys must be ignored. The first channel's `bitrate` is used for the
bus-load figures of the log summary.

### Block

| Offset | Size | Field | Notes |
|---:|---:|---|---|
| 0 | 4 | magic | `43 42 4C 4B` = `"CBLK"` |
| 4 | 4 | payload length P | Bytes after this 32-byte header (at most 64 MiB). |
| 8 | 4 | frame count N | Frames in the payload (the writer uses at most 4096). |
| 12 | 8 | first timestamp | Timestamp of the first frame, µs since the start of the log. |
| 20 | 8 | last timestamp | Timestamp of the last frame. |
| 28 | 4 | CRC-32 | CRC-32 (IEEE 802.3, the zlib polynomial) of the payload. |
| 32 | P | payload | N frame records. |

Rules:

- Timestamps never decrease, within a block and from one block to the next
  (`first timestamp` ≥ the previous block's `last timestamp`).
- The live recorder writes **one block every 250 ms** (and earlier when 4096
  frames are pending). A block is written with a single `write` call and
  flushed, so an abrupt stop leaves at most one incomplete block at the end.

### Frame record

| Size | Field | Notes |
|---:|---|---|
| 1–10 | Δt | Unsigned LEB128 varint: µs since the previous frame of the block (0 for the first frame). |
| 1 | flags | bit 0: extended (29-bit) identifier. Bits 1–7 are reserved and written as 0. |
| 1 | channel | 1-based bus channel. |
| 1 | DLC | 0..8 (classic CAN). |
| 2 or 4 | identifier | 2 bytes for standard (11-bit) frames, 4 bytes for extended frames. |
| DLC | data | The data bytes. |

The absolute timestamp of a frame is the block's `first timestamp` plus the
sum of the Δt values up to and including the frame. A typical 8-byte
standard frame at 1000 frames/s takes 1 + 1 + 1 + 1 + 2 + 8 = 14 bytes; in
the mixed demo traffic (some 2–4 byte frames) the average is 13.2 bytes per frame.

## Reading and seeking

1. Read and check the header.
2. Walk the block headers: from the header length, read 32 bytes, skip P
   bytes, repeat. Stop at the end of the file, at a block whose payload would
   end after the end of the file (incomplete last block: report "truncated"),
   or at a block header that is invalid (bad magic, timestamps going back,
   payload too large: report "damaged").
3. Each block header becomes one index entry (offset, first/last timestamp,
   frame count, index of its first frame). The index takes 40 bytes per block,
   so about 80 KB for a 1,000,000-frame log written at 2000 frames/s.
4. **Seek to a time t**: binary search for the first block whose last
   timestamp is ≥ t, decode that one block (verify its CRC) and binary search
   inside it. **Seek to frame k**: binary search on the first-frame indices.
5. A block whose CRC does not match is skipped and reported; the rest of the
   file stays readable.

CAN Analyzer keeps the decoded frames of only the two most recently used
blocks in memory, so memory use does not grow with the file size. See the
README for the measured numbers.

## Markers sidecar

Markers added during replay are stored next to the log as
`<log file name>.markers.json`:

```json
{
  "format": "can-analyzer-markers",
  "version": 1,
  "markers": [ { "timeUs": 4120000, "name": "Cruise 115 km/h" } ]
}
```

`timeUs` uses the timestamps of the log. The sidecar is written atomically
after every change. A missing sidecar means "no markers". A damaged sidecar
is reported and ignored, and the log itself is never modified.

## CSV frames

Frames can be exported to CSV (Replay bar › Export CSV, or
`can_logtool csv in.canlog out.csv`) and CSV files can be opened like logs
(they are converted to a temporary `.canlog`).

```
time_s,channel,id,format,dlc,data
0.000000,1,0C8,std,8,00 00 F4 FF 00 00 03 00
0.000412,1,92345678,ext,4,10 32 54 76
```

| Column | Format |
|---|---|
| `time_s` | Seconds since the start of the log, up to 6 decimals (exact µs, no floating-point rounding). |
| `channel` | 1-based channel number. |
| `id` | Hex identifier without prefix (`0x` is accepted on import), 3 digits for standard and 8 for extended frames on export. |
| `format` | `std` or `ext` (`0`/`1` are accepted on import). |
| `dlc` | 0..8. |
| `data` | Data bytes as hex separated by single spaces. The count must equal the DLC. |

Import rules: the header row is optional, and empty lines and lines starting
with `#` are ignored. Rows must be in time order. The first invalid row stops
the import with its line number and the reason (for example "line 12: id
800 is out of range for 11-bit frames").
