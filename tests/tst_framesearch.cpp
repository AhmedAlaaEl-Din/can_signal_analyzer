#include "dbc.h"
#include "framemodel.h"
#include "framesearch.h"
#include "signalcodec.h"

#include <QTest>

using namespace can;

// Trace search: IDs, message names, data patterns with wildcards and signal
// value conditions (numeric and value-table text), plus FrameModel navigation.
class TestFrameSearch : public QObject
{
    Q_OBJECT

    static CanFrame frame(std::uint32_t id, std::initializer_list<int> bytes, bool ext = false)
    {
        CanFrame f;
        f.id = id;
        f.extended = ext;
        int i = 0;
        for (int b : bytes)
            f.data[static_cast<std::size_t>(i++)] = static_cast<std::uint8_t>(b);
        f.dlc = static_cast<std::uint8_t>(i);
        return f;
    }

    static CanFrame speedFrame(double kmh)
    {
        auto db = demoDatabase();
        const MessageDef *m = db->find(200, false);
        CanFrame f;
        f.id = 200;
        f.dlc = 8;
        codec::encode(*m->findSignal(QStringLiteral("VehicleSpeed")), kmh, f.data.data());
        return f;
    }

private slots:
    void emptyAndInvalid()
    {
        auto s = FrameSearch::parse(QStringLiteral("   "), nullptr);
        QCOMPARE(s.kind(), FrameSearch::Kind::Empty);
        QVERIFY(!s.isValid());
        s = FrameSearch::parse(QStringLiteral("hello"), nullptr);
        QCOMPARE(s.kind(), FrameSearch::Kind::Invalid);
        QVERIFY(!s.error().isEmpty());
        s = FrameSearch::parse(QStringLiteral("123456789"), nullptr);
        QCOMPARE(s.kind(), FrameSearch::Kind::Invalid);
    }

    void ids()
    {
        auto s = FrameSearch::parse(QStringLiteral("1A4"), nullptr);
        QCOMPARE(s.kind(), FrameSearch::Kind::Id);
        QVERIFY(s.matches(frame(0x1A4, {1})));
        QVERIFY(!s.matches(frame(0x1A5, {1})));
        for (const char *t : {"0x1a4", "id 1A4", "id:1A4", "ID: 0x1A4"})
            QVERIFY2(FrameSearch::parse(QString::fromLatin1(t), nullptr).matches(frame(0x1A4, {})), t);
        s = FrameSearch::parse(QStringLiteral("18FEF100"), nullptr);
        QVERIFY(s.matches(frame(0x18FEF100, {}, true)));
        QCOMPARE(s.description(), QStringLiteral("ID 18FEF100"));
    }

    void messageNames()
    {
        auto s = FrameSearch::parse(QStringLiteral("enginedata"), demoDatabase());
        QCOMPARE(s.kind(), FrameSearch::Kind::Id);
        QVERIFY(s.matches(frame(256, {0})));
        QVERIFY(!s.matches(frame(256, {0}, true))); // the message is standard
        QVERIFY(s.description().contains(QStringLiteral("EngineData")));
    }

    void dataPatterns()
    {
        auto s = FrameSearch::parse(QStringLiteral("12 ?? FF"), nullptr);
        QCOMPARE(s.kind(), FrameSearch::Kind::Data);
        QVERIFY(s.matches(frame(1, {0x12, 0x00, 0xFF})));
        QVERIFY(s.matches(frame(1, {0x12, 0xAB, 0xFF, 0x77, 0x01}))); // prefix
        QVERIFY(!s.matches(frame(1, {0x12, 0xAB, 0xFE})));
        QVERIFY(!s.matches(frame(1, {0x13, 0xAB, 0xFF})));
        QVERIFY(!s.matches(frame(1, {0x12, 0xAB})));                  // too short
        QCOMPARE(s.description(), QStringLiteral("Data 12 ?? FF…"));
        // Other wildcard spellings and single hex digits.
        QVERIFY(FrameSearch::parse(QStringLiteral("? xx ** a"), nullptr).matches(frame(1, {9, 9, 9, 0x0A})));
        QVERIFY(!FrameSearch::parse(QStringLiteral("? xx ** a"), nullptr).matches(frame(1, {9, 9, 9, 0x0B})));
        // Errors.
        QCOMPARE(FrameSearch::parse(QStringLiteral("12 GG"), nullptr).kind(), FrameSearch::Kind::Invalid);
        QCOMPARE(FrameSearch::parse(QStringLiteral("1 2 3 4 5 6 7 8 9"), nullptr).kind(), FrameSearch::Kind::Invalid);
        QCOMPARE(FrameSearch::parse(QStringLiteral("123 45"), nullptr).kind(), FrameSearch::Kind::Invalid);
    }

    void signalConditions()
    {
        auto db = demoDatabase();
        auto s = FrameSearch::parse(QStringLiteral("VehicleSpeed > 100"), db);
        QCOMPARE(s.kind(), FrameSearch::Kind::Signal);
        QVERIFY(s.matches(speedFrame(100.01)));
        QVERIFY(!s.matches(speedFrame(100.0)));
        QVERIFY(!s.matches(speedFrame(42)));
        QVERIFY(!s.matches(frame(0x100, {0xFF, 0xFF})));         // other message
        QCOMPARE(s.description(), QStringLiteral("Signal VehicleSpeed > 100 km/h"));

        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed>=100"), db).matches(speedFrame(100.0)));
        QVERIFY(FrameSearch::parse(QStringLiteral("vehiclespeed <= 50"), db).matches(speedFrame(50)));
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleDynamics.VehicleSpeed < 50"), db).matches(speedFrame(49.99)));
        // Equality is within half a quantisation step (0.01 km/h).
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed == 88.8"), db).matches(speedFrame(88.8)));
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed = 88.8"), db).matches(speedFrame(88.8)));
        QVERIFY(!FrameSearch::parse(QStringLiteral("VehicleSpeed == 88.8"), db).matches(speedFrame(88.82)));
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed != 88.8"), db).matches(speedFrame(88.82)));

        // Value-table text.
        const MessageDef *tm = db->find(416, false);
        const SignalDef *lever = tm->findSignal(QStringLiteral("GearLever"));
        QVERIFY(lever && !lever->valueTable.empty());
        const auto &[raw, text] = *lever->valueTable.rbegin();
        CanFrame f;
        f.id = 416;
        f.dlc = 4;
        codec::encode(*lever, codec::toPhysical(*lever, raw), f.data.data());
        auto e = FrameSearch::parse(QStringLiteral("GearLever = %1").arg(text.toLower()), db);
        QVERIFY2(e.isValid(), qPrintable(e.error()));
        QVERIFY(e.matches(f));
        QVERIFY(FrameSearch::parse(QStringLiteral("GearLever = \"%1\"").arg(text), db).matches(f));

        // Errors.
        QVERIFY(FrameSearch::parse(QStringLiteral("NoSuchSignal > 1"), db).error().contains(QStringLiteral("Unknown signal")));
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed > fast"), db).error().contains(QStringLiteral("not a number")));
        QVERIFY(FrameSearch::parse(QStringLiteral("GearLever = Warp"), db).error().contains(QStringLiteral("state of")));
        QVERIFY(FrameSearch::parse(QStringLiteral("VehicleSpeed > 100"), nullptr).error().contains(QStringLiteral("database")));
    }

    void multiplexedSignal()
    {
        auto db = demoDatabase();
        const MessageDef *m = db->find(1008, false);
        CanFrame page1;
        page1.id = 1008;
        page1.dlc = 8;
        codec::encode(*m->findSignal(QStringLiteral("TripPage")), 1, page1.data.data());
        codec::encode(*m->findSignal(QStringLiteral("FuelLevel")), 60, page1.data.data());
        CanFrame page0 = page1;
        codec::encode(*m->findSignal(QStringLiteral("TripPage")), 0, page0.data.data());
        auto s = FrameSearch::parse(QStringLiteral("FuelLevel > 50"), db);
        QVERIFY(s.matches(page1));
        QVERIFY(!s.matches(page0)); // FuelLevel is not on page 0
    }

    void modelNavigation()
    {
        FrameModel model;
        std::vector<CanFrame> frames;
        for (int i = 0; i < 20; ++i)
            frames.push_back(frame(i % 4 == 0 ? 0x7FF : 0x100, {i}));
        model.append(frames);
        model.setSearch(FrameSearch::parse(QStringLiteral("7FF"), nullptr));
        QCOMPARE(model.matchCount(), 5);
        QCOMPARE(model.findMatch(-1, true), 0);
        QCOMPARE(model.findMatch(0, true), 4);
        QCOMPARE(model.findMatch(16, true), 0);          // wraps around
        QCOMPARE(model.findMatch(4, false), 0);
        QCOMPARE(model.findMatch(0, false), 16);         // wraps backwards
        QCOMPARE(model.findMatch(model.count(), false), 16);
        QCOMPARE(model.data(model.index(4, 0), FrameModel::MatchRole).toBool(), true);
        QCOMPARE(model.data(model.index(5, 0), FrameModel::MatchRole).toBool(), false);
        model.setSearch(FrameSearch::parse(QStringLiteral("123"), nullptr));
        QCOMPARE(model.findMatch(-1, true), -1);
        QCOMPARE(model.matchCount(), 0);
    }
};

QTEST_GUILESS_MAIN(TestFrameSearch)
#include "tst_framesearch.moc"
