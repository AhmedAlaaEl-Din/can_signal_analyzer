#include "busstatistics.h"

#include <QtTest>

using namespace can;

namespace {
std::vector<CanFrame> make(int n, int dlc, bool ext = false)
{
    std::vector<CanFrame> v(static_cast<std::size_t>(n));
    for (auto &f : v) {
        f.dlc = static_cast<std::uint8_t>(dlc);
        f.extended = ext;
    }
    return v;
}
} // namespace

class TstStatistics : public QObject
{
    Q_OBJECT
private slots:
    void frameBits()
    {
        CanFrame f;
        f.dlc = 0;
        QCOMPARE(BusStatistics::frameBits(f), 47);
        f.dlc = 8;
        QCOMPARE(BusStatistics::frameBits(f), 111);
        f.extended = true;
        QCOMPARE(BusStatistics::frameBits(f), 131);
    }

    void rateAndLoadSteadyState()
    {
        BusStatistics s(500000);
        // 2000 frames/s of 8-byte standard frames, delivered every 30 ms.
        std::int64_t now = 0;
        for (int i = 0; i < 100; ++i) {
            now += 30;
            const auto b = make(60, 8);
            s.add(b.data(), b.size(), now);
        }
        QVERIFY(std::abs(s.framesPerSecond(now) - 2000.0) < 70.0);
        // 2000 * 111 bit = 222 kbit/s -> 44.4 % of 500 kbit/s
        QVERIFY(std::abs(s.busLoad(now) - 0.444) < 0.02);
        QCOMPARE(s.totalFrames(), 6000u);
    }

    void startupDoesNotUnderReport()
    {
        BusStatistics s;
        const auto b = make(100, 8);
        s.add(b.data(), b.size(), 0);
        s.add(b.data(), b.size(), 100); // 200 frames in the first 100 ms (~2000/s)
        QVERIFY(s.framesPerSecond(100) > 1500.0);
    }

    void decaysWhenIdle()
    {
        BusStatistics s;
        const auto b = make(50, 8);
        s.add(b.data(), b.size(), 0);
        s.add(b.data(), b.size(), 500);
        s.tick(2000);
        QCOMPARE(s.framesPerSecond(2000), 0.0);
        QCOMPARE(s.totalFrames(), 100u); // total is not windowed
    }

    void reset()
    {
        BusStatistics s;
        const auto b = make(10, 1);
        s.add(b.data(), b.size(), 10);
        s.reset();
        QCOMPARE(s.totalFrames(), 0u);
        QCOMPARE(s.framesPerSecond(20), 0.0);
    }
};

QTEST_GUILESS_MAIN(TstStatistics)
#include "tst_statistics.moc"
