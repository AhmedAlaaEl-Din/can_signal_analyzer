#include "canlog.h"
#include "capturesession.h"
#include "framemodel.h"
#include "logsummary.h"
#include "markerstore.h"
#include "replaysource.h"
#include "signaldatabase.h"
#include "signallistmodel.h"
#include "simulatedsource.h"
#include "simulatorengine.h"
#include "timeseriesstore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace can;

// Replay timing with a fake clock at different speeds, pause / step / loop /
// seek with pre-roll, the full session pipeline in replay mode, markers and
// the log summary.
class TestReplay : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_log;            // 10 s, 1 frame every 1 ms (exactly 10,001 frames)
    QString m_vehicleLog;     // 20 s of demo-vehicle traffic
    qint64 m_now = 0;         // fake clock, microseconds

    struct Collector
    {
        std::vector<CanFrame> frames;
        void attach(ReplaySource &src)
        {
            QObject::connect(&src, &FrameSource::framesReady, [this](const CanFrameBatch &b) {
                frames.insert(frames.end(), b.cbegin(), b.cend());
            });
        }
    };

    void makeSource(ReplaySource &src)
    {
        src.setTimerDriven(false);
        src.setClock([this] { return m_now; });
        QVERIFY(src.open(m_log));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        m_log = m_dir.filePath(QStringLiteral("ms.canlog"));
        canlog::Writer w;
        QVERIFY(w.open(m_log, canlog::Header{}));
        for (int i = 0; i <= 10000; ++i) {
            CanFrame f;
            f.id = 0x100 + static_cast<std::uint32_t>(i % 3);
            f.dlc = 2;
            f.data[0] = static_cast<std::uint8_t>(i & 0xFF);
            f.timestampUs = i * 1000;
            w.append(f);
            if (i % 250 == 0)
                w.flush();
        }
        QVERIFY(w.close());

        m_vehicleLog = m_dir.filePath(QStringLiteral("vehicle.canlog"));
        SimulatorEngine engine(SimulatorEngine::kDefaultSeed, SimulatorEngine::Mode::Vehicle);
        engine.setTargetRate(1000);
        std::vector<CanFrame> frames;
        engine.advanceTo(20'000'000, frames);
        QVERIFY(w.open(m_vehicleLog, canlog::Header{}));
        w.append(frames.data(), frames.size());
        QVERIFY(w.close());
    }

    void realTimeSpeeds_data()
    {
        QTest::addColumn<double>("speed");
        QTest::newRow("0.25x") << 0.25;
        QTest::newRow("1x") << 1.0;
        QTest::newRow("2x") << 2.0;
        QTest::newRow("8x") << 8.0;
    }

    void realTimeSpeeds()
    {
        QFETCH(double, speed);
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        src.setSpeed(speed);
        src.start();
        // 100 ticks of 10 ms wall time = 1 s wall.
        for (int i = 0; i < 100; ++i) {
            m_now += 10'000;
            src.tick();
        }
        const std::int64_t expectedUs = static_cast<std::int64_t>(1'000'000 * speed);
        QCOMPARE(src.positionUs(), expectedUs);
        // Frames are 1 ms apart starting at 0: all with t <= position delivered.
        QCOMPARE(static_cast<std::int64_t>(c.frames.size()), expectedUs / 1000 + 1);
        QCOMPARE(c.frames.back().timestampUs, expectedUs);
        // Order and timestamps are preserved.
        for (std::size_t i = 0; i < c.frames.size(); ++i)
            QCOMPARE(c.frames[i].timestampUs, static_cast<std::int64_t>(i) * 1000);
    }

    void irregularTicksDoNotDrift()
    {
        ReplaySource src;
        makeSource(src);
        src.setSpeed(1.5);
        src.start();
        const int steps[] = {3, 17, 9, 1, 40, 12, 7, 25};
        qint64 wall = 0;
        for (int round = 0; round < 20; ++round)
            for (int ms : steps) {
                m_now += ms * 1000;
                wall += ms * 1000;
                src.tick();
            }
        QCOMPARE(src.positionUs(), static_cast<qint64>(wall * 1.5));
    }

    void pauseFreezesPosition()
    {
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        src.start();
        m_now += 200'000;
        src.tick();
        src.stop();
        const auto pos = src.positionUs();
        const auto n = c.frames.size();
        m_now += 5'000'000; // a long pause
        src.tick();
        QCOMPARE(src.positionUs(), pos);
        QCOMPARE(c.frames.size(), n);
        src.start(); // resume: the pause must not count
        m_now += 100'000;
        src.tick();
        QCOMPARE(src.positionUs(), pos + 100'000);
    }

    void speedChangeKeepsPosition()
    {
        ReplaySource src;
        makeSource(src);
        src.start();
        m_now += 1'000'000;
        src.tick();
        QCOMPARE(src.positionUs(), 1'000'000);
        src.setSpeed(4);
        m_now += 500'000;
        src.tick();
        QCOMPARE(src.positionUs(), 3'000'000);
        src.setSpeed(0.25);
        m_now += 400'000;
        src.tick();
        QCOMPARE(src.positionUs(), 3'100'000);
    }

    void asFastAsPossible()
    {
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        QSignalSpy end(&src, &ReplaySource::endReached);
        src.setSpeed(0);
        QCOMPARE(src.speed(), 0.0);
        src.start();
        src.tick(); // no time passes, frames still flow
        QCOMPARE(static_cast<int>(c.frames.size()), ReplaySource::kAfapFramesPerTick);
        for (int i = 0; i < 10 && src.isRunning(); ++i)
            src.tick();
        QCOMPARE(c.frames.size(), 10001u);
        QCOMPARE(end.count(), 1);
        QVERIFY(!src.isRunning());
        QVERIFY(src.atEnd());
        QCOMPARE(src.positionUs(), 10'000'000);
    }

    void stepFrameDeliversOne()
    {
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        src.seek(2'000'000, 0);
        src.stepFrame();
        QCOMPARE(c.frames.size(), 1u);
        QCOMPARE(c.frames[0].timestampUs, 2'001'000);
        QCOMPARE(src.positionUs(), 2'001'000);
        src.stepFrame();
        QCOMPARE(c.frames.size(), 2u);
        QCOMPARE(src.frameIndex(), 2003);
        QVERIFY(!src.isRunning());
    }

    void seekWithPreroll()
    {
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        std::vector<CanFrame> pre;
        QSignalSpy disc(&src, &ReplaySource::discontinuity);
        connect(&src, &ReplaySource::prerollReady, this, [&](const CanFrameBatch &b) { pre.assign(b.cbegin(), b.cend()); });
        src.seek(6'000'000, 2'000'000);
        QCOMPARE(disc.count(), 1);
        QCOMPARE(pre.size(), 2001u);                  // 4.000 .. 6.000 s inclusive
        QCOMPARE(pre.front().timestampUs, 4'000'000);
        QCOMPARE(pre.back().timestampUs, 6'000'000);
        QCOMPARE(src.positionUs(), 6'000'000);
        QCOMPARE(src.frameIndex(), 6001);
        src.start();
        m_now += 10'000;
        src.tick();
        QCOMPARE(c.frames.front().timestampUs, 6'001'000); // continues right after
        QCOMPARE(c.frames.back().timestampUs, 6'010'000);
        // Out of range targets are clamped.
        src.seek(99'000'000, 0);
        QVERIFY(src.atEnd());
        src.seek(-5, 0);
        QCOMPARE(src.positionUs(), 0);
    }

    void loopRestarts()
    {
        ReplaySource src;
        makeSource(src);
        Collector c;
        c.attach(src);
        QSignalSpy disc(&src, &ReplaySource::discontinuity);
        src.setLoop(true);
        src.setSpeed(8);
        src.seek(9'000'000, 0);
        src.start();
        m_now += 200'000; // 1.6 s of log time: reaches the end
        src.tick();
        QVERIFY(src.isRunning());
        QCOMPARE(disc.count(), 2); // seek + loop
        QCOMPARE(src.frameIndex(), 0);
        m_now += 10'000;
        src.tick();
        QCOMPARE(src.positionUs(), 80'000);
    }

    void playAtEndStartsOver()
    {
        ReplaySource src;
        makeSource(src);
        src.seek(src.endUs(), 0);
        QVERIFY(src.atEnd());
        src.start();
        QCOMPARE(src.positionUs(), 0);
        QVERIFY(src.isRunning());
    }

    void sessionReplaysThroughTheSamePipeline()
    {
        auto *sim = new SimulatedSource(SimulatorEngine::kDefaultSeed, SimulatorEngine::Mode::Vehicle);
        CaptureSession session(sim);
        session.setManualTime(true);
        session.database()->loadDemo();
        QVERIFY(session.openLogPath(m_vehicleLog));
        QVERIFY(session.replayMode());
        QVERIFY(session.isRunning());
        // 2 s of manual time at 1x.
        for (int ms = 0; ms <= 2000; ms += 30)
            session.manualTick(ms);
        const int rows = session.frameModel()->count();
        QVERIFY(rows > 1500);
        QVERIFY(session.frameModel()->frameAt(rows - 1).timestampUs <= 2'000'000);
        const int speedId = session.signalModel()->signalIdByName(QStringLiteral("VehicleDynamics"), QStringLiteral("VehicleSpeed"));
        QVERIFY(speedId >= 0);
        QVERIFY(session.timeSeries()->series(speedId) && session.timeSeries()->series(speedId)->size() > 100);

        // Seek: views are rebuilt with 10 s of pre-roll before the target.
        session.seekReplay(15'000'000);
        const int afterSeek = session.frameModel()->count();
        QVERIFY(afterSeek > 5000);
        QVERIFY(session.frameModel()->frameAt(0).timestampUs >= 5'000'000);
        QVERIFY(session.frameModel()->frameAt(afterSeek - 1).timestampUs <= 15'000'000);

        // Pause (Space) pauses the playback in replay mode.
        session.togglePause();
        QVERIFY(!session.isRunning());
        session.togglePause();
        QVERIFY(session.isRunning());

        // Markers through the session.
        session.addMarker(QStringLiteral("Hard braking"));
        QCOMPARE(session.markers()->count(), 1);
        session.seekReplay(1'000'000);
        QVERIFY(session.jumpToMarker(true));
        QCOMPARE(session.replay()->positionUs(), 15'000'000);
        QVERIFY(QFile::exists(MarkerStore::sidecarPathFor(m_vehicleLog)));

        // Search on replayed frames.
        session.setSearchText(QStringLiteral("VehicleSpeed > 0"));
        QVERIFY(session.searchValid());
        QVERIFY(session.findNext(-1, true) >= 0);

        session.closeLog();
        QVERIFY(!session.replayMode());
        QCOMPARE(session.frameModel()->count(), 0);
        QFile::remove(MarkerStore::sidecarPathFor(m_vehicleLog));
    }

    void sessionRecordsThenReplays()
    {
        auto *sim = new SimulatedSource(SimulatorEngine::kDefaultSeed, SimulatorEngine::Mode::Vehicle);
        CaptureSession session(sim);
        session.setManualTime(true);
        session.setTargetRate(1000);
        const QString path = m_dir.filePath(QStringLiteral("rec/recorded.canlog"));
        QVERIFY(session.startRecordingPath(path)); // also starts the source
        QVERIFY(session.isRunning());
        for (int ms = 10; ms <= 3000; ms += 10) {
            sim->stepTo(static_cast<std::int64_t>(ms) * 1000);
            session.manualTick(ms);
        }
        session.stopRecording();
        canlog::Reader r;
        QVERIFY(r.open(path));
        QVERIFY(r.frameCount() > 2500);
        QVERIFY(r.durationUs() > 2'900'000);
        QVERIFY(r.header().application.startsWith(QStringLiteral("CAN Analyzer")));
        QVERIFY(session.openLogPath(path));
        QCOMPARE(session.replay()->frameCount(), r.frameCount());
        // Recording is refused in replay mode.
        QVERIFY(!session.startRecordingPath(m_dir.filePath(QStringLiteral("x.canlog"))));
    }

    void csvImportOpensInReplay()
    {
        const QString csv = m_dir.filePath(QStringLiteral("frames.csv"));
        QVERIFY(canlog::exportCsv(m_log, csv).ok);
        auto *sim = new SimulatedSource;
        CaptureSession session(sim);
        session.setManualTime(true);
        QVERIFY(session.openLogPath(csv));
        QCOMPARE(session.replay()->frameCount(), 10001);
        QCOMPARE(session.markers()->sidecarPath(), MarkerStore::sidecarPathFor(csv));
        QSignalSpy notices(&session, &CaptureSession::notice);
        QVERIFY(!session.openLogPath(m_dir.filePath(QStringLiteral("missing.canlog"))));
        QCOMPARE(notices.count(), 1);
    }

    void markerStore()
    {
        QTemporaryDir dir;
        const QString log = dir.filePath(QStringLiteral("m.canlog"));
        MarkerStore m;
        QVERIFY(m.attach(log));
        QCOMPARE(m.count(), 0);
        QCOMPARE(m.add(5'000'000, QStringLiteral("B")), 0);
        QCOMPARE(m.add(1'000'000, QString()), 0);       // sorted insert, default name
        QCOMPARE(m.add(9'000'000, QStringLiteral("  C ")), 2);
        QCOMPARE(m.markers()[0].name, QStringLiteral("Marker 2"));
        QCOMPARE(m.markers()[2].name, QStringLiteral("C"));
        QCOMPARE(m.nextAfter(1'000'000), 5'000'000);
        QCOMPARE(m.nextAfter(9'000'000), -1);
        QCOMPARE(m.previousBefore(5'000'000), 1'000'000);
        QCOMPARE(m.previousBefore(1'000'000), -1);
        m.rename(1, QStringLiteral("Brake"));
        m.remove(0);
        QCOMPARE(m.count(), 2);
        // Saved automatically; a new store loads the sidecar.
        MarkerStore m2;
        QVERIFY(m2.attach(log));
        QCOMPARE(m2.count(), 2);
        QCOMPARE(m2.markers()[0].name, QStringLiteral("Brake"));
        QCOMPARE(m2.markers()[0].timeUs, 5'000'000);
        QCOMPARE(m2.add(7'000'000), 1);
        QCOMPARE(m2.markers()[1].name, QStringLiteral("Marker 3"));

        // A damaged sidecar is reported, not fatal.
        QFile f(MarkerStore::sidecarPathFor(log));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write("{ not json");
        f.close();
        MarkerStore m3;
        QVERIFY(!m3.attach(log));
        QVERIFY(!m3.lastError().isEmpty());
        QCOMPARE(m3.count(), 0);
    }

    void logSummary()
    {
        auto s = LogSummary::compute(m_log, nullptr);
        QVERIFY(s.ok);
        QCOMPARE(s.frames, 10001);
        QCOMPARE(s.durationUs(), 10'000'000);
        QCOMPARE(s.ids.size(), 3u);
        QCOMPARE(s.ids[0].id, 0x100u);
        QCOMPARE(s.ids[0].count, 3334);
        QCOMPARE(s.ids[0].minPeriodUs, 3000);
        QCOMPARE(s.ids[0].maxPeriodUs, 3000);
        QCOMPARE(s.ids[0].meanPeriodUs(), 3000.0);
        QCOMPARE(s.averageRate, 1000.1);
        // 47 + 16 bits per frame (standard, 2 bytes) = 63; 1000 fr/s at 500 kbit/s.
        QVERIFY(qAbs(s.averageLoad - 63.0 * 10001 / 10.0 / 500000) < 1e-9);
        QVERIFY(s.peakLoad >= s.averageLoad * 0.99);

        auto v = LogSummary::compute(m_vehicleLog, demoDatabase());
        QVERIFY(v.ok);
        QCOMPARE(v.ids.front().name, QStringLiteral("VehicleDynamics"));
        QTemporaryDir dir;
        const QString csv = dir.filePath(QStringLiteral("summary.csv"));
        QString error;
        QVERIFY(v.writeCsv(csv, &error));
        QFile f(csv);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray text = f.readAll();
        QVERIFY(text.contains("frames,"));
        QVERIFY(text.contains("0C8,std,VehicleDynamics,"));
        QCOMPARE(v.toVariant().value(QStringLiteral("idCount")).toInt(), static_cast<int>(v.ids.size()));
        QVERIFY(!LogSummary::compute(dir.filePath(QStringLiteral("none.canlog")), nullptr).ok);
    }
};

QTEST_GUILESS_MAIN(TestReplay)
#include "tst_replay.moc"
