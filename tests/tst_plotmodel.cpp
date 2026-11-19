#include "capturesession.h"
#include "dbc.h"
#include "plotmodel.h"
#include "signaldatabase.h"
#include "signallistmodel.h"
#include "simulatedsource.h"
#include "timeseriesstore.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QtTest>

using namespace can;

namespace {
int idOf(const QString &message, const QString &signal)
{
    SignalListModel m;
    m.setDatabase(demoDatabase());
    return m.signalIdByName(message, signal);
}
} // namespace

class TstPlotModel : public QObject
{
    Q_OBJECT
private slots:
    void addRemoveSignals()
    {
        PlotModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setDatabase(demoDatabase());
        const int speed = idOf(QStringLiteral("VehicleDynamics"), QStringLiteral("VehicleSpeed"));
        const int rpm = idOf(QStringLiteral("EngineData"), QStringLiteral("EngineSpeed"));
        QCOMPARE(m.addSignal(speed), 0);      // new lane
        QCOMPARE(m.addSignal(rpm, 0), 0);     // same lane
        QCOMPARE(m.addSignal(rpm, 0), 0);     // duplicate is ignored
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.seriesCount(), 2);
        const QVariantList series = m.data(m.index(0), PlotModel::SeriesRole).toList();
        QCOMPARE(series.size(), 2);
        QCOMPARE(series[0].toMap().value(QStringLiteral("name")).toString(), QStringLiteral("VehicleSpeed"));
        QCOMPARE(series[0].toMap().value(QStringLiteral("unit")).toString(), QStringLiteral("km/h"));
        // Distinct colours; second series dashed as a second visual cue.
        QVERIFY(series[0].toMap().value(QStringLiteral("color")) != series[1].toMap().value(QStringLiteral("color")));
        QVERIFY(!series[0].toMap().value(QStringLiteral("dashed")).toBool());
        QVERIFY(series[1].toMap().value(QStringLiteral("dashed")).toBool());
        QVERIFY(m.containsSignal(rpm));

        m.toggleVisible(0, rpm);
        QVERIFY(!m.data(m.index(0), PlotModel::SeriesRole).toList()[1].toMap().value(QStringLiteral("visible")).toBool());
        QCOMPARE(m.visibleSignals(), QVariantList{speed});
        m.removeSignal(0, rpm);
        QCOMPARE(m.seriesCount(), 1);
        QVERIFY(!m.containsSignal(rpm));
        QCOMPARE(m.addSignal(-5), -1);
    }

    void enumSignalsCarryLabels()
    {
        PlotModel m;
        m.setDatabase(demoDatabase());
        const QVariantMap info = m.signalInfo(idOf(QStringLiteral("TransmissionStatus"), QStringLiteral("GearLever")));
        QVERIFY(info.value(QStringLiteral("isEnum")).toBool());
        QCOMPARE(info.value(QStringLiteral("valueLabels")).toList().size(), 5);
    }

    void lanesReorderResize()
    {
        PlotModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setDatabase(demoDatabase());
        const int a = m.addSignal(0);
        const int b = m.addSignal(1);
        const int c = m.addSignal(2);
        QCOMPARE(m.count(), 3);
        const int idA = m.data(m.index(a), PlotModel::LaneIdRole).toInt();
        const int idC = m.data(m.index(c), PlotModel::LaneIdRole).toInt();
        Q_UNUSED(b);
        QSignalSpy moved(&m, &QAbstractItemModel::rowsMoved);
        m.moveLane(0, 2);
        QCOMPARE(moved.count(), 1);
        QCOMPARE(m.data(m.index(2), PlotModel::LaneIdRole).toInt(), idA);
        m.moveLane(2, 0);
        QCOMPARE(m.data(m.index(0), PlotModel::LaneIdRole).toInt(), idA);
        QCOMPARE(m.data(m.index(2), PlotModel::LaneIdRole).toInt(), idC);
        m.moveLane(0, 0); // no-op
        m.setWeight(1, 10.0);
        QCOMPARE(m.data(m.index(1), PlotModel::WeightRole).toDouble(), 4.0); // clamped
        m.removeLane(1);
        QCOMPARE(m.count(), 2);
        m.removeLane(7); // out of range: ignored
        QCOMPARE(m.count(), 2);
    }

    void yLockAndThresholds()
    {
        PlotModel m;
        m.setDatabase(demoDatabase());
        m.addSignal(0);
        m.setYLock(0, true, 100, 0); // swapped
        QVERIFY(m.data(m.index(0), PlotModel::YLockedRole).toBool());
        QCOMPARE(m.data(m.index(0), PlotModel::YMinRole).toDouble(), 0.0);
        QCOMPARE(m.data(m.index(0), PlotModel::YMaxRole).toDouble(), 100.0);
        QCOMPARE(m.addThreshold(0, 80.0), 0);
        QCOMPARE(m.addThreshold(0, 20.0), 1);
        m.setThreshold(0, 1, 25.0);
        QCOMPARE(m.data(m.index(0), PlotModel::ThresholdsRole).toList(), (QVariantList{80.0, 25.0}));
        m.removeThreshold(0, 0);
        QCOMPARE(m.data(m.index(0), PlotModel::ThresholdsRole).toList(), QVariantList{25.0});
    }

    void databaseChangeClearsPlots()
    {
        PlotModel m;
        m.setDatabase(demoDatabase());
        m.addSignal(3);
        m.setDatabase(nullptr);
        QCOMPARE(m.count(), 0);
        QCOMPARE(m.addSignal(3), -1);
    }

    // Integration: decoded values reach the time-series store through the session.
    void sessionFeedsStore()
    {
        auto *source = new SimulatedSource(5, SimulatorEngine::Mode::Vehicle);
        CaptureSession session(source);
        session.setManualTime(true);
        session.database()->loadDemo();
        session.start();
        for (int ms = 100; ms <= 3000; ms += 100) {
            source->stepTo(std::int64_t(ms) * 1000);
            session.manualTick(ms);
        }
        const int speed = session.signalModel()->signalIdByName(QStringLiteral("VehicleDynamics"),
                                                                QStringLiteral("VehicleSpeed"));
        const TimeSeries *s = session.timeSeries()->series(speed);
        QVERIFY(s);
        QVERIFY(s->size() > 250); // ~100 Hz for 3 s
        QVERIFY(session.timeSeries()->nowUs() > 2'900'000);
        // Pause keeps the store frozen; resume delivers the parked frames.
        session.setPaused(true);
        const std::size_t frozen = s->size();
        for (int ms = 3100; ms <= 4000; ms += 100) {
            source->stepTo(std::int64_t(ms) * 1000);
            session.manualTick(ms);
        }
        QCOMPARE(s->size(), frozen);
        session.setPaused(false);
        QVERIFY(s->size() > frozen + 80); // nothing lost
        session.clear();
        QCOMPARE(int(session.timeSeries()->totalSamples()), 0);
        session.stop();
    }

    void layoutSaveRestore()
    {
        PlotModel m;
        m.setDatabase(demoDatabase());
        QVERIFY(m.applyDemoLayout());
        QCOMPARE(m.count(), 4);
        QCOMPARE(m.seriesCount(), 6);
        m.addThreshold(1, 42.5);
        m.setYLock(2, true, -100, 100);
        m.toggleVisible(0, idOf(QStringLiteral("EngineData"), QStringLiteral("EngineSpeed")));
        const QString json = m.saveLayout();
        QVERIFY(json.contains(QStringLiteral("VehicleDynamics.VehicleSpeed")));

        PlotModel other;
        QAbstractItemModelTester tester(&other, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(other.restoreLayout(json), 0); // no database yet
        other.setDatabase(demoDatabase());
        QCOMPARE(other.restoreLayout(json), 6);
        QCOMPARE(other.count(), 4);
        QCOMPARE(other.lane(1).thresholds.size(), 1u);
        QCOMPARE(other.lane(1).thresholds[0], 42.5);
        QVERIFY(other.lane(2).yLocked);
        QCOMPARE(other.lane(2).yMin, -100.0);
        QVERIFY(!other.lane(0).series[1].visible);
        QCOMPARE(other.lane(3).weight, 0.7);
        QCOMPARE(other.saveLayout(), json);

        // Unknown signals are skipped, broken JSON is ignored.
        QString partial = json;
        partial.replace(QStringLiteral("SteeringAngle"), QStringLiteral("NoSuchSignal"));
        PlotModel third;
        third.setDatabase(demoDatabase());
        QCOMPARE(third.restoreLayout(partial), 5);
        QCOMPARE(third.count(), 3);
        QCOMPARE(third.restoreLayout(QStringLiteral("{oops")), 0);
        QCOMPARE(third.count(), 3);
        QCOMPARE(m.signalInfo(idOf(QStringLiteral("EngineData"), QStringLiteral("ThrottlePosition")))
                     .value(QStringLiteral("resolution")).toDouble(), 0.4);
    }
};

QTEST_GUILESS_MAIN(TstPlotModel)
#include "tst_plotmodel.moc"
