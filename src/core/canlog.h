#pragma once

#include "canframe.h"

#include <QByteArray>
#include <QDateTime>
#include <QFile>
#include <QMetaType>
#include <QString>

#include <cstdint>
#include <memory>
#include <vector>

namespace can {

// Native log format ".canlog" (see docs/log-format.md).
//
//   file   = header, block*
//   header = magic "CANLOG\r\n", version major/minor, header length, start
//            time (ms since the Unix epoch, UTC), metadata JSON (channels,
//            application, source)
//   block  = 32-byte block header (magic "CBLK", payload length, frame count,
//            first/last timestamp, CRC-32 of the payload), then the payload:
//            frames with timestamps relative to the previous frame (varint).
//
// Blocks are self-contained and written in one piece, so a file whose writer
// stopped abruptly is readable up to the last complete block. The block
// headers double as the seek index: opening a file reads only the headers.
namespace canlog {

inline constexpr char kMagic[8] = {'C', 'A', 'N', 'L', 'O', 'G', '\r', '\n'};
inline constexpr std::uint32_t kBlockMagic = 0x4B4C4243; // "CBLK" little endian
inline constexpr std::uint16_t kVersionMajor = 1;
inline constexpr std::uint16_t kVersionMinor = 0;
inline constexpr int kFixedHeaderSize = 28;   // before the metadata JSON
inline constexpr int kBlockHeaderSize = 32;
inline constexpr int kMaxBlockFrames = 4096;
inline constexpr std::uint32_t kMaxPayload = 64u * 1024u * 1024u; // sanity limit

std::uint32_t crc32(const char *data, std::size_t size, std::uint32_t crc = 0);

struct Channel
{
    int number = 1;
    QString name;
    int bitrate = 500000;
};

struct Header
{
    std::uint16_t versionMajor = kVersionMajor;
    std::uint16_t versionMinor = kVersionMinor;
    QDateTime startTime;          // UTC; invalid = unknown
    QString application;          // e.g. "CAN Analyzer 1.0.0"
    QString source;               // e.g. "Demo vehicle (simulated)"
    std::vector<Channel> channels{Channel{1, QStringLiteral("CAN 1"), 500000}};

    int bitrate() const { return channels.empty() ? 500000 : channels.front().bitrate; }
};

// Encodes frames into blocks. Not thread safe; LogRecorder runs it on a worker.
class Writer
{
public:
    Writer() = default;
    ~Writer();
    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    bool open(const QString &path, const Header &header, QString *error = nullptr);
    bool isOpen() const { return m_file.isOpen(); }
    // Frames must be in timestamp order (earlier timestamps are clamped).
    void append(const CanFrame &frame);
    void append(const CanFrame *frames, std::size_t count);
    // Writes the pending block (if any) and flushes the file to the OS.
    bool flush();
    bool close();

    std::uint64_t framesWritten() const { return m_framesWritten + m_pendingFrames; }
    std::uint64_t bytesWritten() const { return m_bytesWritten; }
    std::uint64_t blocksWritten() const { return m_blocks; }
    std::int64_t firstTimestampUs() const { return m_firstTs; }
    std::int64_t lastTimestampUs() const { return m_lastTs; }
    QString errorString() const { return m_error; }
    QString path() const { return m_file.fileName(); }

private:
    bool writeBlock();

    QFile m_file;
    QByteArray m_payload;
    std::uint32_t m_pendingFrames = 0;
    std::int64_t m_blockFirstTs = 0;
    std::int64_t m_prevTs = 0;
    std::int64_t m_firstTs = -1;
    std::int64_t m_lastTs = -1;
    std::uint64_t m_framesWritten = 0;
    std::uint64_t m_bytesWritten = 0;
    std::uint64_t m_blocks = 0;
    QString m_error;
};

struct BlockInfo
{
    qint64 offset = 0;        // file offset of the block header
    std::uint32_t payloadSize = 0;
    std::uint32_t frameCount = 0;
    std::uint32_t crc = 0;
    std::int64_t firstUs = 0;
    std::int64_t lastUs = 0;
    std::int64_t firstFrame = 0; // global index of the block's first frame
};

// Random-access reader. open() reads the header and every block header (not
// the payloads), so it is fast for any file size and its memory use is
// proportional to the number of blocks (40 bytes per up to 4096 frames).
// Decoded payloads are cached for the two most recently used blocks only.
class Reader
{
public:
    Reader() = default;
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;

    bool open(const QString &path, QString *error = nullptr);
    void close();
    bool isOpen() const { return m_file.isOpen(); }
    QString path() const { return m_file.fileName(); }

    const Header &header() const { return m_header; }
    std::int64_t frameCount() const { return m_frameCount; }
    std::int64_t firstTimestampUs() const { return m_blocks.empty() ? 0 : m_blocks.front().firstUs; }
    std::int64_t lastTimestampUs() const { return m_blocks.empty() ? 0 : m_blocks.back().lastUs; }
    std::int64_t durationUs() const { return lastTimestampUs() - firstTimestampUs(); }
    const std::vector<BlockInfo> &blocks() const { return m_blocks; }
    qint64 fileSize() const { return m_fileSize; }
    // Bytes at the end that do not form a complete block (abrupt stop).
    qint64 truncatedBytes() const { return m_truncatedBytes; }
    QString warning() const { return m_warning; }
    QString errorString() const { return m_error; }

    // Index of the first frame with timestamp >= timeUs (frameCount() if none).
    std::int64_t indexAtTime(std::int64_t timeUs);
    bool frameAt(std::int64_t index, CanFrame &out);
    // Appends up to maxCount frames starting at `index`; returns the number read.
    std::size_t read(std::int64_t index, std::size_t maxCount, std::vector<CanFrame> &out);
    // Decoded frames of one block (cached). Empty on a corrupt block.
    const std::vector<CanFrame> &blockFrames(std::size_t block);
    std::size_t blockOfFrame(std::int64_t index) const;

    std::size_t cachedBlocks() const;

private:
    bool decodeBlock(const BlockInfo &info, std::vector<CanFrame> &out);

    QFile m_file;
    Header m_header;
    std::vector<BlockInfo> m_blocks;
    std::int64_t m_frameCount = 0;
    qint64 m_fileSize = 0;
    qint64 m_truncatedBytes = 0;
    QString m_warning;
    QString m_error;
    struct Cache
    {
        std::size_t block = SIZE_MAX;
        std::vector<CanFrame> frames;
        std::uint64_t used = 0;
    };
    Cache m_cache[2];
    std::uint64_t m_useCounter = 0;
    std::vector<CanFrame> m_empty;
};

// ---- CSV (documented in docs/log-format.md) --------------------------------
// Columns: time_s,channel,id,format,dlc,data
//   time_s  seconds since the start of the log, 6 decimals
//   channel 1-based bus channel
//   id      hex without prefix (3 digits standard, 8 extended)
//   format  "std" or "ext"
//   dlc     0..8
//   data    bytes as upper-case hex separated by single spaces
QByteArray csvHeaderLine();
QByteArray toCsvLine(const CanFrame &frame);
// Parses one data line. Returns false and sets error on invalid input.
bool parseCsvLine(QByteArrayView line, CanFrame &out, QString *error);

struct ConvertResult
{
    bool ok = false;
    std::int64_t frames = 0;
    QString error;
};
// Whole-file conversions (streaming; memory independent of the file size).
ConvertResult exportCsv(const QString &canlogPath, const QString &csvPath);
// CSV rows must be in time order; out-of-order rows are reported as an error
// with their line number.
ConvertResult importCsv(const QString &csvPath, const QString &canlogPath, const Header &header);

} // namespace canlog
} // namespace can

Q_DECLARE_METATYPE(can::canlog::Header)
