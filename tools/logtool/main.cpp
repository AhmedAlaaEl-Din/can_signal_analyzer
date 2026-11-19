// can_logtool: command-line helper for .canlog files.
//
//   can_logtool generate --out <file> [--seconds 60] [--rate 1000] [--seed 0xC0FFEE]
//       Writes a log of the simulated demo vehicle (deterministic for a seed).
//   can_logtool bench [--frames 1000000] [--dir <folder>] [--report <file>]
//       Large-log benchmark: write, index, random seek, sequential read, CSV
//       export, and the process memory while doing so.
//   can_logtool csv <in.canlog> <out.csv>
//   can_logtool info <file.canlog>

#include "canlog.h"
#include "simulatorengine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QTextStream>
#include <QTimeZone>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

#include <algorithm>
#include <cstdio>
#include <vector>

using namespace can;

namespace {

struct Memory
{
    double workingSetMb = 0;
    double peakMb = 0;
};

Memory memory()
{
    Memory m;
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof pmc)) {
        m.workingSetMb = double(pmc.WorkingSetSize) / (1024.0 * 1024.0);
        m.peakMb = double(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return m;
}

canlog::Header sampleHeader(const QString &source)
{
    canlog::Header h;
    // Fixed start time: the generated sample is byte-for-byte reproducible.
    h.startTime = QDateTime(QDate(2026, 1, 1), QTime(9, 0, 0), QTimeZone::UTC);
    h.application = QStringLiteral("can_logtool %1").arg(QCoreApplication::applicationVersion());
    h.source = source;
    h.channels = {canlog::Channel{1, QStringLiteral("CAN 1 (simulated)"), 500000}};
    return h;
}

int generate(const QString &out, double seconds, int rate, quint64 seed)
{
    SimulatorEngine engine(seed, SimulatorEngine::Mode::Vehicle);
    engine.setTargetRate(rate);
    canlog::Writer w;
    QString error;
    QDir().mkpath(QFileInfo(out).absolutePath());
    if (!w.open(out, sampleHeader(QStringLiteral("Demo vehicle (simulated), seed 0x%1").arg(seed, 0, 16)), &error)) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    std::vector<CanFrame> chunk;
    const auto end = static_cast<std::int64_t>(seconds * 1e6);
    for (std::int64_t t = 250'000; t <= end; t += 250'000) {
        chunk.clear();
        engine.advanceTo(t, chunk);
        w.append(chunk.data(), chunk.size());
        w.flush(); // one block per 250 ms, like the live recorder
    }
    if (!w.close()) {
        std::fprintf(stderr, "%s\n", qPrintable(w.errorString()));
        return 1;
    }
    std::printf("%s: %llu frames, %.1f s, %llu bytes\n", qPrintable(out),
                static_cast<unsigned long long>(w.framesWritten()), seconds,
                static_cast<unsigned long long>(w.bytesWritten()));
    return 0;
}

int bench(qint64 frameTarget, const QString &dirPath, const QString &reportPath)
{
    QDir dir(dirPath);
    dir.mkpath(QStringLiteral("."));
    const QString log = dir.filePath(QStringLiteral("bench_1m.canlog"));
    const QString csv = dir.filePath(QStringLiteral("bench_1m.csv"));
    QString report;
    QTextStream os(&report);
    const Memory m0 = memory();

    // 1. Write: demo vehicle at 2000 frames/s, flushed every 250 ms.
    QElapsedTimer t;
    t.start();
    SimulatorEngine engine(SimulatorEngine::kDefaultSeed, SimulatorEngine::Mode::Vehicle);
    engine.setTargetRate(2000);
    canlog::Writer w;
    if (!w.open(log, sampleHeader(QStringLiteral("benchmark")))) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(log));
        return 1;
    }
    std::vector<CanFrame> chunk;
    std::int64_t written = 0;
    for (std::int64_t ts = 250'000; written < frameTarget; ts += 250'000) {
        chunk.clear();
        engine.advanceTo(ts, chunk);
        const auto take = static_cast<std::size_t>(std::min<std::int64_t>(frameTarget - written, static_cast<std::int64_t>(chunk.size())));
        w.append(chunk.data(), take);
        written += static_cast<std::int64_t>(take);
        w.flush();
    }
    w.close();
    const qint64 writeMs = t.elapsed();
    const Memory m1 = memory();

    // 2. Open + index.
    t.restart();
    canlog::Reader r;
    if (!r.open(log)) {
        std::fprintf(stderr, "cannot read %s\n", qPrintable(log));
        return 1;
    }
    const double openMs = double(t.nsecsElapsed()) / 1e6;
    const Memory m2 = memory();

    // 3. Random seeks by time (index lookup + decode of one block).
    QRandomGenerator rng(1234);
    const int seeks = 1000;
    double maxSeekUs = 0, totalSeekUs = 0;
    for (int i = 0; i < seeks; ++i) {
        const auto target = r.firstTimestampUs() + static_cast<std::int64_t>(rng.bounded(static_cast<double>(r.durationUs())));
        QElapsedTimer st;
        st.start();
        const std::int64_t idx = r.indexAtTime(target);
        CanFrame f;
        r.frameAt(std::min(idx, r.frameCount() - 1), f);
        const double us = double(st.nsecsElapsed()) / 1e3;
        totalSeekUs += us;
        maxSeekUs = std::max(maxSeekUs, us);
    }

    // 4. Sequential read of everything.
    t.restart();
    std::int64_t readCount = 0;
    std::uint64_t checksum = 0;
    for (std::size_t b = 0; b < r.blocks().size(); ++b)
        for (const CanFrame &f : r.blockFrames(b)) {
            ++readCount;
            checksum += f.id + f.data[0];
        }
    const qint64 readMs = t.elapsed();
    const Memory m3 = memory();

    // 5. CSV export.
    t.restart();
    const auto ex = canlog::exportCsv(log, csv);
    const qint64 csvMs = t.elapsed();
    const qint64 csvBytes = QFileInfo(csv).size();
    const Memory m4 = memory();

    os << "Large-log benchmark (can_logtool bench)\n";
    os << "frames                 " << r.frameCount() << " (demo vehicle, 2000 fr/s, " << QString::number(r.durationUs() / 1e6, 'f', 1) << " s)\n";
    os << "file size              " << QString::number(r.fileSize() / 1048576.0, 'f', 2) << " MiB ("
       << QString::number(double(r.fileSize()) / double(r.frameCount()), 'f', 2) << " bytes/frame)\n";
    os << "blocks (index entries) " << r.blocks().size() << " (" << QString::number(r.blocks().size() * sizeof(canlog::BlockInfo) / 1024.0, 'f', 1) << " KiB index)\n";
    os << "write                  " << writeMs << " ms (" << QString::number(r.frameCount() / (writeMs / 1000.0) / 1e6, 'f', 2) << " M frames/s incl. simulation)\n";
    os << "open + index           " << QString::number(openMs, 'f', 2) << " ms\n";
    os << "random seek (1000x)    avg " << QString::number(totalSeekUs / seeks, 'f', 1) << " us, max " << QString::number(maxSeekUs, 'f', 1) << " us\n";
    os << "sequential read        " << readMs << " ms (" << readCount << " frames, checksum " << checksum % 100000 << ")\n";
    os << "CSV export             " << csvMs << " ms, " << QString::number(csvBytes / 1048576.0, 'f', 1) << " MiB" << (ex.ok ? "" : " FAILED") << "\n";
    os << "memory (working set)   start " << QString::number(m0.workingSetMb, 'f', 1) << " MiB, after write "
       << QString::number(m1.workingSetMb, 'f', 1) << ", after open " << QString::number(m2.workingSetMb, 'f', 1)
       << ", after full read " << QString::number(m3.workingSetMb, 'f', 1) << ", after CSV " << QString::number(m4.workingSetMb, 'f', 1)
       << " MiB; peak " << QString::number(m4.peakMb, 'f', 1) << " MiB\n";
    os << "reader cache           " << r.cachedBlocks() << " decoded blocks held (max 2)\n";
    os.flush();
    std::printf("%s", qPrintable(report));
    if (!reportPath.isEmpty()) {
        QFile f(reportPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            f.write(report.toUtf8());
    }
    r.close();
    QFile::remove(csv);
    QFile::remove(log);
    return ex.ok ? 0 : 1;
}

int info(const QString &path)
{
    canlog::Reader r;
    QString error;
    if (!r.open(path, &error)) {
        std::fprintf(stderr, "%s\n", qPrintable(error));
        return 1;
    }
    std::printf("format %d.%d, %lld frames, %.3f s, %zu blocks, %lld bytes\n", r.header().versionMajor,
                r.header().versionMinor, static_cast<long long>(r.frameCount()), r.durationUs() / 1e6,
                r.blocks().size(), static_cast<long long>(r.fileSize()));
    std::printf("application: %s\nsource: %s\n", qPrintable(r.header().application), qPrintable(r.header().source));
    if (!r.warning().isEmpty())
        std::printf("warning: %s\n", qPrintable(r.warning()));
    return 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("can_logtool"));
    QCoreApplication::setApplicationVersion(QStringLiteral(CAN_ANALYZER_VERSION));
    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Generate, inspect and benchmark .canlog files."));
    p.addHelpOption();
    p.addVersionOption();
    p.addPositionalArgument(QStringLiteral("command"), QStringLiteral("generate | bench | csv | info"));
    const QCommandLineOption outOpt(QStringLiteral("out"), QStringLiteral("Output file."), QStringLiteral("file"));
    const QCommandLineOption secOpt(QStringLiteral("seconds"), QStringLiteral("Duration."), QStringLiteral("s"), QStringLiteral("60"));
    const QCommandLineOption rateOpt(QStringLiteral("rate"), QStringLiteral("Frames/s (0 = nominal)."), QStringLiteral("fps"), QStringLiteral("1000"));
    const QCommandLineOption seedOpt(QStringLiteral("seed"), QStringLiteral("Simulator seed."), QStringLiteral("seed"), QStringLiteral("0xC0FFEE"));
    const QCommandLineOption framesOpt(QStringLiteral("frames"), QStringLiteral("Benchmark frames."), QStringLiteral("n"), QStringLiteral("1000000"));
    const QCommandLineOption dirOpt(QStringLiteral("dir"), QStringLiteral("Benchmark folder."), QStringLiteral("dir"), QDir::tempPath());
    const QCommandLineOption reportOpt(QStringLiteral("report"), QStringLiteral("Write the benchmark report here."), QStringLiteral("file"));
    p.addOptions({outOpt, secOpt, rateOpt, seedOpt, framesOpt, dirOpt, reportOpt});
    p.process(app);
    const QStringList args = p.positionalArguments();
    const QString cmd = args.value(0);
    if (cmd == QLatin1String("generate") && p.isSet(outOpt))
        return generate(p.value(outOpt), p.value(secOpt).toDouble(), p.value(rateOpt).toInt(),
                        p.value(seedOpt).toULongLong(nullptr, 0));
    if (cmd == QLatin1String("bench"))
        return bench(p.value(framesOpt).toLongLong(), p.value(dirOpt), p.value(reportOpt));
    if (cmd == QLatin1String("csv") && args.size() == 3) {
        const auto r = canlog::exportCsv(args[1], args[2]);
        if (!r.ok)
            std::fprintf(stderr, "%s\n", qPrintable(r.error));
        return r.ok ? 0 : 1;
    }
    if (cmd == QLatin1String("info") && args.size() == 2)
        return info(args[1]);
    p.showHelp(2);
}
