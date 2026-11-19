#include "simulatedsource.h"
#include "simulatorengine.h"

#include <QSignalSpy>
#include <QtTest>

#include <map>
#include <set>

using namespace can;

class TstSimulator : public QObject
{
    Q_OBJECT
private:
    static std::vector<CanFrame> run(std::uint64_t seed, std::int64_t endUs, std::int64_t stepUs,
                                     int rate = 0, bool bursts = false)
    {
        SimulatorEngine e(seed);
        e.setTargetRate(rate);
        e.setBurstsEnabled(bursts);
        std::vector<CanFrame> out;
        for (std::int64_t t = stepUs; t < endUs; t += stepUs)
            e.advanceTo(t, out);
        e.advanceTo(endUs, out);
        return out;
    }

private slots:
    void sameSeedSameFrames()
    {
        const auto a = run(42, 5'000'000, 10'000);
        const auto b = run(42, 5'000'000, 10'000);
        QVERIFY(!a.empty());
        QCOMPARE(a.size(), b.size());
        QVERIFY(a == b);
    }

    void chunkingDoesNotMatter()
    {
        // One big step vs. many tiny irregular steps must give identical output.
        const auto big = run(7, 3'000'000, 3'000'000, 1000, true);
        const auto small = run(7, 3'000'000, 1'337, 1000, true);
        QCOMPARE(big.size(), small.size());
        QVERIFY(big == small);
    }

    void differentSeedDiffers()
    {
        const auto a = run(1, 2'000'000, 10'000);
        const auto b = run(2, 2'000'000, 10'000);
        QVERIFY(a != b);
    }

    void timestampsAreOrderedAndInRange()
    {
        const auto frames = run(99, 2'000'000, 5'000, 2000, true);
        std::int64_t last = 0;
        for (const auto &f : frames) {
            QVERIFY(f.timestampUs >= last);
            QVERIFY(f.timestampUs <= 2'000'000);
            QVERIFY(f.isValid());
            last = f.timestampUs;
        }
    }

    void messageTableIsAsSpecified()
    {
        const auto &msgs = SimulatorEngine::messages();
        QCOMPARE(int(msgs.size()), 12);
        std::set<int> periods;
        std::set<std::uint32_t> keys;
        bool anyExtended = false;
        for (const auto &m : msgs) {
            periods.insert(m.periodMs);
            CanFrame f;
            f.id = m.id;
            f.extended = m.extended;
            keys.insert(f.key());
            anyExtended |= m.extended;
            QVERIFY(m.dlc >= 0 && m.dlc <= 8);
        }
        QCOMPARE(periods, (std::set<int>{10, 20, 50, 100, 500, 1000}));
        QCOMPARE(int(keys.size()), 12); // unique IDs
        QVERIFY(anyExtended);
    }

    void nominalRateAndPeriods()
    {
        // 10 s at nominal speed: each ID should appear ~10000/period times.
        const auto frames = run(5, 10'000'000, 10'000);
        std::map<std::uint32_t, int> counts;
        for (const auto &f : frames)
            ++counts[f.key()];
        for (const auto &m : SimulatorEngine::messages()) {
            CanFrame probe;
            probe.id = m.id;
            probe.extended = m.extended;
            const double expected = 10000.0 / m.periodMs;
            const int got = counts[probe.key()];
            QVERIFY2(std::abs(got - expected) <= std::max(2.0, expected * 0.03),
                     qPrintable(QStringLiteral("id %1: %2 vs %3").arg(m.id, 0, 16).arg(got).arg(expected)));
        }
        QVERIFY(std::abs(SimulatorEngine::rawNominalRate() - 366.0) < 0.01);
    }

    void targetRateIsHonoured()
    {
        for (int rate : {500, 1000, 2000}) {
            const auto frames = run(11, 4'000'000, 10'000, rate);
            const double fps = frames.size() / 4.0;
            QVERIFY2(std::abs(fps - rate) < rate * 0.03,
                     qPrintable(QStringLiteral("rate %1 -> %2").arg(rate).arg(fps)));
        }
    }

    void burstsAddFrames()
    {
        const auto plain = run(3, 6'000'000, 10'000, 0, false);
        const auto bursty = run(3, 6'000'000, 10'000, 0, true);
        int burstFrames = 0;
        for (const auto &f : bursty)
            burstFrames += (f.id == SimulatorEngine::kBurstId);
        QCOMPARE(burstFrames, 2 * SimulatorEngine::kBurstFrames); // bursts at 2.5 s and 5.0 s
        QCOMPARE(int(bursty.size() - plain.size()), burstFrames);
    }

    void payloadsChange()
    {
        const auto frames = run(8, 3'000'000, 10'000);
        std::map<std::uint32_t, std::set<std::array<std::uint8_t, 8>>> distinct;
        for (const auto &f : frames)
            distinct[f.key()].insert(f.data);
        int changing = 0;
        for (const auto &[key, set] : distinct)
            changing += set.size() > 1;
        QVERIFY(changing >= 10);
    }

    void threadedSourceDeliversBatches()
    {
        SimulatedSource source(123);
        source.setTargetRate(2000);
        QSignalSpy spy(&source, &FrameSource::framesReady);
        source.start();
        QVERIFY(source.isRunning());
        QTRY_VERIFY_WITH_TIMEOUT(spy.count() >= 5, 3000);
        source.stop();
        QVERIFY(!source.isRunning());
        qint64 total = 0;
        std::int64_t last = -1;
        for (const auto &args : spy) {
            const auto batch = args.at(0).value<CanFrameBatch>();
            QVERIFY(!batch.isEmpty());
            for (const auto &f : batch) {
                QVERIFY(f.timestampUs >= last);
                last = f.timestampUs;
            }
            total += batch.size();
        }
        QVERIFY(total > 0);
    }
};

QTEST_GUILESS_MAIN(TstSimulator)
#include "tst_simulator.moc"
