#include "dbc.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace can;

namespace {
const char *kMinimal =
    "VERSION \"\"\n"
    "\n"
    "NS_ :\n"
    "\tNS_DESC_\n"
    "\tCM_\n"
    "\tBA_DEF_\n"
    "\n"
    "BS_:\n"
    "\n"
    "BU_: NodeA NodeB\n"
    "\n"
    "BO_ 100 Alpha: 8 NodeA\n"
    " SG_ Speed : 0|16@1+ (0.01,0) [0|320] \"km/h\" NodeB\n"
    " SG_ Temp : 23|8@0- (0.5,-10) [-50|50] \"degC\" NodeB,NodeA\n"
    "\n"
    "BO_ 2147484160 Beta: 4 NodeB\n"
    " SG_ Mode : 0|2@1+ (1,0) [0|3] \"\" NodeA\n"
    "\n"
    "CM_ \"global comment\";\n"
    "CM_ BO_ 100 \"message\n"
    "comment\";\n"
    "CM_ SG_ 100 Speed \"speed \\\"comment\\\"\";\n"
    "BA_DEF_ BO_ \"GenMsgCycleTime\" INT 0 10000;\n"
    "BA_ \"GenMsgCycleTime\" BO_ 100 20;\n"
    "VAL_ 2147484160 Mode 0 \"Off\" 1 \"On\" 2 \"Auto\" ;\n"
    "";

ParseResult parse(const QString &text)
{
    return DbcParser::parse(text, QStringLiteral("test.dbc"));
}

bool hasErrorAt(const ParseResult &r, int line, const QString &fragment)
{
    for (const auto &e : r.errors)
        if (e.line == line && e.message.contains(fragment, Qt::CaseInsensitive))
            return true;
    return false;
}

QString errorsText(const ParseResult &r)
{
    QStringList l;
    for (const auto &e : r.errors)
        l << e.toString();
    return l.join(QLatin1Char('\n'));
}
} // namespace

class TstDbcParser : public QObject
{
    Q_OBJECT
private slots:
    void minimalFile()
    {
        const auto r = parse(QString::fromUtf8(kMinimal));
        QVERIFY2(r.ok(), qPrintable(errorsText(r)));
        const Database &db = *r.database;
        QCOMPARE(int(db.messages.size()), 2);
        QCOMPARE(db.comment, QStringLiteral("global comment"));
        QCOMPARE(db.sourceName, QStringLiteral("test.dbc"));

        const MessageDef *a = db.find(100, false);
        QVERIFY(a);
        QCOMPARE(a->name, QStringLiteral("Alpha"));
        QCOMPARE(a->dlc, 8);
        QCOMPARE(a->transmitter, QStringLiteral("NodeA"));
        QCOMPARE(a->comment, QStringLiteral("message\ncomment"));
        QCOMPARE(a->cycleTimeMs, 20);
        QCOMPARE(int(a->signalList.size()), 2);

        const SignalDef &speed = a->signalList[0];
        QCOMPARE(speed.startBit, 0);
        QCOMPARE(speed.length, 16);
        QVERIFY(speed.byteOrder == ByteOrder::Intel);
        QVERIFY(!speed.isSigned);
        QCOMPARE(speed.factor, 0.01);
        QCOMPARE(speed.maximum, 320.0);
        QCOMPARE(speed.unit, QStringLiteral("km/h"));
        QCOMPARE(speed.receivers, QStringList{QStringLiteral("NodeB")});
        QCOMPARE(speed.comment, QStringLiteral("speed \"comment\""));

        const SignalDef &temp = a->signalList[1];
        QVERIFY(temp.byteOrder == ByteOrder::Motorola);
        QVERIFY(temp.isSigned);
        QCOMPARE(temp.offset, -10.0);
        QCOMPARE(temp.receivers, (QStringList{QStringLiteral("NodeB"), QStringLiteral("NodeA")}));
    }

    void extendedIdAndValueTable()
    {
        const auto r = parse(QString::fromUtf8(kMinimal));
        QVERIFY(r.ok());
        const MessageDef *b = r.database->find(0x200, true);
        QVERIFY(b);
        QVERIFY(b->extended);
        QCOMPARE(b->id, 0x200u);
        QVERIFY(!r.database->find(0x200, false));
        const SignalDef &mode = b->signalList[0];
        QCOMPARE(int(mode.valueTable.size()), 3);
        QCOMPARE(mode.valueTable.at(2), QStringLiteral("Auto"));
    }

    void crlfLineEndings()
    {
        QString text = QString::fromUtf8(kMinimal);
        text.replace(QLatin1String("\n"), QLatin1String("\r\n"));
        const auto r = parse(text);
        QVERIFY2(r.ok(), qPrintable(errorsText(r)));
        QCOMPARE(r.database->find(100, false)->signalList[0].unit, QStringLiteral("km/h"));
    }

    void multiplexing()
    {
        const auto r = parse(QStringLiteral("BO_ 10 Mux: 8 N\n"
                                            " SG_ Page M : 0|8@1+ (1,0) [0|0] \"\" N\n"
                                            " SG_ A m0 : 8|16@1+ (1,0) [0|0] \"\" N\n"
                                            " SG_ B m1 : 8|8@1+ (1,0) [0|0] \"\" N\n"));
        QVERIFY2(r.ok(), qPrintable(errorsText(r)));
        const MessageDef &m = r.database->messages[0];
        QVERIFY(m.multiplexor());
        QCOMPARE(m.multiplexor()->name, QStringLiteral("Page"));
        QVERIFY(m.signalList[1].muxRole == SignalDef::MuxRole::Multiplexed);
        QCOMPARE(m.signalList[2].muxValue, 1);
    }

    void demoDatabaseParses()
    {
        const auto db = demoDatabase();
        QVERIFY(db);
        QCOMPARE(int(db->messages.size()), 9);
        QVERIFY(db->signalCount() >= 40);
        const MessageDef *engine = db->find(0x100, false);
        QVERIFY(engine);
        QCOMPARE(engine->name, QStringLiteral("EngineData"));
        QCOMPARE(engine->cycleTimeMs, 10);
        QVERIFY(db->find(0x121CE042, true));
        // The comment spanning two lines is kept.
        QVERIFY(db->find(0x120, false)->findSignal(QStringLiteral("CoolantTemp"))->comment.contains(QLatin1Char('\n')));
    }

    void parseFileAndMissingFile()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("x.dbc"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(kMinimal);
        f.close();
        auto r = DbcParser::parseFile(path);
        QVERIFY(r.ok());
        QCOMPARE(r.database->sourceName, QStringLiteral("x.dbc"));

        r = DbcParser::parseFile(dir.filePath(QStringLiteral("missing.dbc")));
        QVERIFY(!r.ok());
        QVERIFY(r.errors[0].message.contains(QStringLiteral("cannot open")));
    }

    // --- invalid input: errors carry line numbers -------------------------------
    void errors_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("line");
        QTest::addColumn<QString>("fragment");
        QTest::newRow("bad id") << "BO_ abc M: 8 N\n" << 1 << "decimal";
        QTest::newRow("missing colon") << "BO_ 1 M 8 N\n" << 1 << "':'";
        QTest::newRow("fd dlc") << "BO_ 1 M: 64 N\n" << 1 << "CAN FD";
        QTest::newRow("std id too big") << "BO_ 4096 M: 8 N\n" << 1 << "out of range";
        QTest::newRow("sg outside bo") << "\n SG_ S : 0|8@1+ (1,0) [0|0] \"\" N\n" << 2 << "outside";
        QTest::newRow("bad byte order") << "BO_ 1 M: 8 N\n SG_ S : 0|8@2+ (1,0) [0|0] \"\" N\n" << 2 << "byte order";
        QTest::newRow("bad factor") << "BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (x,0) [0|0] \"\" N\n" << 2 << "factor";
        QTest::newRow("no unit quotes") << "BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (1,0) [0|0] km N\n" << 2 << "quoted";
        QTest::newRow("length 0") << "BO_ 1 M: 8 N\n SG_ S : 0|0@1+ (1,0) [0|0] \"\" N\n" << 2 << "length";
        QTest::newRow("too long for dlc") << "BO_ 1 M: 2 N\n SG_ S : 8|16@1+ (1,0) [0|0] \"\" N\n" << 2 << "do not fit";
        QTest::newRow("motorola beyond") << "BO_ 1 M: 1 N\n SG_ S : 3|8@0+ (1,0) [0|0] \"\" N\n" << 2 << "do not fit";
        QTest::newRow("duplicate id") << "BO_ 1 A: 8 N\nBO_ 1 B: 8 N\n" << 2 << "twice";
        QTest::newRow("duplicate signal") << "BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (1,0) [0|0] \"\" N\n SG_ S : 8|8@1+ (1,0) [0|0] \"\" N\n" << 3 << "twice";
        QTest::newRow("unterminated cm") << "BO_ 1 M: 8 N\nCM_ BO_ 1 \"never closed\n" << 2 << "';'";
        QTest::newRow("mux without M") << "BO_ 1 M: 8 N\n SG_ S m1 : 0|8@1+ (1,0) [0|0] \"\" N\n" << 1 << "multiplexor";
        QTest::newRow("bad val") << "BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (1,0) [0|0] \"\" N\nVAL_ 1 S x \"a\" ;\n" << 3 << "integer";
    }
    void errors()
    {
        QFETCH(QString, text);
        QFETCH(int, line);
        QFETCH(QString, fragment);
        const auto r = parse(text);
        QVERIFY(!r.ok());
        QVERIFY2(hasErrorAt(r, line, fragment), qPrintable(errorsText(r)));
        QVERIFY(r.errors[0].toString().startsWith(QStringLiteral("Line %1:").arg(r.errors[0].line)));
    }

    void emptyFileIsAnError()
    {
        const auto r = parse(QStringLiteral("VERSION \"\"\n\nBU_: A\n"));
        QVERIFY(!r.ok());
        QVERIFY(r.errors[0].message.contains(QStringLiteral("no messages")));
    }

    void warningsDoNotFail()
    {
        const auto r = parse(QStringLiteral("BO_ 1 M: 8 N\n SG_ S : 0|8@1+ (1,0) [0|0] \"\" N\n"
                                            "VAL_ 1 Unknown 0 \"a\" ;\nCM_ SG_ 99 S \"x\";\nFOO_ bar\n"));
        QVERIFY(r.ok());
        QCOMPARE(int(r.warnings.size()), 3);
    }

    void independentSignalsPseudoMessageIsSkipped()
    {
        const auto r = parse(QStringLiteral("BO_ 3221225472 VECTOR__INDEPENDENT_SIG_MSG: 0 Vector__XXX\n"
                                            " SG_ Orphan : 0|8@1+ (1,0) [0|0] \"\" N\n"
                                            "BO_ 1 M: 8 N\n"));
        QVERIFY(r.ok());
        QCOMPARE(int(r.database->messages.size()), 1);
    }
};

QTEST_GUILESS_MAIN(TstDbcParser)
#include "tst_dbcparser.moc"
