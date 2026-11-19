#include "canlog.h"
#include "logrecorder.h"
#include "simulatorengine.h"

#include <QElapsedTimer>
#include <QFile>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace can;

// Log writer/reader round trip, truncated and damaged files, index and seek
// accuracy, CSV export/import round trip.
class TestCanLog : public QObject
{
    Q_OBJECT

    static std::vector<CanFrame> simFrames(std::int64_t untilUs, SimulatorEngine::Mode mode = SimulatorEngine::Mode::Raw)
    {
        SimulatorEngine engine(42, mode);
        engine.setBurstsEnabled(true);
        engine.setTargetRate(2000);
        std::vector<CanFrame> frames;
        engine.advanceTo(untilUs, frames);
        return frames;
    }

    static canlog::Header header()
    {
        canlog::Header h;
        h.startTime = QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5), QTimeZone::UTC);
        h.application = QStringLiteral("test");
        h.source = QStringLiteral("unit test");
        h.channels = {canlog::Channel{1, QStringLiteral("CAN 1"), 250000}};
        return h;
    }

    static bool writeLog(const QString &path, const std::vector<CanFrame> &frames, int flushEvery = 0)
    {
        canlog::Writer w;
        if (!w.open(path, header()))
            return false;
        for (std::size_t i = 0; i < frames.size(); ++i) {
            w.append(frames[i]);
            if (flushEvery > 0 && (i + 1) % static_cast<std::size_t>(flushEvery) == 0)
                w.flush();
        }
        return w.close();
    }

private slots:
    void crc32KnownValue()
    {
        const QByteArray s("123456789");
        QCOMPARE(canlog::crc32(s.constData(), static_cast<std::size_t>(s.size())), 0xCBF43926u);
    }

    void roundTrip()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("a.canlog"));
        auto frames = simFrames(3'000'000);
        QVERIFY(frames.size() > 5000);
        QVERIFY(std::any_of(frames.begin(), frames.end(), [](const CanFrame &f) { return f.extended; }));
        QVERIFY(writeLog(path, frames, 777));

        canlog::Reader r;
        QVERIFY2(r.open(path), qPrintable(r.errorString()));
        QCOMPARE(r.frameCount(), static_cast<std::int64_t>(frames.size()));
        QCOMPARE(r.header().application, QStringLiteral("test"));
        QCOMPARE(r.header().source, QStringLiteral("unit test"));
        QCOMPARE(r.header().bitrate(), 250000);
        QCOMPARE(r.header().startTime, header().startTime);
        QCOMPARE(r.truncatedBytes(), 0);
        QVERIFY(r.warning().isEmpty());
        std::vector<CanFrame> back;
        QCOMPARE(r.read(0, frames.size() + 10, back), frames.size());
        QCOMPARE(back.size(), frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            QVERIFY2(back[i] == frames[i], qPrintable(QStringLiteral("frame %1 differs").arg(i)));
        QCOMPARE(r.firstTimestampUs(), frames.front().timestampUs);
        QCOMPARE(r.lastTimestampUs(), frames.back().timestampUs);
    }

    void emptyLogAndBadFiles()
    {
        QTemporaryDir dir;
        const QString empty = dir.filePath(QStringLiteral("empty.canlog"));
        QVERIFY(writeLog(empty, {}));
        canlog::Reader r;
        QVERIFY(r.open(empty));
        QCOMPARE(r.frameCount(), 0);

        const QString notLog = dir.filePath(QStringLiteral("x.canlog"));
        QFile f(notLog);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello world, definitely not a log file");
        f.close();
        QString error;
        QVERIFY(!r.open(notLog, &error));
        QVERIFY(error.contains(QStringLiteral("not a CAN Analyzer log")));
        QVERIFY(!r.open(dir.filePath(QStringLiteral("missing.canlog")), &error));
    }

    void futureMajorVersionIsRejected()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("v.canlog"));
        QVERIFY(writeLog(path, simFrames(100'000)));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadWrite));
        f.seek(8);
        f.write("\x02\x00", 2);
        f.close();
        canlog::Reader r;
        QString error;
        QVERIFY(!r.open(path, &error));
        QVERIFY(error.contains(QStringLiteral("not supported")));
    }

    void truncatedFileKeepsCompleteBlocks()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("t.canlog"));
        auto frames = simFrames(2'000'000);
        QVERIFY(writeLog(path, frames, 500)); // blocks of 500 frames
        canlog::Reader full;
        QVERIFY(full.open(path));
        const auto &blocks = full.blocks();
        QVERIFY(blocks.size() >= 4);
        // Cut the file in the middle of the third block (simulated crash).
        const qint64 cut = blocks[2].offset + canlog::kBlockHeaderSize + blocks[2].payloadSize / 2;
        const std::int64_t expected = blocks[2].firstFrame;
        full.close();
        QFile f(path);
        QVERIFY(f.resize(cut));

        canlog::Reader r;
        QVERIFY(r.open(path));
        QCOMPARE(r.frameCount(), expected);
        QVERIFY(r.truncatedBytes() > 0);
        QVERIFY(r.warning().contains(QStringLiteral("incomplete")));
        std::vector<CanFrame> back;
        QCOMPARE(r.read(0, 1'000'000, back), static_cast<std::size_t>(expected));
        for (std::size_t i = 0; i < back.size(); ++i)
            QVERIFY(back[i] == frames[i]);

        // Cut inside a block header too.
        QVERIFY(f.resize(blocks[1].offset + 10));
        QVERIFY(r.open(path));
        QCOMPARE(r.frameCount(), blocks[1].firstFrame);
    }

    void damagedPayloadIsSkipped()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("d.canlog"));
        auto frames = simFrames(1'000'000);
        QVERIFY(writeLog(path, frames, 400));
        canlog::Reader r;
        QVERIFY(r.open(path));
        const auto b1 = r.blocks()[1];
        r.close();
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadWrite));
        f.seek(b1.offset + canlog::kBlockHeaderSize + 5);
        f.write("\xFF\xFF\xFF", 3);
        f.close();
        QVERIFY(r.open(path));
        QVERIFY(r.blockFrames(1).empty());
        QVERIFY(r.warning().contains(QStringLiteral("checksum")));
        std::vector<CanFrame> back;
        const std::size_t n = r.read(0, frames.size(), back);
        QCOMPARE(n, frames.size() - b1.frameCount);
    }

    void recorderSurvivesAbruptStop()
    {
        // Synchronous recorder: after a flush the file is readable even though
        // the recorder was never stopped (process killed).
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("r.canlog"));
        auto frames = simFrames(1'000'000);
        auto *rec = new LogRecorder;
        rec->setSynchronous(true);
        QVERIFY(rec->start(path, header()));
        rec->append(frames.data(), frames.size() / 2);
        rec->flushNow();
        rec->append(frames.data() + frames.size() / 2, frames.size() - frames.size() / 2); // not flushed
        canlog::Reader r;
        QVERIFY(r.open(path));
        QCOMPARE(r.frameCount(), static_cast<std::int64_t>(frames.size() / 2));
        CanFrame first;
        QVERIFY(r.frameAt(0, first));
        QCOMPARE(first.timestampUs, 0); // relative to the first recorded frame
        QVERIFY(rec->stop());
        QVERIFY(r.open(path));
        QCOMPARE(r.frameCount(), static_cast<std::int64_t>(frames.size()));
        QCOMPARE(rec->durationUs(), frames.back().timestampUs - frames.front().timestampUs);
        delete rec;
    }

    void threadedRecorderWritesEverything()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("rt.canlog"));
        auto frames = simFrames(1'500'000);
        LogRecorder rec;
        QVERIFY(rec.start(path, header()));
        for (std::size_t i = 0; i < frames.size(); i += 100)
            rec.append(frames.data() + i, std::min<std::size_t>(100, frames.size() - i));
        QVERIFY(rec.stop());
        QVERIFY(rec.lastError().isEmpty());
        canlog::Reader r;
        QVERIFY(r.open(path));
        QCOMPARE(r.frameCount(), static_cast<std::int64_t>(frames.size()));
        QCOMPARE(rec.lastFile(), path);
    }

    void recorderReportsUnwritablePath()
    {
        LogRecorder rec;
        QVERIFY(!rec.start(QStringLiteral("Z:/no/such/dir/x.canlog"), header()));
        QVERIFY(!rec.lastError().isEmpty());
        QVERIFY(!rec.isRecording());
        QCOMPARE(LogRecorder::defaultFileName(QDateTime(QDate(2026, 11, 19), QTime(14, 5, 33))),
                 QStringLiteral("can_2026-11-19_14-05-33.canlog"));
    }

    void indexAndSeekAccuracy()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("s.canlog"));
        auto frames = simFrames(5'000'000);
        QVERIFY(writeLog(path, frames, 333));
        canlog::Reader r;
        QVERIFY(r.open(path));
        QRandomGenerator rng(7);
        for (int i = 0; i < 400; ++i) {
            const std::int64_t t = static_cast<std::int64_t>(rng.bounded(5'100'000));
            const auto expected = std::lower_bound(frames.begin(), frames.end(), t,
                                                   [](const CanFrame &f, std::int64_t x) { return f.timestampUs < x; })
                                  - frames.begin();
            QCOMPARE(r.indexAtTime(t), static_cast<std::int64_t>(expected));
            CanFrame f;
            if (expected < static_cast<std::ptrdiff_t>(frames.size())) {
                QVERIFY(r.frameAt(expected, f));
                QVERIFY(f == frames[static_cast<std::size_t>(expected)]);
            }
        }
        QCOMPARE(r.indexAtTime(-5), 0);
        QCOMPARE(r.indexAtTime(frames.back().timestampUs + 1), static_cast<std::int64_t>(frames.size()));
        QVERIFY(r.cachedBlocks() <= 2);
    }

    void millionFrameLogStaysBounded()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("big.canlog"));
        // 1,000,000 frames from the simulator (about 8.3 minutes at 2000 fr/s).
        SimulatorEngine engine(9, SimulatorEngine::Mode::Raw);
        engine.setTargetRate(2000);
        canlog::Writer w;
        QVERIFY(w.open(path, header()));
        std::vector<CanFrame> chunk;
        std::vector<CanFrame> samples; // every 997th frame for verification
        std::int64_t written = 0, t = 0;
        QElapsedTimer timer;
        timer.start();
        while (written < 1'000'000) {
            chunk.clear();
            t += 250'000;
            engine.advanceTo(t, chunk);
            for (const auto &f : chunk) {
                if (written >= 1'000'000)
                    break;
                if (written % 997 == 0)
                    samples.push_back(f);
                w.append(f);
                ++written;
            }
            w.flush(); // like the recorder: a block every 250 ms
        }
        QVERIFY(w.close());
        const qint64 writeMs = timer.elapsed();
        timer.restart();
        canlog::Reader r;
        QVERIFY(r.open(path));
        const qint64 openMs = timer.elapsed();
        QCOMPARE(r.frameCount(), 1'000'000);
        // Index memory is per block, not per frame.
        QVERIFY(r.blocks().size() < 5000);
        timer.restart();
        for (std::size_t i = 0; i < samples.size(); ++i) {
            CanFrame f;
            QVERIFY(r.frameAt(static_cast<std::int64_t>(i) * 997, f));
            QVERIFY(f == samples[i]);
            QCOMPARE(r.indexAtTime(samples[i].timestampUs) <= static_cast<std::int64_t>(i) * 997, true);
        }
        const qint64 seekMs = timer.elapsed();
        QVERIFY(r.cachedBlocks() <= 2);
        qInfo("1M frames: %lld bytes, %zu blocks, write %lld ms, open+index %lld ms, %zu random reads %lld ms",
              static_cast<long long>(r.fileSize()), r.blocks().size(), static_cast<long long>(writeMs),
              static_cast<long long>(openMs), samples.size(), static_cast<long long>(seekMs));
        QVERIFY(openMs < 2000);
    }

    void csvLineFormat()
    {
        CanFrame f;
        f.id = 0x1A4;
        f.dlc = 3;
        f.data = {0x01, 0xAB, 0xFF};
        f.timestampUs = 12'000'345;
        f.channel = 2;
        QCOMPARE(canlog::toCsvLine(f), QByteArray("12.000345,2,1A4,std,3,01 AB FF\n"));
        f.extended = true;
        f.id = 0x18FEF100;
        f.dlc = 0;
        f.data = {}; // bytes beyond the dlc are always zero
        QCOMPARE(canlog::toCsvLine(f), QByteArray("12.000345,2,18FEF100,ext,0,\n"));

        CanFrame back;
        QString error;
        QVERIFY(canlog::parseCsvLine("12.000345,2,18FEF100,ext,0,", back, &error));
        QVERIFY(back == f);
        QVERIFY(canlog::parseCsvLine(" 0.5 , 1 , 0x7ff , std , 2 , a 0B ", back, &error));
        QCOMPARE(back.timestampUs, 500'000);
        QCOMPARE(back.id, 0x7FFu);
        QCOMPARE(back.data[0], 0x0A);

        QVERIFY(!canlog::parseCsvLine("1,1,800,std,0,", back, &error)); // 11-bit overflow
        QVERIFY(error.contains(QStringLiteral("out of range")));
        QVERIFY(!canlog::parseCsvLine("1,1,100,std,2,01", back, &error));
        QVERIFY(error.contains(QStringLiteral("dlc")));
        QVERIFY(!canlog::parseCsvLine("1,1,100,fd,0,", back, &error));
        QVERIFY(!canlog::parseCsvLine("x,1,100,std,0,", back, &error));
        QVERIFY(!canlog::parseCsvLine("1,1,100,std,1,1FF", back, &error));
        QVERIFY(!canlog::parseCsvLine("1,1,100", back, &error));
        QVERIFY(error.contains(QStringLiteral("6 columns")));
    }

    void csvRoundTrip()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath(QStringLiteral("a.canlog"));
        const QString csv = dir.filePath(QStringLiteral("a.csv"));
        const QString log2 = dir.filePath(QStringLiteral("b.canlog"));
        auto frames = simFrames(1'000'000);
        QVERIFY(writeLog(log, frames));
        auto ex = canlog::exportCsv(log, csv);
        QVERIFY2(ex.ok, qPrintable(ex.error));
        QCOMPARE(ex.frames, static_cast<std::int64_t>(frames.size()));
        QFile f(csv);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readLine(), canlog::csvHeaderLine());
        f.close();
        auto im = canlog::importCsv(csv, log2, header());
        QVERIFY2(im.ok, qPrintable(im.error));
        QCOMPARE(im.frames, static_cast<std::int64_t>(frames.size()));
        canlog::Reader r;
        QVERIFY(r.open(log2));
        std::vector<CanFrame> back;
        r.read(0, frames.size(), back);
        QCOMPARE(back.size(), frames.size());
        for (std::size_t i = 0; i < frames.size(); ++i)
            QVERIFY(back[i] == frames[i]);
    }

    void csvImportErrors()
    {
        QTemporaryDir dir;
        const QString csv = dir.filePath(QStringLiteral("bad.csv"));
        const QString out = dir.filePath(QStringLiteral("bad.canlog"));
        auto write = [&](const QByteArray &text) {
            QFile f(csv);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(text);
        };
        write("time_s,channel,id,format,dlc,data\n0.1,1,100,std,1,01\n0.05,1,100,std,1,02\n");
        auto r = canlog::importCsv(csv, out, header());
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("line 3")));
        QVERIFY(r.error.contains(QStringLiteral("time order")));
        QVERIFY(!QFile::exists(out));

        write("time_s,channel,id,format,dlc,data\n0.1,1,100,std,1,01\n0.2,1,ZZZ,std,1,02\n");
        r = canlog::importCsv(csv, out, header());
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("line 3: invalid id")));

        write("time_s,channel,id,format,dlc,data\n");
        r = canlog::importCsv(csv, out, header());
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("no frames")));

        // Comments, blank lines and no header row are fine.
        write("# exported by hand\n\n0.1,1,100,std,1,01\n0.2,1,100,std,1,02\n");
        r = canlog::importCsv(csv, out, header());
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.frames, 2);
    }
};

QTEST_GUILESS_MAIN(TestCanLog)
#include "tst_canlog.moc"
