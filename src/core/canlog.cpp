#include "canlog.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimeZone>

#include <algorithm>
#include <array>
#include <cstring>

namespace can::canlog {

namespace {

const std::array<std::uint32_t, 256> &crcTable()
{
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> t{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    return table;
}

void putU16(QByteArray &b, std::uint16_t v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>(v >> 8));
}
void putU32(QByteArray &b, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i)
        b.append(static_cast<char>((v >> (8 * i)) & 0xFF));
}
void putI64(QByteArray &b, std::int64_t v)
{
    const auto u = static_cast<std::uint64_t>(v);
    for (int i = 0; i < 8; ++i)
        b.append(static_cast<char>((u >> (8 * i)) & 0xFF));
}
void putVarint(QByteArray &b, std::uint64_t v)
{
    while (v >= 0x80) {
        b.append(static_cast<char>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    b.append(static_cast<char>(v));
}

std::uint16_t getU16(const char *p)
{
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    return static_cast<std::uint16_t>(u[0] | (u[1] << 8));
}
std::uint32_t getU32(const char *p)
{
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    return static_cast<std::uint32_t>(u[0]) | (static_cast<std::uint32_t>(u[1]) << 8)
           | (static_cast<std::uint32_t>(u[2]) << 16) | (static_cast<std::uint32_t>(u[3]) << 24);
}
std::int64_t getI64(const char *p)
{
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    std::uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | u[i];
    return static_cast<std::int64_t>(v);
}

QByteArray headerJson(const Header &h)
{
    QJsonArray channels;
    for (const auto &c : h.channels)
        channels.append(QJsonObject{{QStringLiteral("number"), c.number},
                                    {QStringLiteral("name"), c.name},
                                    {QStringLiteral("bitrate"), c.bitrate}});
    QJsonObject o{{QStringLiteral("application"), h.application},
                  {QStringLiteral("source"), h.source},
                  {QStringLiteral("channels"), channels}};
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QString tr(const char *text)
{
    return QString::fromUtf8(text);
}

} // namespace

std::uint32_t crc32(const char *data, std::size_t size, std::uint32_t crc)
{
    const auto &t = crcTable();
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i)
        crc = t[(crc ^ static_cast<unsigned char>(data[i])) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

// =========================================================================
// Writer

Writer::~Writer()
{
    close();
}

bool Writer::open(const QString &path, const Header &header, QString *error)
{
    close();
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = tr("Could not create the log file: %1").arg(m_file.errorString());
        if (error)
            *error = m_error;
        return false;
    }
    const QByteArray json = headerJson(header);
    QByteArray h;
    h.append(kMagic, sizeof kMagic);
    putU16(h, kVersionMajor);
    putU16(h, kVersionMinor);
    putU32(h, static_cast<std::uint32_t>(kFixedHeaderSize + json.size()));
    putI64(h, header.startTime.isValid() ? header.startTime.toMSecsSinceEpoch() : 0);
    putU32(h, static_cast<std::uint32_t>(json.size()));
    h.append(json);
    if (m_file.write(h) != h.size() || !m_file.flush()) {
        m_error = tr("Could not write to the log file: %1").arg(m_file.errorString());
        if (error)
            *error = m_error;
        m_file.close();
        return false;
    }
    m_bytesWritten = static_cast<std::uint64_t>(h.size());
    m_payload.clear();
    m_payload.reserve(kMaxBlockFrames * 16);
    m_pendingFrames = 0;
    m_framesWritten = 0;
    m_blocks = 0;
    m_firstTs = m_lastTs = -1;
    m_prevTs = 0;
    m_error.clear();
    return true;
}

void Writer::append(const CanFrame &frame)
{
    if (!m_file.isOpen())
        return;
    std::int64_t ts = frame.timestampUs;
    if (m_lastTs >= 0 && ts < m_lastTs)
        ts = m_lastTs; // the format requires non-decreasing timestamps
    if (m_firstTs < 0)
        m_firstTs = ts;
    if (m_pendingFrames == 0) {
        m_blockFirstTs = ts;
        m_prevTs = ts;
    }
    putVarint(m_payload, static_cast<std::uint64_t>(ts - m_prevTs));
    m_prevTs = ts;
    m_lastTs = ts;
    const int dlc = std::min<int>(frame.dlc, CanFrame::kMaxDlc);
    m_payload.append(static_cast<char>(frame.extended ? 1 : 0));
    m_payload.append(static_cast<char>(frame.channel));
    m_payload.append(static_cast<char>(dlc));
    if (frame.extended)
        putU32(m_payload, frame.id & CanFrame::kMaxExtendedId);
    else
        putU16(m_payload, static_cast<std::uint16_t>(frame.id & CanFrame::kMaxStandardId));
    m_payload.append(reinterpret_cast<const char *>(frame.data.data()), dlc);
    if (++m_pendingFrames >= static_cast<std::uint32_t>(kMaxBlockFrames))
        writeBlock();
}

void Writer::append(const CanFrame *frames, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i)
        append(frames[i]);
}

bool Writer::writeBlock()
{
    if (m_pendingFrames == 0)
        return true;
    QByteArray block;
    block.reserve(kBlockHeaderSize + m_payload.size());
    putU32(block, kBlockMagic);
    putU32(block, static_cast<std::uint32_t>(m_payload.size()));
    putU32(block, m_pendingFrames);
    putI64(block, m_blockFirstTs);
    putI64(block, m_lastTs);
    putU32(block, crc32(m_payload.constData(), static_cast<std::size_t>(m_payload.size())));
    block.append(m_payload);
    // One write call per block: a crash leaves at most one partial block.
    const bool ok = m_file.write(block) == block.size();
    if (!ok)
        m_error = tr("Writing the log failed: %1").arg(m_file.errorString());
    m_bytesWritten += static_cast<std::uint64_t>(block.size());
    m_framesWritten += m_pendingFrames;
    ++m_blocks;
    m_pendingFrames = 0;
    m_payload.clear();
    return ok;
}

bool Writer::flush()
{
    if (!m_file.isOpen())
        return false;
    const bool ok = writeBlock();
    return m_file.flush() && ok;
}

bool Writer::close()
{
    if (!m_file.isOpen())
        return true;
    const bool ok = flush();
    m_file.close();
    return ok;
}

// =========================================================================
// Reader

bool Reader::open(const QString &path, QString *error)
{
    close();
    auto fail = [&](const QString &message) {
        m_error = message;
        if (error)
            *error = message;
        m_file.close();
        m_blocks.clear();
        m_frameCount = 0;
        return false;
    };
    m_file.setFileName(path);
    if (!m_file.open(QIODevice::ReadOnly))
        return fail(tr("Could not open %1: %2").arg(QFileInfo(path).fileName(), m_file.errorString()));
    m_fileSize = m_file.size();

    const QByteArray fixed = m_file.read(kFixedHeaderSize);
    if (fixed.size() < kFixedHeaderSize || std::memcmp(fixed.constData(), kMagic, sizeof kMagic) != 0)
        return fail(tr("%1 is not a CAN Analyzer log (.canlog).").arg(QFileInfo(path).fileName()));
    m_header = Header{};
    m_header.versionMajor = getU16(fixed.constData() + 8);
    m_header.versionMinor = getU16(fixed.constData() + 10);
    if (m_header.versionMajor != kVersionMajor)
        return fail(tr("Log format version %1.%2 is not supported (this version reads %3.x).")
                        .arg(m_header.versionMajor).arg(m_header.versionMinor).arg(kVersionMajor));
    const std::uint32_t headerLength = getU32(fixed.constData() + 12);
    const std::int64_t startMs = getI64(fixed.constData() + 16);
    const std::uint32_t jsonLength = getU32(fixed.constData() + 24);
    if (headerLength < kFixedHeaderSize + jsonLength || headerLength > m_fileSize)
        return fail(tr("The log header is damaged."));
    const QByteArray json = m_file.read(jsonLength);
    if (startMs != 0)
        m_header.startTime = QDateTime::fromMSecsSinceEpoch(startMs, QTimeZone::UTC);
    const QJsonObject meta = QJsonDocument::fromJson(json).object();
    m_header.application = meta.value(QStringLiteral("application")).toString();
    m_header.source = meta.value(QStringLiteral("source")).toString();
    m_header.channels.clear();
    for (const auto &v : meta.value(QStringLiteral("channels")).toArray()) {
        const QJsonObject c = v.toObject();
        m_header.channels.push_back(Channel{c.value(QStringLiteral("number")).toInt(1),
                                            c.value(QStringLiteral("name")).toString(),
                                            c.value(QStringLiteral("bitrate")).toInt(500000)});
    }
    if (m_header.channels.empty())
        m_header.channels.push_back(Channel{1, QStringLiteral("CAN 1"), 500000});

    // Index: walk the block headers only.
    qint64 offset = headerLength;
    std::int64_t frames = 0;
    m_blocks.reserve(static_cast<std::size_t>(std::max<qint64>(16, m_fileSize / (kMaxBlockFrames * 8))));
    while (offset + kBlockHeaderSize <= m_fileSize) {
        if (!m_file.seek(offset))
            break;
        char bh[kBlockHeaderSize];
        if (m_file.read(bh, kBlockHeaderSize) != kBlockHeaderSize)
            break;
        BlockInfo b;
        b.offset = offset;
        if (getU32(bh) != kBlockMagic) {
            m_warning = tr("Damaged data at byte %1; the rest of the file was ignored.").arg(offset);
            break;
        }
        b.payloadSize = getU32(bh + 4);
        b.frameCount = getU32(bh + 8);
        b.firstUs = getI64(bh + 12);
        b.lastUs = getI64(bh + 20);
        b.crc = getU32(bh + 28);
        if (b.payloadSize > kMaxPayload || b.lastUs < b.firstUs
            || (!m_blocks.empty() && b.firstUs < m_blocks.back().lastUs)) {
            m_warning = tr("Damaged data at byte %1; the rest of the file was ignored.").arg(offset);
            break;
        }
        if (offset + kBlockHeaderSize + b.payloadSize > m_fileSize)
            break; // incomplete last block (writer stopped abruptly)
        b.firstFrame = frames;
        frames += b.frameCount;
        m_blocks.push_back(b);
        offset += kBlockHeaderSize + b.payloadSize;
    }
    m_truncatedBytes = m_fileSize - offset;
    if (m_truncatedBytes > 0 && m_warning.isEmpty())
        m_warning = tr("The log ends with an incomplete block (%1 bytes ignored); the recording "
                       "was probably interrupted.").arg(m_truncatedBytes);
    m_frameCount = frames;
    m_blocks.shrink_to_fit();
    return true;
}

void Reader::close()
{
    m_file.close();
    m_blocks.clear();
    m_frameCount = 0;
    m_fileSize = 0;
    m_truncatedBytes = 0;
    m_warning.clear();
    m_error.clear();
    for (auto &c : m_cache)
        c = Cache{};
}

bool Reader::decodeBlock(const BlockInfo &info, std::vector<CanFrame> &out)
{
    out.clear();
    if (!m_file.seek(info.offset + kBlockHeaderSize))
        return false;
    const QByteArray payload = m_file.read(info.payloadSize);
    if (payload.size() != static_cast<qsizetype>(info.payloadSize)
        || crc32(payload.constData(), static_cast<std::size_t>(payload.size())) != info.crc) {
        m_warning = tr("A block at byte %1 is damaged (checksum mismatch) and was skipped.").arg(info.offset);
        return false;
    }
    out.reserve(info.frameCount);
    const auto *p = reinterpret_cast<const unsigned char *>(payload.constData());
    const auto *end = p + payload.size();
    std::int64_t ts = info.firstUs;
    for (std::uint32_t i = 0; i < info.frameCount; ++i) {
        std::uint64_t delta = 0;
        int shift = 0;
        while (p < end) {
            const unsigned char c = *p++;
            delta |= static_cast<std::uint64_t>(c & 0x7F) << shift;
            shift += 7;
            if (!(c & 0x80))
                break;
            if (shift > 63)
                return false;
        }
        if (end - p < 3)
            return false;
        CanFrame f;
        ts += static_cast<std::int64_t>(delta);
        f.timestampUs = ts;
        const unsigned char flags = *p++;
        f.extended = (flags & 1u) != 0;
        f.channel = *p++;
        f.dlc = static_cast<std::uint8_t>(std::min<int>(*p++, CanFrame::kMaxDlc));
        const int idBytes = f.extended ? 4 : 2;
        if (end - p < idBytes + f.dlc)
            return false;
        f.id = f.extended ? getU32(reinterpret_cast<const char *>(p)) : getU16(reinterpret_cast<const char *>(p));
        p += idBytes;
        std::memcpy(f.data.data(), p, f.dlc);
        p += f.dlc;
        out.push_back(f);
    }
    return true;
}

const std::vector<CanFrame> &Reader::blockFrames(std::size_t block)
{
    if (block >= m_blocks.size())
        return m_empty;
    for (auto &c : m_cache) {
        if (c.block == block) {
            c.used = ++m_useCounter;
            return c.frames;
        }
    }
    Cache &victim = m_cache[0].used <= m_cache[1].used ? m_cache[0] : m_cache[1];
    victim.block = block;
    victim.used = ++m_useCounter;
    if (!decodeBlock(m_blocks[block], victim.frames))
        victim.frames.clear();
    return victim.frames;
}

std::size_t Reader::cachedBlocks() const
{
    return (m_cache[0].block != SIZE_MAX ? 1u : 0u) + (m_cache[1].block != SIZE_MAX ? 1u : 0u);
}

std::size_t Reader::blockOfFrame(std::int64_t index) const
{
    auto it = std::upper_bound(m_blocks.begin(), m_blocks.end(), index,
                               [](std::int64_t i, const BlockInfo &b) { return i < b.firstFrame; });
    return it == m_blocks.begin() ? 0 : static_cast<std::size_t>(it - m_blocks.begin() - 1);
}

std::int64_t Reader::indexAtTime(std::int64_t timeUs)
{
    // First block whose last timestamp reaches timeUs.
    auto it = std::lower_bound(m_blocks.begin(), m_blocks.end(), timeUs,
                               [](const BlockInfo &b, std::int64_t t) { return b.lastUs < t; });
    if (it == m_blocks.end())
        return m_frameCount;
    const std::size_t block = static_cast<std::size_t>(it - m_blocks.begin());
    const auto &frames = blockFrames(block);
    auto f = std::lower_bound(frames.begin(), frames.end(), timeUs,
                              [](const CanFrame &fr, std::int64_t t) { return fr.timestampUs < t; });
    return it->firstFrame + (f - frames.begin());
}

bool Reader::frameAt(std::int64_t index, CanFrame &out)
{
    if (index < 0 || index >= m_frameCount)
        return false;
    const std::size_t block = blockOfFrame(index);
    const auto &frames = blockFrames(block);
    const auto local = static_cast<std::size_t>(index - m_blocks[block].firstFrame);
    if (local >= frames.size())
        return false;
    out = frames[local];
    return true;
}

std::size_t Reader::read(std::int64_t index, std::size_t maxCount, std::vector<CanFrame> &out)
{
    std::size_t n = 0;
    while (n < maxCount && index < m_frameCount && index >= 0) {
        const std::size_t block = blockOfFrame(index);
        const auto &frames = blockFrames(block);
        auto local = static_cast<std::size_t>(index - m_blocks[block].firstFrame);
        if (frames.empty()) { // damaged block: skip it
            index = m_blocks[block].firstFrame + m_blocks[block].frameCount;
            continue;
        }
        const std::size_t take = std::min(maxCount - n, frames.size() - local);
        out.insert(out.end(), frames.begin() + static_cast<std::ptrdiff_t>(local),
                   frames.begin() + static_cast<std::ptrdiff_t>(local + take));
        n += take;
        index += static_cast<std::int64_t>(take);
    }
    return n;
}

// =========================================================================
// CSV

QByteArray csvHeaderLine()
{
    return QByteArrayLiteral("time_s,channel,id,format,dlc,data\n");
}

namespace {

// Formats one CSV line into buf (no allocations); returns its length.
int formatCsvLine(const CanFrame &f, char *buf)
{
    static const char hex[] = "0123456789ABCDEF";
    char *p = buf;
    std::int64_t us = f.timestampUs;
    if (us < 0) {
        *p++ = '-';
        us = -us;
    }
    char tmp[24];
    int n = 0;
    std::int64_t sec = us / 1000000;
    do {
        tmp[n++] = static_cast<char>('0' + sec % 10);
        sec /= 10;
    } while (sec > 0);
    while (n > 0)
        *p++ = tmp[--n];
    *p++ = '.';
    std::int64_t frac = us % 1000000;
    for (int i = 5; i >= 0; --i) {
        p[i] = static_cast<char>('0' + frac % 10);
        frac /= 10;
    }
    p += 6;
    *p++ = ',';
    int ch = f.channel;
    if (ch >= 100)
        *p++ = static_cast<char>('0' + ch / 100);
    if (ch >= 10)
        *p++ = static_cast<char>('0' + (ch / 10) % 10);
    *p++ = static_cast<char>('0' + ch % 10);
    *p++ = ',';
    const int digits = f.extended ? 8 : 3;
    for (int i = digits - 1; i >= 0; --i)
        *p++ = hex[(f.id >> (4 * i)) & 0xF];
    std::memcpy(p, f.extended ? ",ext," : ",std,", 5);
    p += 5;
    *p++ = static_cast<char>('0' + std::min<int>(f.dlc, 8));
    *p++ = ',';
    for (int i = 0; i < f.dlc && i < CanFrame::kMaxDlc; ++i) {
        if (i)
            *p++ = ' ';
        *p++ = hex[f.data[static_cast<std::size_t>(i)] >> 4];
        *p++ = hex[f.data[static_cast<std::size_t>(i)] & 0xF];
    }
    *p++ = '\n';
    return static_cast<int>(p - buf);
}

} // namespace

QByteArray toCsvLine(const CanFrame &f)
{
    char buf[96];
    return QByteArray(buf, formatCsvLine(f, buf));
}

bool parseCsvLine(QByteArrayView line, CanFrame &out, QString *error)
{
    auto fail = [&](const QString &m) {
        if (error)
            *error = m;
        return false;
    };
    QByteArray text = line.toByteArray().trimmed();
    const QList<QByteArray> cols = text.split(',');
    if (cols.size() != 6)
        return fail(tr("expected 6 columns (time_s,channel,id,format,dlc,data), found %1").arg(cols.size()));
    CanFrame f;
    // Time: parse seconds with up to 6 decimals exactly (no floating point drift).
    const QByteArray t = cols[0].trimmed();
    {
        const qsizetype dot = t.indexOf('.');
        bool ok1 = true, ok2 = true;
        const QByteArray ip = dot < 0 ? t : t.left(dot);
        QByteArray fp = dot < 0 ? QByteArray() : t.mid(dot + 1);
        if (fp.size() > 6)
            fp.truncate(6);
        const qlonglong whole = ip.isEmpty() ? 0 : ip.toLongLong(&ok1);
        const qlonglong frac = fp.isEmpty() ? 0 : fp.leftJustified(6, '0').toLongLong(&ok2);
        if (!ok1 || !ok2 || whole < 0 || t.isEmpty())
            return fail(tr("invalid time \"%1\"").arg(QString::fromUtf8(t)));
        f.timestampUs = whole * 1000000 + frac;
    }
    bool ok = false;
    const int ch = cols[1].trimmed().toInt(&ok);
    if (!ok || ch < 0 || ch > 255)
        return fail(tr("invalid channel \"%1\"").arg(QString::fromUtf8(cols[1].trimmed())));
    f.channel = static_cast<std::uint8_t>(ch);
    QByteArray idText = cols[2].trimmed();
    if (idText.startsWith("0x") || idText.startsWith("0X"))
        idText = idText.mid(2);
    const uint id = idText.toUInt(&ok, 16);
    if (!ok)
        return fail(tr("invalid id \"%1\"").arg(QString::fromUtf8(cols[2].trimmed())));
    const QByteArray fmt = cols[3].trimmed().toLower();
    if (fmt == "ext" || fmt == "1" || fmt == "x")
        f.extended = true;
    else if (fmt == "std" || fmt == "0" || fmt.isEmpty())
        f.extended = false;
    else
        return fail(tr("invalid format \"%1\" (use std or ext)").arg(QString::fromUtf8(cols[3].trimmed())));
    f.id = id;
    const int dlc = cols[4].trimmed().toInt(&ok);
    if (!ok || dlc < 0 || dlc > CanFrame::kMaxDlc)
        return fail(tr("invalid dlc \"%1\" (0..8)").arg(QString::fromUtf8(cols[4].trimmed())));
    f.dlc = static_cast<std::uint8_t>(dlc);
    const QList<QByteArray> bytes = cols[5].simplified().split(' ');
    int n = 0;
    for (const QByteArray &b : bytes) {
        if (b.isEmpty())
            continue;
        const uint v = b.toUInt(&ok, 16);
        if (!ok || v > 0xFF || b.size() > 2)
            return fail(tr("invalid data byte \"%1\"").arg(QString::fromUtf8(b)));
        if (n >= dlc)
            return fail(tr("more data bytes than dlc %1").arg(dlc));
        f.data[static_cast<std::size_t>(n++)] = static_cast<std::uint8_t>(v);
    }
    if (n != dlc)
        return fail(tr("%1 data bytes but dlc is %2").arg(n).arg(dlc));
    if (!f.isValid())
        return fail(tr("id %1 is out of range for %2 frames")
                        .arg(QString::fromUtf8(idText), f.extended ? QStringLiteral("29-bit") : QStringLiteral("11-bit")));
    out = f;
    return true;
}

ConvertResult exportCsv(const QString &canlogPath, const QString &csvPath)
{
    ConvertResult r;
    Reader reader;
    if (!reader.open(canlogPath, &r.error))
        return r;
    QSaveFile out(csvPath);
    if (!out.open(QIODevice::WriteOnly)) {
        r.error = tr("Could not write %1: %2").arg(QFileInfo(csvPath).fileName(), out.errorString());
        return r;
    }
    out.write(csvHeaderLine());
    // Lines are formatted into one buffer that is written in ~1 MB pieces.
    QByteArray chunk;
    chunk.reserve(1 << 20);
    char line[96];
    for (std::size_t b = 0; b < reader.blocks().size(); ++b) {
        for (const CanFrame &f : reader.blockFrames(b))
            chunk.append(line, formatCsvLine(f, line));
        if (chunk.size() >= (1 << 20) - 8192) {
            out.write(chunk);
            chunk.clear();
        }
        r.frames += static_cast<std::int64_t>(reader.blocks()[b].frameCount);
    }
    out.write(chunk);
    if (!out.commit()) {
        r.error = tr("Could not write %1: %2").arg(QFileInfo(csvPath).fileName(), out.errorString());
        return r;
    }
    r.ok = true;
    return r;
}

ConvertResult importCsv(const QString &csvPath, const QString &canlogPath, const Header &header)
{
    ConvertResult r;
    QFile in(csvPath);
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        r.error = tr("Could not open %1: %2").arg(QFileInfo(csvPath).fileName(), in.errorString());
        return r;
    }
    Writer writer;
    if (!writer.open(canlogPath, header, &r.error))
        return r;
    int lineNo = 0;
    std::int64_t lastTs = -1;
    while (!in.atEnd()) {
        const QByteArray line = in.readLine();
        ++lineNo;
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        if (lineNo == 1 && trimmed.startsWith("time"))
            continue; // header row
        CanFrame f;
        QString error;
        if (!parseCsvLine(trimmed, f, &error)) {
            r.error = tr("%1, line %2: %3").arg(QFileInfo(csvPath).fileName()).arg(lineNo).arg(error);
            writer.close();
            QFile::remove(canlogPath);
            return r;
        }
        if (f.timestampUs < lastTs) {
            r.error = tr("%1, line %2: rows must be in time order (%3 s comes after a later row)")
                          .arg(QFileInfo(csvPath).fileName()).arg(lineNo)
                          .arg(static_cast<double>(f.timestampUs) / 1e6, 0, 'f', 6);
            writer.close();
            QFile::remove(canlogPath);
            return r;
        }
        lastTs = f.timestampUs;
        writer.append(f);
        ++r.frames;
    }
    if (r.frames == 0) {
        r.error = tr("%1 contains no frames.").arg(QFileInfo(csvPath).fileName());
        writer.close();
        QFile::remove(canlogPath);
        return r;
    }
    r.ok = writer.close();
    if (!r.ok)
        r.error = writer.errorString();
    return r;
}

} // namespace can::canlog
