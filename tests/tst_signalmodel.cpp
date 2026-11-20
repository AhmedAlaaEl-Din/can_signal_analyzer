#include "dbc.h"
#include "signaldatabase.h"
#include "signallistmodel.h"
#include "simulatorengine.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace can;

namespace {
std::vector<CanFrame> vehicleFrames(std::int64_t endUs)
{
    SimulatorEngine e(9, SimulatorEngine::Mode::Vehicle);
    std::vector<CanFrame> out;
    e.advanceTo(endUs, out);
    return out;
}

int countKind(const SignalListModel &m, const QString &kind)
{
    int n = 0;
    for (int r = 0; r < m.rowCount(); ++r)
        n += m.data(m.index(r), SignalListModel::KindRole).toString() == kind;
    return n;
}
} // namespace

class TstSignalModel : public QObject
{
    Q_OBJECT
private slots:
    void structure()
    {
        SignalListModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(m.rowCount(), 0);
        m.setDatabase(demoDatabase());
        QCOMPARE(countKind(m, QStringLiteral("header")), 9);
        QCOMPARE(countKind(m, QStringLiteral("signal")), demoDatabase()->signalCount());
        QCOMPARE(m.data(m.index(0), SignalListModel::NameRole).toString(), QStringLiteral("VehicleDynamics"));
        QCOMPARE(m.data(m.index(0), SignalListModel::FrameIdRole).toString(), QStringLiteral("0C8"));
    }

    void liveValuesAndHistory()
    {
        SignalListModel m;
        m.setDatabase(demoDatabase());
        const int speed = m.signalIdByName(QStringLiteral("VehicleDynamics"), QStringLiteral("VehicleSpeed"));
        QVERIFY(speed >= 0);
        QVERIFY(!m.data(m.index(m.rowOfSignal(speed)), SignalListModel::HasValueRole).toBool());

        const auto frames = vehicleFrames(12'000'000);
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
        m.update(frames);
        QCOMPARE(changed.count(), 1); // one notification per batch
        const QModelIndex idx = m.index(m.rowOfSignal(speed));
        QVERIFY(m.data(idx, SignalListModel::HasValueRole).toBool());
        QCOMPARE(m.data(idx, SignalListModel::UnitRole).toString(), QStringLiteral("km/h"));
        QVERIFY(m.data(idx, SignalListModel::MaxRole).toString().toDouble() > 20.0);
        QCOMPARE(m.data(idx, SignalListModel::MinRole).toString(), QStringLiteral("0.00"));
        QVERIFY(m.timeUs() > 11'900'000);

        std::vector<SignalListModel::Sample> h;
        m.history(speed, h);
        QVERIFY(h.size() > 250 && h.size() <= std::size_t(SignalListModel::kHistoryCapacity));
        for (std::size_t i = 1; i < h.size(); ++i)
            QVERIFY(h[i].timeUs > h[i - 1].timeUs);
        // One sample per 40 ms bucket.
        QVERIFY(h.back().timeUs - h.front().timeUs < 12'100'000);

        // Enumerations expose their value-table text.
        const int lever = m.signalIdByName(QString(), QStringLiteral("GearLever"));
        QCOMPARE(m.data(m.index(m.rowOfSignal(lever)), SignalListModel::EnumTextRole).toString(), QStringLiteral("D"));

        m.clearValues();
        QVERIFY(!m.data(idx, SignalListModel::HasValueRole).toBool());
        m.history(speed, h);
        QVERIFY(h.empty());
    }

    void filterPinCollapse()
    {
        SignalListModel m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.setDatabase(demoDatabase());
        m.setFilterText(QStringLiteral("temp"));
        // CoolantTemp, OilTemp, IntakeAirTemp, TransOilTemp, BatteryTemp, AmbientTemp, CabinTemp
        // + all signals of EngineTemperatures (message name matches)
        QVERIFY(countKind(m, QStringLiteral("signal")) >= 7);
        for (int r = 0; r < m.rowCount(); ++r) {
            if (m.data(m.index(r), SignalListModel::KindRole) != QStringLiteral("signal"))
                continue;
            const QString text = m.data(m.index(r), SignalListModel::NameRole).toString()
                                 + m.data(m.index(r), SignalListModel::MessageNameRole).toString();
            QVERIFY(text.contains(QStringLiteral("temp"), Qt::CaseInsensitive));
        }
        m.setFilterText(QString());

        const int rpm = m.signalIdByName(QString(), QStringLiteral("EngineSpeed"));
        m.togglePin(rpm);
        QVERIFY(m.isPinned(rpm));
        QCOMPARE(m.pinnedCount(), 1);
        QVERIFY(m.data(m.index(0), SignalListModel::PinnedSectionRole).toBool());
        QCOMPARE(m.rowOfSignal(rpm), 1); // directly under the Pinned header
        m.togglePin(rpm);
        QVERIFY(!m.isPinned(rpm));

        const int signalsBefore = countKind(m, QStringLiteral("signal"));
        m.toggleGroup(0); // collapse VehicleDynamics
        QCOMPARE(countKind(m, QStringLiteral("signal")), signalsBefore - 5);
        QVERIFY(m.data(m.index(0), SignalListModel::CollapsedRole).toBool());
        m.setAllCollapsed(false);
        QCOMPARE(countKind(m, QStringLiteral("signal")), signalsBefore);
    }

    void detailsAndBitLayout()
    {
        SignalListModel m;
        m.setDatabase(demoDatabase());
        const int torque = m.signalIdByName(QString(), QStringLiteral("EngineTorque"));
        const QVariantMap d = m.signalDetails(torque);
        QCOMPARE(d.value(QStringLiteral("message")).toString(), QStringLiteral("EngineData"));
        QCOMPARE(d.value(QStringLiteral("startBit")).toInt(), 39);
        QCOMPARE(d.value(QStringLiteral("length")).toInt(), 12);
        QVERIFY(!d.value(QStringLiteral("intel")).toBool());
        QVERIFY(d.value(QStringLiteral("signed")).toBool());
        const QVariantList bits = d.value(QStringLiteral("bits")).toList();
        QCOMPARE(bits.size(), 12);
        QCOMPARE(bits.first().toInt(), 39); // MSB
        QCOMPARE(bits.last().toInt(), 44);  // LSB
        const int state = m.signalIdByName(QString(), QStringLiteral("EngineState"));
        QCOMPARE(m.signalDetails(state).value(QStringLiteral("valueTable")).toList().size(), 5);
        const int odo = m.signalIdByName(QString(), QStringLiteral("Odometer"));
        QCOMPARE(m.signalDetails(odo).value(QStringLiteral("multiplexing")).toString(), QStringLiteral("Multiplexed, page 0"));
    }

    void databaseLoading()
    {
        SignalDatabase db;
        QSignalSpy changed(&db, &SignalDatabase::changed);
        db.loadDemo();
        QVERIFY(db.isLoaded());
        QVERIFY(db.isDemo());
        QCOMPARE(db.name(), QStringLiteral("demo_vehicle.dbc"));
        QCOMPARE(changed.count(), 1);

        QTemporaryDir dir;
        const QString bad = dir.filePath(QStringLiteral("broken.dbc"));
        QFile f(bad);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (1,0) [0|0] km N\n");
        f.close();
        QVERIFY(!db.loadPath(bad));
        QVERIFY(db.isLoaded()); // previous database kept
        QVERIFY(db.errorTitle().contains(QStringLiteral("broken.dbc")));
        QVERIFY(db.errorLines().first().startsWith(QStringLiteral("Line 2:")));
        db.dismissError();
        QVERIFY(db.errorLines().isEmpty());
        db.unload();
        QVERIFY(!db.isLoaded());
    }
};

QTEST_GUILESS_MAIN(TstSignalModel)
#include "tst_signalmodel.moc"
