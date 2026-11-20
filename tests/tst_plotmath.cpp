#include "plotcontroller.h"
#include "plotmath.h"
#include "timeseriesstore.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace can;
using namespace can::plotmath;

class TstPlotMath : public QObject
{
    Q_OBJECT
private slots:
    void niceSteps()
    {
        QCOMPARE(niceStep(10.0, 5), 2.0);
        QCOMPARE(niceStep(100.0, 5), 20.0);
        QCOMPARE(niceStep(7.0, 5), 1.0);
        QCOMPARE(niceStep(1.0, 4), 0.2);
        QCOMPARE(niceStep(0.03, 3), 0.01);
        const auto t = niceTicks(-3.2, 7.9, 5);
        QCOMPARE(t, (std::vector<double>{-2, 0, 2, 4, 6}));
        double step = 0;
        const auto t2 = niceTicks(12.0, 22.0, 5, &step);
        QCOMPARE(step, 2.0);
        QCOMPARE(t2.front(), 12.0);
        QCOMPARE(t2.back(), 22.0);
        QVERIFY(niceTicks(5, 5, 4).empty());
    }

    void paddedRanges()
    {
        auto [a, b] = paddedRange(0, 100);
        QCOMPARE(a, -8.0);
        QCOMPARE(b, 108.0);
        std::tie(a, b) = paddedRange(42, 42); // flat signal gets a visible range
        QVERIFY(a < 42 && b > 42);
        std::tie(a, b) = paddedRange(10, -10); // swapped input
        QVERIFY(a < -10 && b > 10);
    }

    void zoomKeepsAnchor()
    {
        const TimeRange r{0, 10'000'000};
        // Zoom in 2x around 25 %: the time at 25 % stays at 25 %.
        const TimeRange z = zoomAt(r, 0.25, 0.5, 1000, 100'000'000);
        QCOMPARE(z.span(), std::int64_t(5'000'000));
        QCOMPARE(z.t0 + z.span() / 4, std::int64_t(2'500'000));
        // Clamped to the minimum span.
        const TimeRange tiny = zoomAt(r, 0.5, 1e-9, 20'000, 100'000'000);
        QCOMPARE(tiny.span(), std::int64_t(20'000));
        QCOMPARE(tiny.t0 + tiny.span() / 2, std::int64_t(5'000'000));
        // And to the maximum span.
        QCOMPARE(zoomAt(r, 1.0, 1000, 1000, 60'000'000).span(), std::int64_t(60'000'000));
    }

    void panAndClamp()
    {
        const TimeRange r{10'000'000, 20'000'000};
        QCOMPARE(panBy(r, 0.5).t0, std::int64_t(15'000'000));
        QCOMPARE(panBy(r, -1.0).t1, std::int64_t(10'000'000));
        const TimeRange c = clampTo({-5'000'000, 5'000'000}, 0, 100'000'000);
        QCOMPARE(c.t0, std::int64_t(0));
        QCOMPARE(c.span(), std::int64_t(10'000'000));
        QCOMPARE(clampTo({95'000'000, 105'000'000}, 0, 100'000'000).t1, std::int64_t(100'000'000));
    }

    void pixelMapping()
    {
        const TimeRange r{1'000'000, 3'000'000};
        QCOMPARE(toPixel(2'000'000, r, 800), 400.0);
        QCOMPARE(toTime(400, r, 800), std::int64_t(2'000'000));
        QCOMPARE(toTime(0, r, 800), std::int64_t(1'000'000));
    }

    void cursorDeltas()
    {
        CursorDelta d = cursorDelta(1'000'000, 1'250'000);
        QCOMPARE(d.dtSeconds, 0.25);
        QCOMPARE(d.frequencyHz, 4.0);
        d = cursorDelta(2'000'000, 1'000'000); // B before A: negative dt, positive frequency
        QCOMPARE(d.dtSeconds, -1.0);
        QCOMPARE(d.frequencyHz, 1.0);
        QCOMPARE(cursorDelta(5, 5).frequencyHz, 0.0);
    }

    void labels()
    {
        QCOMPARE(formatSeconds(12.5, 0.5), QStringLiteral("12.5 s"));
        QCOMPARE(formatSeconds(12, 2), QStringLiteral("12 s"));
        QCOMPARE(formatValue(0.30000000000000004, 0.1), QStringLiteral("0.3"));
        QCOMPARE(formatValue(-1e-17, 0.5), QStringLiteral("0.0"));
    }

    // --- controller ----------------------------------------------------------
    void followLiveScrollsSmoothly()
    {
        TimeSeriesStore store;
        store.reset(1);
        PlotController pc;
        pc.setStore(&store);
        QVERIFY(pc.followLive());
        // Data arrives in 30 ms batches; the display advances every 16.7 ms.
        double lastEnd = -1;
        std::int64_t t = 0;
        for (int frame = 0; frame < 300; ++frame) {
            if (frame % 2 == 0) {
                for (int i = 0; i < 30; ++i, t += 1000)
                    store.append(0, t, 1.0);
                store.commit();
            }
            pc.advance(1.0 / 60.0);
            QVERIFY(pc.viewEndUs() <= store.nowUs());   // never ahead of the data
            QVERIFY(pc.viewEndUs() >= lastEnd);         // never jumps backwards
            QCOMPARE(pc.viewEndUs() - pc.viewStartUs(), 10e6);
            lastEnd = pc.viewEndUs();
        }
        QVERIFY(store.nowUs() - pc.viewEndUs() < 80'000);
    }

    void zoomPanLeaveFollowAndResume()
    {
        TimeSeriesStore store;
        store.reset(1);
        for (int i = 0; i <= 60'000; ++i)
            store.append(0, std::int64_t(i) * 1000, i);
        store.commit();
        PlotController pc;
        pc.setStore(&store);
        pc.follow();
        pc.advance(0.016);
        pc.zoomAt(1.0, 0.5);
        QVERIFY(!pc.followLive());
        QCOMPARE(pc.viewEndUs() - pc.viewStartUs(), 5e6);
        const double before = pc.viewStartUs();
        pc.panByFraction(-0.5);
        QCOMPARE(pc.viewStartUs(), before - 2.5e6);
        pc.panByFraction(-100); // clamped to the start of the data
        QCOMPARE(pc.viewStartUs(), 0.0);
        pc.setWindowSeconds(30);
        pc.follow();
        pc.advance(0.016);
        QVERIFY(pc.followLive());
        QCOMPARE(pc.viewEndUs() - pc.viewStartUs(), 30e6);
        pc.resetView();
        QCOMPARE(pc.windowSeconds(), PlotController::kDefaultWindowSeconds);
    }

    void cursorsAndTicks()
    {
        PlotController pc;
        pc.setView(0, 9e6);
        pc.placeCursors();
        QVERIFY(pc.hasCursors());
        QCOMPARE(pc.cursorAUs(), 3e6);
        QCOMPARE(pc.cursorBUs(), 6e6);
        const QVariantMap d = pc.cursorDelta();
        QCOMPARE(d.value(QStringLiteral("dtSeconds")).toDouble(), 3.0);
        QCOMPARE(d.value(QStringLiteral("freqText")).toString(), QStringLiteral("333.3 mHz"));
        pc.setCursor(1, 3.02e6);
        QCOMPARE(pc.cursorDelta().value(QStringLiteral("dtText")).toString(), QStringLiteral("20.0 ms"));
        QCOMPARE(pc.cursorDelta().value(QStringLiteral("freqText")).toString(), QStringLiteral("50.00 Hz"));
        pc.clearCursors();
        QVERIFY(!pc.hasCursors());

        const QVariantList ticks = pc.timeTicks(900);
        QVERIFY(ticks.size() >= 4);
        QCOMPARE(ticks.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("0 s"));
        const QVariantList vt = pc.valueTicks(0, 100, 200);
        QVERIFY(vt.size() >= 3);
        QCOMPARE(vt.first().toMap().value(QStringLiteral("y")).toDouble(), 200.0); // value 0 at the bottom
    }

    void csvExport()
    {
        TimeSeriesStore store;
        store.reset(2);
        for (int i = 0; i < 10; ++i) {
            store.append(0, i * 100'000, i);
            if (i % 2 == 0)
                store.append(1, i * 100'000, i * 10.0);
        }
        store.commit();
        PlotController pc;
        pc.setStore(&store);
        pc.setView(200'000, 500'000);
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("view.csv"));
        QVERIFY(pc.writeCsv(path, {0, 1}, {QStringLiteral("Speed"), QStringLiteral("Rpm")}));
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QCOMPARE(lines.first(), QStringLiteral("time_s,Speed,Rpm"));
        QCOMPARE(lines.size(), 1 + 4); // samples at 0.2, 0.3, 0.4, 0.5 s
        QCOMPARE(lines[1], QStringLiteral("0.200000,2,20"));
        QCOMPARE(lines[2], QStringLiteral("0.300000,3,"));
        // Friendly error instead of a crash.
        QVERIFY(!pc.writeCsv(dir.filePath(QStringLiteral("no/such/dir/x.csv")), {0}, {}));
        QVERIFY(!pc.lastError().isEmpty());
        QVERIFY(!pc.writeCsv(path, {}, {}));
    }
};

QTEST_GUILESS_MAIN(TstPlotMath)
#include "tst_plotmath.moc"
