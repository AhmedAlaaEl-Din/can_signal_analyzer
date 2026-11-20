#include "canframe.h"

#include <QtTest>

using namespace can;

class TstCanFrame : public QObject
{
    Q_OBJECT
private slots:
    void formatId_data()
    {
        QTest::addColumn<quint32>("id");
        QTest::addColumn<bool>("extended");
        QTest::addColumn<QString>("expected");
        QTest::newRow("std zero") << 0u << false << "000";
        QTest::newRow("std small") << 0x0Au << false << "00A";
        QTest::newRow("std max") << 0x7FFu << false << "7FF";
        QTest::newRow("ext small") << 0x123u << true << "00000123";
        QTest::newRow("ext max") << 0x1FFFFFFFu << true << "1FFFFFFF";
        QTest::newRow("upper case") << 0xabcu << false << "ABC";
    }
    void formatId()
    {
        QFETCH(quint32, id);
        QFETCH(bool, extended);
        QFETCH(QString, expected);
        QCOMPARE(can::formatId(id, extended), expected);
    }

    void formatData()
    {
        CanFrame f;
        f.dlc = 0;
        QCOMPARE(can::formatData(f), QString());
        f.dlc = 1;
        f.data = {0x0F};
        QCOMPARE(can::formatData(f), QStringLiteral("0F"));
        f.dlc = 8;
        f.data = {0x01, 0xA2, 0xFF, 0x00, 0x10, 0x7e, 0x80, 0x09};
        QCOMPARE(can::formatData(f), QStringLiteral("01 A2 FF 00 10 7E 80 09"));
        // Only dlc bytes are printed even if more data is present.
        f.dlc = 3;
        QCOMPARE(can::formatData(f), QStringLiteral("01 A2 FF"));
        // Width is fixed per DLC: 3 chars per byte minus the trailing space.
        QCOMPARE(can::formatData(f).size(), 3 * 3 - 1);
    }

    void formatTimestamp_data()
    {
        QTest::addColumn<qint64>("us");
        QTest::addColumn<QString>("expected");
        QTest::newRow("zero") << qint64(0) << "0.000000";
        QTest::newRow("micro") << qint64(1) << "0.000001";
        QTest::newRow("ms") << qint64(12345) << "0.012345";
        QTest::newRow("seconds") << qint64(12345678) << "12.345678";
        QTest::newRow("hour") << qint64(3600000000) << "3600.000000";
        QTest::newRow("negative") << qint64(-1500000) << "-1.500000";
    }
    void formatTimestamp()
    {
        QFETCH(qint64, us);
        QFETCH(QString, expected);
        QCOMPARE(can::formatTimestamp(us), expected);
    }

    void parseHexId_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<bool>("ok");
        QTest::addColumn<quint32>("value");
        QTest::newRow("plain") << "1A4" << true << 0x1A4u;
        QTest::newRow("prefix") << "0x1a4" << true << 0x1A4u;
        QTest::newRow("spaces") << "  7ff " << true << 0x7FFu;
        QTest::newRow("ext max") << "1FFFFFFF" << true << 0x1FFFFFFFu;
        QTest::newRow("too big") << "20000000" << false << 0u;
        QTest::newRow("bad char") << "12G" << false << 0u;
        QTest::newRow("empty") << "" << false << 0u;
        QTest::newRow("only prefix") << "0x" << false << 0u;
    }
    void parseHexId()
    {
        QFETCH(QString, text);
        QFETCH(bool, ok);
        QFETCH(quint32, value);
        std::uint32_t out = 0xDEAD;
        QCOMPARE(can::parseHexId(text, out), ok);
        if (ok)
            QCOMPARE(out, value);
        else
            QCOMPARE(out, 0xDEADu);
    }

    void validity()
    {
        CanFrame f;
        f.id = 0x7FF;
        QVERIFY(f.isValid());
        f.id = 0x800;
        QVERIFY(!f.isValid());
        f.extended = true;
        QVERIFY(f.isValid());
        f.dlc = 9;
        QVERIFY(!f.isValid());
    }

    void keySeparatesFormats()
    {
        CanFrame a;
        a.id = 0x123;
        CanFrame b = a;
        b.extended = true;
        QVERIFY(a.key() != b.key());
    }
};

QTEST_GUILESS_MAIN(TstCanFrame)
#include "tst_canframe.moc"
