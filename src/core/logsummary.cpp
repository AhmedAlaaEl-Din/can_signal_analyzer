#include "logsummary.h"

#include "busstatistics.h"

#include <QFileInfo>
#include <QLocale>
#include <QSaveFile>

#include <algorithm>
#include <unordered_map>

namespace can {

LogSummary LogSummary::compute(const QString &path, std::shared_ptr<const Database> db)
{
    LogSummary s;
    canlog::Reader reader;
    if (!reader.open(path, &s.error))
        return s;
    s.fileName = QFileInfo(path).fileName();
    s.fileSize = reader.fileSize();
    s.startTime = reader.header().startTime;
    s.application = reader.header().application;
    s.source = reader.header().source;
    s.bitrate = std::max(1, reader.header().bitrate());
    s.firstUs = reader.firstTimestampUs();
    s.lastUs = reader.lastTimestampUs();

    std::unordered_map<std::uint32_t, IdStats> map;
    // Bits per 1 s window for the peak load.
    std::int64_t windowStart = s.firstUs;
    double windowBits = 0;
    double totalBits = 0;
    for (std::size_t b = 0; b < reader.blocks().size(); ++b) {
        for (const CanFrame &f : reader.blockFrames(b)) {
            const double bits = BusStatistics::frameBits(f);
            totalBits += bits;
            while (f.timestampUs >= windowStart + 1'000'000) {
                s.peakLoad = std::max(s.peakLoad, windowBits / s.bitrate);
                windowBits = 0;
                windowStart += 1'000'000;
            }
            windowBits += bits;
            auto [it, inserted] = map.try_emplace(f.key());
            IdStats &st = it->second;
            if (inserted) {
                st.id = f.id;
                st.extended = f.extended;
                st.firstUs = f.timestampUs;
                if (db)
                    if (const MessageDef *m = db->find(f.id, f.extended))
                        st.name = m->name;
            } else {
                const std::int64_t p = f.timestampUs - st.lastUs;
                st.minPeriodUs = st.minPeriodUs < 0 ? p : std::min(st.minPeriodUs, p);
                st.maxPeriodUs = std::max(st.maxPeriodUs, p);
            }
            st.lastUs = f.timestampUs;
            st.count++;
            st.dlcMin = std::min<int>(st.dlcMin, f.dlc);
            st.dlcMax = std::max<int>(st.dlcMax, f.dlc);
            st.bits += bits;
            s.frames++;
        }
    }
    // The last (partial) window counts relative to a full second only when the
    // log is at least that long.
    if (s.durationUs() >= 1'000'000)
        s.peakLoad = std::max(s.peakLoad, windowBits / s.bitrate);
    const double seconds = std::max(1e-6, double(s.durationUs()) / 1e6);
    s.averageLoad = totalBits / seconds / s.bitrate;
    s.averageRate = double(s.frames) / seconds;
    if (s.durationUs() < 1'000'000)
        s.peakLoad = s.averageLoad;
    s.ids.reserve(map.size());
    for (auto &[k, v] : map)
        s.ids.push_back(v);
    std::sort(s.ids.begin(), s.ids.end(), [](const IdStats &a, const IdStats &b) {
        return a.extended != b.extended ? !a.extended : a.id < b.id;
    });
    s.warning = reader.warning();
    s.ok = true;
    return s;
}

QVariantMap LogSummary::toVariant() const
{
    QVariantList rows;
    const double seconds = std::max(1e-6, double(durationUs()) / 1e6);
    for (const auto &st : ids) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), formatId(st.id, st.extended)},
            {QStringLiteral("name"), st.name},
            {QStringLiteral("count"), QVariant::fromValue(st.count)},
            {QStringLiteral("share"), frames > 0 ? double(st.count) / double(frames) : 0.0},
            {QStringLiteral("rate"), double(st.count) / seconds},
            {QStringLiteral("periodMs"), st.meanPeriodUs() / 1000.0},
            {QStringLiteral("minPeriodMs"), st.minPeriodUs < 0 ? 0.0 : st.minPeriodUs / 1000.0},
            {QStringLiteral("maxPeriodMs"), st.maxPeriodUs < 0 ? 0.0 : st.maxPeriodUs / 1000.0},
            {QStringLiteral("dlc"), st.dlcMin == st.dlcMax ? QString::number(st.dlcMax)
                                                           : QStringLiteral("%1–%2").arg(st.dlcMin).arg(st.dlcMax)},
            {QStringLiteral("load"), st.bits / seconds / bitrate},
        });
    }
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
        {QStringLiteral("warning"), warning},
        {QStringLiteral("fileName"), fileName},
        {QStringLiteral("fileSize"), QVariant::fromValue(fileSize)},
        {QStringLiteral("startTime"), startTime.isValid() ? startTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString()},
        {QStringLiteral("application"), application},
        {QStringLiteral("source"), source},
        {QStringLiteral("bitrate"), bitrate},
        {QStringLiteral("frames"), QVariant::fromValue(frames)},
        {QStringLiteral("firstUs"), QVariant::fromValue(firstUs)},
        {QStringLiteral("lastUs"), QVariant::fromValue(lastUs)},
        {QStringLiteral("durationUs"), QVariant::fromValue(durationUs())},
        {QStringLiteral("averageLoad"), averageLoad},
        {QStringLiteral("peakLoad"), peakLoad},
        {QStringLiteral("averageRate"), averageRate},
        {QStringLiteral("idCount"), static_cast<int>(ids.size())},
        {QStringLiteral("rows"), rows},
    };
}

bool LogSummary::writeCsv(const QString &path, QString *errorOut) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not write %1: %2").arg(QFileInfo(path).fileName(), f.errorString());
        return false;
    }
    const QLocale c = QLocale::c();
    auto num = [&](double v, int d) { return c.toString(v, 'f', d); };
    QByteArray out;
    out += "key,value\n";
    out += "file," + fileName.toUtf8() + "\n";
    out += "start_time_utc," + (startTime.isValid() ? startTime.toUTC().toString(Qt::ISODate).toUtf8() : QByteArray()) + "\n";
    out += "frames," + QByteArray::number(static_cast<qlonglong>(frames)) + "\n";
    out += "first_timestamp_s," + num(firstUs / 1e6, 6).toUtf8() + "\n";
    out += "last_timestamp_s," + num(lastUs / 1e6, 6).toUtf8() + "\n";
    out += "duration_s," + num(durationUs() / 1e6, 6).toUtf8() + "\n";
    out += "bitrate," + QByteArray::number(bitrate) + "\n";
    out += "average_bus_load_percent," + num(averageLoad * 100, 2).toUtf8() + "\n";
    out += "peak_bus_load_percent," + num(peakLoad * 100, 2).toUtf8() + "\n";
    out += "average_rate_fps," + num(averageRate, 1).toUtf8() + "\n";
    out += "\n";
    out += "id,format,name,count,share_percent,rate_hz,mean_period_ms,min_period_ms,max_period_ms,dlc_min,dlc_max,bus_load_percent\n";
    const double seconds = std::max(1e-6, double(durationUs()) / 1e6);
    for (const auto &st : ids) {
        out += formatId(st.id, st.extended).toUtf8() + (st.extended ? ",ext," : ",std,");
        out += st.name.toUtf8() + ",";
        out += QByteArray::number(static_cast<qlonglong>(st.count)) + ",";
        out += num(frames ? 100.0 * double(st.count) / double(frames) : 0, 2).toUtf8() + ",";
        out += num(double(st.count) / seconds, 2).toUtf8() + ",";
        out += num(st.meanPeriodUs() / 1000.0, 3).toUtf8() + ",";
        out += num(st.minPeriodUs < 0 ? 0 : st.minPeriodUs / 1000.0, 3).toUtf8() + ",";
        out += num(st.maxPeriodUs < 0 ? 0 : st.maxPeriodUs / 1000.0, 3).toUtf8() + ",";
        out += QByteArray::number(st.dlcMin) + "," + QByteArray::number(st.dlcMax) + ",";
        out += num(100.0 * st.bits / seconds / bitrate, 2).toUtf8() + "\n";
    }
    f.write(out);
    if (!f.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("Could not write %1: %2").arg(QFileInfo(path).fileName(), f.errorString());
        return false;
    }
    return true;
}

} // namespace can
