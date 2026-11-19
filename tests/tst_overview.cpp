#include "busstatistics.h"
#include "messageoverviewmodel.h"

#include <QAbstractItemModelTester>
#include <QtTest>

using namespace can;

namespace {
CanFrame frame(std::uint32_t id, std::int64_t tUs, std::uint8_t b0 = 0, bool ext = false)
{
    CanFrame f;
    f.id = id;
    f.extended = ext;
    f.dlc = 4;
    f.data = {b0, 0x11, 0x22, 0x33};
    f.timestampUs = tUs;
    return f;
}
} // namespace

class TstOverview : public QObject
{
    Q_OBJECT
private slots:
    // --- period maths -------------------------------------------------------
    void periodNeedsTwoSamples()
    {
        PeriodEstimator p;
        QCOMPARE(p.periodMs(), 0.0);
        p.add(1000);
        QCOMPARE(p.periodMs(), 0.0);
        p.add(11000);
        QCOMPARE(p.periodMs(), 10.0);
    }

    void periodOfRegularSignal()
    {
        PeriodEstimator p;
        for (int i = 0; i < 100; ++i)
            p.add(i * 20000); // 20 ms
        QCOMPARE(p.periodMs(), 20.0);
    }

    void periodAveragesJitter()
    {
        PeriodEstimator p;
        std::int64_t t = 0;
        for (int i = 0; i < 400; ++i) {
            p.add(t);
            t += (i % 2) ? 49000 : 51000; // 50 ms +-1 ms
        }
        QVERIFY(std::abs(p.periodMs() - 50.0) < 0.2);
    }

    void periodFollowsRateChange()
    {
        PeriodEstimator p;
        std::int64_t t = 0;
        for (int i = 0; i < 50; ++i, t += 100000)
            p.add(t); // 100 ms
        for (int i = 0; i < 80; ++i, t += 10000)
            p.add(t); // then 10 ms
        QVERIFY(std::abs(p.periodMs() - 10.0) < 0.1);
    }

    // --- overview model -----------------------------------------------------
    void oneRowPerId()
    {
        MessageOverviewModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        std::vector<CanFrame> batch{frame(0x200, 0), frame(0x100, 1000), frame(0x200, 10000),
                                    frame(0x100, 21000), frame(0x200, 20000)};
        m.update(batch);
        QCOMPARE(m.rowCount(), 2);
        const auto *e200 = m.entryFor(0x200, false);
        QVERIFY(e200);
        QCOMPARE(e200->count, 3u);
        QCOMPARE(e200->period.periodMs(), 10.0);
        const auto *e100 = m.entryFor(0x100, false);
        QCOMPARE(e100->period.periodMs(), 20.0);
        // Sorted by ID ascending by default.
        QCOMPARE(m.data(m.index(0, MessageOverviewModel::IdColumn)).toString(), QStringLiteral("100"));
        QCOMPARE(m.data(m.index(1, MessageOverviewModel::PeriodColumn)).toString(), QStringLiteral("10.0"));
    }

    void standardAndExtendedAreDistinct()
    {
        MessageOverviewModel m;
        m.update(std::vector<CanFrame>{frame(0x123, 0), frame(0x123, 10, 0, true)});
        QCOMPARE(m.rowCount(), 2);
        QCOMPARE(m.data(m.index(1, MessageOverviewModel::IdColumn)).toString(), QStringLiteral("00000123"));
    }

    void changedBytesMask()
    {
        MessageOverviewModel m;
        m.update(std::vector<CanFrame>{frame(0x100, 0, 0x01)});
        m.update(std::vector<CanFrame>{frame(0x100, 10000, 0x02)});
        const int mask = m.data(m.index(0, 0), MessageOverviewModel::ChangedMaskRole).toInt();
        QCOMPARE(mask, 0x01); // only byte 0 changed
        const auto serial = m.data(m.index(0, 0), MessageOverviewModel::ChangeSerialRole).toULongLong();
        m.update(std::vector<CanFrame>{frame(0x100, 20000, 0x02)});
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::ChangedMaskRole).toInt(), 0);
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::ChangeSerialRole).toULongLong(), serial);
    }

    void sortingByCount()
    {
        MessageOverviewModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        std::vector<CanFrame> batch;
        for (int i = 0; i < 5; ++i)
            batch.push_back(frame(0x300, i * 1000));
        batch.push_back(frame(0x100, 0));
        for (int i = 0; i < 3; ++i)
            batch.push_back(frame(0x200, i * 1000));
        m.update(batch);
        m.sortBy(MessageOverviewModel::CountColumn); // descending first
        QVERIFY(!m.sortAscending());
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("300"));
        QCOMPARE(m.data(m.index(2, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("100"));
        m.sortBy(MessageOverviewModel::CountColumn); // toggles
        QVERIFY(m.sortAscending());
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("100"));
        // New traffic re-sorts automatically.
        std::vector<CanFrame> more;
        for (int i = 0; i < 10; ++i)
            more.push_back(frame(0x100, 10000 + i * 1000));
        m.update(more);
        QCOMPARE(m.data(m.index(2, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("100"));
    }

    void sortBucketRounding()
    {
        QCOMPARE(MessageOverviewModel::sortBucket(0), 0.0);
        QCOMPARE(MessageOverviewModel::sortBucket(7177), 7200.0);
        QCOMPARE(MessageOverviewModel::sortBucket(7178), 7200.0);
        QVERIFY(qFuzzyCompare(MessageOverviewModel::sortBucket(1.84), 1.8));
        QCOMPARE(MessageOverviewModel::sortBucket(183.2), 180.0);
    }

    void nearTiesKeepIdOrder()
    {
        // Two IDs whose counts differ by one must not swap places on every update.
        MessageOverviewModel m;
        std::vector<CanFrame> batch;
        for (int i = 0; i < 1000; ++i)
            batch.push_back(frame(0x300, i * 1000));
        for (int i = 0; i < 1001; ++i)
            batch.push_back(frame(0x200, i * 1000));
        m.update(batch);
        m.sortBy(MessageOverviewModel::CountColumn);
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("200"));
        m.update(std::vector<CanFrame>{frame(0x300, 2'000'000), frame(0x300, 2'001'000)});
        QCOMPARE(m.data(m.index(0, 0), MessageOverviewModel::IdRole).toString(), QStringLiteral("200"));
    }

    void clearEmpties()
    {
        MessageOverviewModel m;
        m.update(std::vector<CanFrame>{frame(0x100, 0)});
        m.clear();
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.entryFor(0x100, false), nullptr);
    }
};

QTEST_GUILESS_MAIN(TstOverview)
#include "tst_overview.moc"
