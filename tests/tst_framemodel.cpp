#include "framemodel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest>

using namespace can;

namespace {
std::vector<CanFrame> frames(int n, std::uint32_t idBase = 0x100, std::int64_t t0 = 0)
{
    std::vector<CanFrame> out;
    for (int i = 0; i < n; ++i) {
        CanFrame f;
        f.id = idBase + static_cast<std::uint32_t>(i % 4);
        f.dlc = 8;
        f.data[0] = static_cast<std::uint8_t>(i);
        f.timestampUs = t0 + i * 1000;
        out.push_back(f);
    }
    return out;
}
} // namespace

class TstFrameModel : public QObject
{
    Q_OBJECT
private slots:
    void appendsRows()
    {
        FrameModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy inserted(&m, &QAbstractItemModel::rowsInserted);
        m.append(frames(10));
        QCOMPARE(m.rowCount(), 10);
        QCOMPARE(inserted.count(), 1); // one batch -> one insert notification
        QCOMPARE(m.columnCount(), int(FrameModel::ColumnCount));
        QCOMPARE(m.data(m.index(0, FrameModel::IdColumn)).toString(), QStringLiteral("100"));
        QCOMPARE(m.data(m.index(3, FrameModel::TimeColumn)).toString(), QStringLiteral("0.003000"));
        QCOMPARE(m.data(m.index(0, 0), FrameModel::FreshRole).toBool(), true);
        m.append(frames(1, 0x200, 99000));
        QCOMPARE(m.data(m.index(0, 0), FrameModel::FreshRole).toBool(), false);
        QCOMPARE(m.data(m.index(10, 0), FrameModel::FreshRole).toBool(), true);
    }

    void evictsOldestWhenFull()
    {
        FrameModel m;
        m.setCapacity(8);
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy evicted(&m, &FrameModel::rowsEvicted);
        m.append(frames(6));
        m.append(frames(5, 0x100, 6000));
        QCOMPARE(m.rowCount(), 8);
        QCOMPARE(m.bufferedCount(), 8);
        QCOMPARE(evicted.count(), 1);
        QCOMPARE(evicted.at(0).at(0).toInt(), 3);
        // Oldest remaining frame is the 4th of the first batch (t = 3 ms).
        QCOMPARE(m.frameAt(0).timestampUs, 3000);
        QCOMPARE(m.frameAt(7).timestampUs, 10000);
        // Ordinals continue across eviction (stable zebra striping).
        QCOMPARE(m.data(m.index(0, 0), FrameModel::OrdinalRole).toULongLong(), 3ull);
    }

    void batchLargerThanCapacity()
    {
        FrameModel m;
        m.setCapacity(5);
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.append(frames(3));
        m.append(frames(12, 0x100, 100000));
        QCOMPARE(m.rowCount(), 5);
        QCOMPARE(m.frameAt(0).timestampUs, 100000 + 7 * 1000);
        QCOMPARE(m.frameAt(4).timestampUs, 100000 + 11 * 1000);
    }

    void filterAppliesToExistingAndNewFrames()
    {
        FrameModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.append(frames(20)); // ids 100..103 round robin
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("101")));
        m.setFilter(f);
        QCOMPARE(m.rowCount(), 5);
        for (int r = 0; r < m.rowCount(); ++r)
            QCOMPARE(m.frameAt(r).id, 0x101u);
        m.append(frames(8, 0x100, 50000));
        QCOMPARE(m.rowCount(), 7);
        QCOMPARE(m.bufferedCount(), 28);
        m.setFilter(FrameFilter{});
        QCOMPARE(m.rowCount(), 28);
    }

    void filteredEviction()
    {
        FrameModel m;
        m.setCapacity(10);
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("100")));
        m.setFilter(f);
        m.append(frames(10)); // visible: t=0,4,8 ms
        QCOMPARE(m.rowCount(), 3);
        m.append(frames(5, 0x101, 20000)); // ids 101..104, one 0x100 never; evicts first 5
        // t=0 and t=4 ms evicted; t=8 ms stays.
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.frameAt(0).timestampUs, 8000);
    }

    void deltaPerId()
    {
        FrameModel m;
        m.append(frames(8)); // ids 100..103 round robin, 1 ms apart
        QCOMPARE(m.deltaAt(0), std::int64_t(-1));   // first 0x100
        QCOMPARE(m.deltaAt(3), std::int64_t(-1));   // first 0x103
        QCOMPARE(m.deltaAt(4), std::int64_t(4000)); // second 0x100, 4 ms later
        QCOMPARE(m.data(m.index(4, FrameModel::DeltaColumn)).toString(), QStringLiteral("4.000"));
        QCOMPARE(m.data(m.index(0, FrameModel::DeltaColumn)).toString(), QStringLiteral("—"));
        // Delta survives filtering (it is computed on arrival, over all frames).
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("101")));
        m.setFilter(f);
        QCOMPARE(m.deltaAt(1), std::int64_t(4000));
        m.setFilter(FrameFilter{});
        m.clear();
        m.append(frames(1, 0x100, 50000));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.deltaAt(0), std::int64_t(-1)); // history reset by clear()
    }

    void rowForSeq()
    {
        FrameModel m;
        m.append(frames(4));
        const auto seq = m.data(m.index(2, 0), FrameModel::SeqRole).toULongLong();
        QCOMPARE(m.rowForSeq(seq), 2);
        QCOMPARE(m.rowForSeq(9999), -1);
    }

    void clearResets()
    {
        FrameModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.append(frames(10));
        m.clear();
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.bufferedCount(), 0);
        m.append(frames(2));
        QCOMPARE(m.rowCount(), 2);
    }

    void throughput()
    {
        // 100k-frame buffer at 2000 fps for 2 minutes of traffic in 30 ms batches.
        FrameModel m;
        std::int64_t t = 0;
        QElapsedTimer timer;
        timer.start();
        for (int batch = 0; batch < 4000; ++batch) {
            auto b = frames(60, 0x100, t);
            t += 60000;
            m.append(b);
        }
        QCOMPARE(m.rowCount(), FrameModel::kDefaultCapacity);
        qInfo("appended 240000 frames in %lld ms", timer.elapsed());
        QVERIFY(timer.elapsed() < 5000);
    }
};

QTEST_GUILESS_MAIN(TstFrameModel)
#include "tst_framemodel.moc"
