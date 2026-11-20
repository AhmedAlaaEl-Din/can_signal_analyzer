#pragma once

#include "canlog.h"
#include "dbc.h"

#include <QVariantMap>

#include <memory>
#include <vector>

namespace can {

// Statistics over a whole log file: duration, frame count, bus load and one
// row per message ID. Computed in one streaming pass (memory independent of
// the log size).
struct LogSummary
{
    struct IdStats
    {
        std::uint32_t id = 0;
        bool extended = false;
        QString name;              // from the database, if any
        std::int64_t count = 0;
        int dlcMin = 8;
        int dlcMax = 0;
        std::int64_t firstUs = 0;
        std::int64_t lastUs = 0;
        std::int64_t minPeriodUs = -1;
        std::int64_t maxPeriodUs = -1;
        double bits = 0;

        double meanPeriodUs() const { return count > 1 ? double(lastUs - firstUs) / double(count - 1) : 0.0; }
    };

    bool ok = false;
    QString error;
    QString warning;
    QString fileName;
    qint64 fileSize = 0;
    QDateTime startTime;           // UTC, from the header
    QString application;
    QString source;
    int bitrate = 500000;
    std::int64_t frames = 0;
    std::int64_t firstUs = 0;
    std::int64_t lastUs = 0;
    double averageLoad = 0;        // 0..1 over the whole log
    double peakLoad = 0;           // highest 1 s window
    double averageRate = 0;        // frames/s
    std::vector<IdStats> ids;      // sorted by id

    std::int64_t durationUs() const { return lastUs - firstUs; }

    static LogSummary compute(const QString &path, std::shared_ptr<const Database> db);
    QVariantMap toVariant() const;
    // CSV with a short header block (key,value) followed by the per-ID table.
    bool writeCsv(const QString &path, QString *error) const;
};

} // namespace can
