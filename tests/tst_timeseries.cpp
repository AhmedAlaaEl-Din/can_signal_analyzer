#include "timeseriesstore.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtTest>

#include <cmath>

using namespace can;

namespace {
// 1 kHz sine with optional single-sample spike.
TimeSeries makeSeries(int n, std::int64_t dtUs = 1000, int spikeAt = -1, double spike = 100.0)
{
    TimeSeries s;
    for (int i = 0; i < n; ++i)
        s.append(std::int64_t(i) * dtUs, i == spikeAt ? spike : std::sin(i * 0.01));
    return s;
}
} // namespace

class TstTimeSeries : public QObject
{
    Q_OBJECT
private slots:
    void appendAndLookup()
    {
        TimeSeries s = makeSeries(1000);
        QCOMPARE(s.size(), std::size_t(1000));
        QCOMPARE(s.firstTime(), std::int64_t(0));
        QCOMPARE(s.lastTime(), std::int64_t(999000));
        QCOMPARE(s.lowerBound(500000), std::size_t(500));
        QCOMPARE(s.lowerBound(500001), std::size_t(501));
        QCOMPARE(s.lowerBound(-5), std::size_t(0));
        QCOMPARE(s.lowerBound(10'000'000), std::size_t(1000));
        double v = 0;
        std::int64_t at = 0;
        QVERIFY(s.valueAt(500500, &v, &at)); // between samples: the earlier one
        QCOMPARE(at, std::int64_t(500000));
        QVERIFY(!s.valueAt(-1, &v));
    }

    void spikePreservedAtEveryZoom_data()
    {
        QTest::addColumn<int>("columns");
        QTest::newRow("1 column") << 1;
        QTest::newRow("7 columns") << 7;
        QTest::newRow("300 columns") << 300;
        QTest::newRow("1200 columns") << 1200;
        QTest::newRow("more columns than samples") << 200000;
    }
    void spikePreservedAtEveryZoom()
    {
        QFETCH(int, columns);
        // 100k samples, one spike at a sample that is not block-aligned.
        const TimeSeries s = makeSeries(100'000, 1000, 54'321, 1000.0);
        const auto cols = s.decimate(0, 99'999'000, columns);
        QCOMPARE(int(cols.size()), columns);
        double best = -1e300;
        int total = 0;
        for (const auto &c : cols) {
            total += c.count;
            if (c.count)
                best = std::max(best, c.max);
        }
        QCOMPARE(best, 1000.0);   // the spike survives
        QCOMPARE(total, 100'000); // every sample is accounted for exactly once
    }

    void negativeSpikeToo()
    {
        const TimeSeries s = makeSeries(50'000, 1000, 12'800, -500.0); // block-aligned index
        const auto cols = s.decimate(0, 49'999'000, 64);
        double worst = 1e300;
        for (const auto &c : cols)
            if (c.count)
                worst = std::min(worst, c.min);
        QCOMPARE(worst, -500.0);
    }

    void emptyAndOutsideRanges()
    {
        TimeSeries empty;
        auto cols = empty.decimate(0, 1000, 10);
        QCOMPARE(int(cols.size()), 10);
        for (const auto &c : cols)
            QCOMPARE(c.count, 0);

        const TimeSeries s = makeSeries(100); // 0 .. 99 ms
        cols = s.decimate(200'000, 300'000, 10); // entirely after the data
        for (const auto &c : cols)
            QCOMPARE(c.count, 0);
        cols = s.decimate(-500'000, -100'000, 10); // entirely before
        for (const auto &c : cols)
            QCOMPARE(c.count, 0);
        QVERIFY(s.decimate(0, 0, 10)[0].count == 0); // zero-length range
        QVERIFY(s.decimate(0, 1000, 0).empty());
    }

    void rangePartlyOutside()
    {
        const TimeSeries s = makeSeries(100); // samples at 0..99 ms
        // Range -50 ms .. +50 ms in 10 columns of 10 ms: the first 5 are empty.
        const auto cols = s.decimate(-50'000, 49'999, 10);
        for (int c = 0; c < 5; ++c)
            QCOMPARE(cols[std::size_t(c)].count, 0);
        int total = 0;
        for (int c = 5; c < 10; ++c) {
            QCOMPARE(cols[std::size_t(c)].count, 10);
            total += cols[std::size_t(c)].count;
        }
        QCOMPARE(total, 50);
        // first/last values keep the line continuous
        QCOMPARE(cols[5].first, std::sin(0.0));
        QCOMPARE(cols[9].last, std::sin(49 * 0.01));
    }

    void ringWrapAndTrim()
    {
        TimeSeries s;
        // Fill, trim, refill many times: the ring wraps and blocks are dropped.
        std::int64_t t = 0;
        for (int round = 0; round < 20; ++round) {
            for (int i = 0; i < 5000; ++i, t += 1000)
                s.append(t, (i == 2500 && round == 19) ? 777.0 : 1.0);
            s.trimBefore(t - 3'000'000); // keep 3 s
        }
        QVERIFY(s.size() <= 3001);
        QCOMPARE(s.firstTime(), t - 3'000'000);
        QVERIFY(s.capacity() <= 16384); // trimmed rings do not grow without bound
        // The last round's spike (2.5 s before the end) is found by decimation.
        const auto cols = s.decimate(s.firstTime(), s.lastTime(), 10);
        double best = 0;
        int total = 0;
        for (const auto &c : cols) {
            total += c.count;
            if (c.count)
                best = std::max(best, c.max);
        }
        QCOMPARE(best, 777.0);
        QCOMPARE(total, int(s.size()));
        // Old samples are really gone.
        double v = 0;
        QVERIFY(!s.valueAt(t - 3'500'000, &v));
    }

    void samplesIncludeNeighbours()
    {
        const TimeSeries s = makeSeries(100);
        std::vector<TimeSample> out;
        s.samples(10'500, 20'500, out); // samples 11..20 inside, neighbours 10 and 21
        QCOMPARE(int(out.size()), 12);
        QCOMPARE(out.front().t, std::int64_t(10'000));
        QCOMPARE(out.back().t, std::int64_t(21'000));
    }

    void storeHistoryAndRevision()
    {
        TimeSeriesStore store;
        store.setHistoryUs(2'000'000);
        store.reset(3);
        QSignalSpy rev(&store, &TimeSeriesStore::revisionChanged);
        for (int i = 0; i < 5000; ++i)
            store.append(i % 3, std::int64_t(i) * 1000, i);
        store.commit();
        QCOMPARE(rev.count(), 1);
        QCOMPARE(store.nowUs(), qint64(4'999'000));
        QVERIFY(store.series(0)->firstTime() >= 2'999'000);
        QVERIFY(store.valueAt(1, 4'000'000).isValid());
        QVERIFY(!store.valueAt(1, 1'000'000).isValid()); // trimmed
        QVERIFY(!store.valueAt(7, 0).isValid());         // unknown id
        store.commit();
        QCOMPARE(rev.count(), 1); // nothing new, no notification
        store.clear();
        QCOMPARE(int(store.totalSamples()), 0);
    }

    // Performance smoke test with a generous threshold: 8 signals x 10 minutes
    // at 100 Hz, decimated to 1200 columns, 60 times (one second of frames).
    void performanceSmoke()
    {
        std::vector<TimeSeries> series(8);
        for (auto &s : series)
            for (int i = 0; i < 60'000; ++i)
                s.append(std::int64_t(i) * 10'000, std::sin(i * 0.001));
        QElapsedTimer timer;
        timer.start();
        int columnsWithData = 0;
        for (int frame = 0; frame < 60; ++frame)
            for (const auto &s : series)
                for (const auto &c : s.decimate(0, 600'000'000, 1200))
                    columnsWithData += c.count > 0;
        const qint64 ms = timer.elapsed();
        qInfo("zoomed out (8 x 60k samples -> 1200 px) x 60 frames: %lld ms", ms);
        QCOMPARE(columnsWithData, 60 * 8 * 1200);
        QVERIFY2(ms < 1500, "decimation is far too slow");

        timer.restart();
        for (int frame = 0; frame < 60; ++frame)
            for (const auto &s : series)
                s.decimate(500'000'000, 600'000'000, 1200); // last 100 s: 10k points
        qInfo("10k visible points per signal x 8 x 60 frames: %lld ms", timer.elapsed());
        QVERIFY(timer.elapsed() < 1500);
    }
};

QTEST_GUILESS_MAIN(TstTimeSeries)
#include "tst_timeseries.moc"
