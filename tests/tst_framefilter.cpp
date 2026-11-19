#include "framefilter.h"

#include <QtTest>

using namespace can;
using Range = FrameFilter::Range;

namespace {
CanFrame frame(std::uint32_t id, int dlc = 8, bool ext = false)
{
    CanFrame f;
    f.id = id;
    f.dlc = static_cast<std::uint8_t>(dlc);
    f.extended = ext;
    return f;
}
} // namespace

class TstFrameFilter : public QObject
{
    Q_OBJECT
private slots:
    void emptyAcceptsAll()
    {
        FrameFilter f;
        QVERIFY(!f.isActive());
        QVERIFY(f.matches(frame(0x123)));
        QVERIFY(f.matches(frame(0x1ABCDE01, 3, true)));
    }

    void parseIds()
    {
        std::vector<Range> r;
        QVERIFY(FrameFilter::parseIdList(QStringLiteral("123"), r));
        QCOMPARE(r, (std::vector<Range>{{0x123, 0x123}}));
        QVERIFY(FrameFilter::parseIdList(QStringLiteral("0x0A0, 123;600-6FF"), r));
        QCOMPARE(r, (std::vector<Range>{{0xA0, 0xA0}, {0x123, 0x123}, {0x600, 0x6FF}}));
        QVERIFY(FrameFilter::parseIdList(QStringLiteral(" 100 - 1ff  7DF "), r));
        QCOMPARE(r, (std::vector<Range>{{0x100, 0x1FF}, {0x7DF, 0x7DF}}));
        QVERIFY(FrameFilter::parseIdList(QStringLiteral("2FF-200"), r)); // reversed range
        QCOMPARE(r, (std::vector<Range>{{0x200, 0x2FF}}));
        QVERIFY(FrameFilter::parseIdList(QStringLiteral("   "), r));
        QVERIFY(r.empty());
    }

    void parseIdErrors()
    {
        std::vector<Range> r{{1, 1}};
        QString error;
        QVERIFY(!FrameFilter::parseIdList(QStringLiteral("123, xyz"), r, &error));
        QVERIFY(error.contains(QStringLiteral("xyz")));
        QCOMPARE(r, (std::vector<Range>{{1, 1}})); // untouched on failure
        QVERIFY(!FrameFilter::parseIdList(QStringLiteral("100-"), r, &error));
        QVERIFY(!FrameFilter::parseIdList(QStringLiteral("FFFFFFFF"), r, &error));
    }

    void parseDlc()
    {
        std::vector<Range> r;
        QVERIFY(FrameFilter::parseDlcList(QStringLiteral("8"), r));
        QCOMPARE(r, (std::vector<Range>{{8, 8}}));
        QVERIFY(FrameFilter::parseDlcList(QStringLiteral("0-4, 8"), r));
        QCOMPARE(r, (std::vector<Range>{{0, 4}, {8, 8}}));
        QString error;
        QVERIFY(!FrameFilter::parseDlcList(QStringLiteral("9"), r, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!FrameFilter::parseDlcList(QStringLiteral("A"), r, &error));
    }

    void singleId()
    {
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("123")));
        QVERIFY(f.isActive());
        QVERIFY(f.matches(frame(0x123)));
        QVERIFY(!f.matches(frame(0x124)));
    }

    void idListAndRange()
    {
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("0A0, 300-3FF")));
        QVERIFY(f.matches(frame(0x0A0)));
        QVERIFY(f.matches(frame(0x300)));
        QVERIFY(f.matches(frame(0x3E8)));
        QVERIFY(f.matches(frame(0x3FF)));
        QVERIFY(!f.matches(frame(0x400)));
        QVERIFY(!f.matches(frame(0x2FF)));
    }

    void invalidKeepsPrevious()
    {
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("123")));
        QString error;
        QVERIFY(!f.setIdExpression(QStringLiteral("12Z"), &error));
        QVERIFY(f.matches(frame(0x123)));
        QVERIFY(!f.matches(frame(0x1)));
    }

    void dlcFilter()
    {
        FrameFilter f;
        QVERIFY(f.setDlcExpression(QStringLiteral("0-2")));
        QVERIFY(f.matches(frame(1, 0)));
        QVERIFY(f.matches(frame(1, 2)));
        QVERIFY(!f.matches(frame(1, 3)));
    }

    void formatFilter()
    {
        FrameFilter f;
        f.setFormat(FrameFilter::Format::Standard);
        QVERIFY(f.matches(frame(0x100)));
        QVERIFY(!f.matches(frame(0x100, 8, true)));
        f.setFormat(FrameFilter::Format::Extended);
        QVERIFY(!f.matches(frame(0x100)));
        QVERIFY(f.matches(frame(0x100, 8, true)));
    }

    void criteriaCombineWithAnd()
    {
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("100-1FF")));
        QVERIFY(f.setDlcExpression(QStringLiteral("8")));
        f.setFormat(FrameFilter::Format::Standard);
        QVERIFY(f.matches(frame(0x123, 8)));
        QVERIFY(!f.matches(frame(0x123, 4)));
        QVERIFY(!f.matches(frame(0x123, 8, true)));
        QVERIFY(!f.matches(frame(0x223, 8)));
    }

    void extendedIdRange()
    {
        FrameFilter f;
        QVERIFY(f.setIdExpression(QStringLiteral("1ABCDE00-1ABCDEFF")));
        QVERIFY(f.matches(frame(0x1ABCDE01, 8, true)));
        QVERIFY(!f.matches(frame(0x0C0FFEE5, 5, true)));
    }
};

QTEST_GUILESS_MAIN(TstFrameFilter)
#include "tst_framefilter.moc"
