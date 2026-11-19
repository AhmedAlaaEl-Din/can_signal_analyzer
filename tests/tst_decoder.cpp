#include "signalcodec.h"

#include <QtTest>

#include <random>

using namespace can;

namespace {
SignalDef sig(int start, int len, ByteOrder order, bool isSigned = false, double factor = 1,
              double offset = 0, double min = 0, double max = 0)
{
    SignalDef s;
    s.name = QStringLiteral("S");
    s.startBit = start;
    s.length = len;
    s.byteOrder = order;
    s.isSigned = isSigned;
    s.factor = factor;
    s.offset = offset;
    s.minimum = min;
    s.maximum = max;
    return s;
}

CanFrame frame(std::initializer_list<int> bytes)
{
    CanFrame f;
    f.dlc = static_cast<std::uint8_t>(bytes.size());
    int i = 0;
    for (int b : bytes)
        f.data[static_cast<std::size_t>(i++)] = static_cast<std::uint8_t>(b);
    return f;
}

DecodedSignal decodeOne(const SignalDef &s, const CanFrame &f)
{
    MessageDef m;
    m.dlc = 8;
    m.signalList.push_back(s);
    return Decoder::decodeSignal(m, 0, f);
}

constexpr auto I = ByteOrder::Intel;
constexpr auto M = ByteOrder::Motorola;
} // namespace

class TstDecoder : public QObject
{
    Q_OBJECT
private slots:
    // --- known test vectors (computed by hand) -------------------------------
    void vectors_data()
    {
        QTest::addColumn<int>("start");
        QTest::addColumn<int>("length");
        QTest::addColumn<bool>("motorola");
        QTest::addColumn<bool>("isSigned");
        QTest::addColumn<double>("factor");
        QTest::addColumn<double>("offset");
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<qlonglong>("raw");
        QTest::addColumn<double>("physical");

        // Intel, unsigned 16-bit, little endian: 34 12 -> 0x1234
        QTest::newRow("intel u16 scaled") << 0 << 16 << false << false << 0.01 << 0.0
            << QByteArray::fromHex("3412") << 0x1234LL << 46.60;
        // Intel, 12 bits from bit 4: high nibble of byte 0 + byte 1 (crosses bytes)
        QTest::newRow("intel cross byte") << 4 << 12 << false << false << 1.0 << 0.0
            << QByteArray::fromHex("A0BC") << 0xBCALL << 3018.0;
        // Intel signed: FC18 = -1000, factor 0.1
        QTest::newRow("intel signed") << 16 << 16 << false << true << 0.1 << 0.0
            << QByteArray::fromHex("000018FC") << -1000LL << -100.0;
        // Intel signed positive stays positive
        QTest::newRow("intel signed positive") << 16 << 16 << false << true << 0.1 << 0.0
            << QByteArray::fromHex("0000E803") << 1000LL << 100.0;
        // Offset: 0x50 = 80, offset -40
        QTest::newRow("offset") << 8 << 8 << false << false << 1.0 << -40.0
            << QByteArray::fromHex("0050") << 80LL << 40.0;
        // Motorola 16-bit from bit 7: big endian 12 34 -> 0x1234
        QTest::newRow("motorola u16") << 7 << 16 << true << false << 0.25 << 0.0
            << QByteArray::fromHex("1234") << 0x1234LL << 1165.0;
        // Motorola 8 bits from bit 3: low nibble of byte 0, high nibble of byte 1
        QTest::newRow("motorola cross byte") << 3 << 8 << true << false << 1.0 << 0.0
            << QByteArray::fromHex("05A0") << 0x5ALL << 90.0;
        // Motorola signed 12-bit from bit 39: byte 4 = AB, high nibble of byte 5 = C -> 0xABC
        QTest::newRow("motorola signed") << 39 << 12 << true << true << 0.5 << 0.0
            << QByteArray::fromHex("00000000ABC0") << -1348LL << -674.0;
        // Single bits
        QTest::newRow("bit 0") << 0 << 1 << false << false << 1.0 << 0.0
            << QByteArray::fromHex("01") << 1LL << 1.0;
        QTest::newRow("bit 63") << 63 << 1 << false << false << 1.0 << 0.0
            << QByteArray::fromHex("0000000000000080") << 1LL << 1.0;
        // Full 64-bit Intel
        QTest::newRow("intel u64") << 0 << 64 << false << false << 1.0 << 0.0
            << QByteArray::fromHex("0102030405060708") << 0x0807060504030201LL
            << static_cast<double>(0x0807060504030201LL);
        // 1-bit signed field reads as -1
        QTest::newRow("signed 1 bit") << 5 << 1 << false << true << 1.0 << 0.0
            << QByteArray::fromHex("20") << -1LL << -1.0;
        // Negative factor
        QTest::newRow("negative factor") << 0 << 8 << false << false << -0.5 << 10.0
            << QByteArray::fromHex("0A") << 10LL << 5.0;
    }
    void vectors()
    {
        QFETCH(int, start);
        QFETCH(int, length);
        QFETCH(bool, motorola);
        QFETCH(bool, isSigned);
        QFETCH(double, factor);
        QFETCH(double, offset);
        QFETCH(QByteArray, data);
        QFETCH(qlonglong, raw);
        QFETCH(double, physical);

        CanFrame f;
        f.dlc = 8;
        for (int i = 0; i < data.size(); ++i)
            f.data[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(data[i]);
        const SignalDef s = sig(start, length, motorola ? M : I, isSigned, factor, offset);
        const auto d = decodeOne(s, f);
        QCOMPARE(d.raw, raw);
        QVERIFY2(qAbs(d.physical - physical) < 1e-9, qPrintable(QString::number(d.physical, 'f', 6)));

        // And back: encoding the physical value reproduces the bits (a double
        // holds integers exactly only up to 2^53).
        if (length > 52)
            return;
        CanFrame g;
        g.dlc = 8;
        codec::encode(s, physical, g.data.data());
        QCOMPARE(codec::extractRaw(g.data.data(), start, length, s.byteOrder),
                 codec::extractRaw(f.data.data(), start, length, s.byteOrder));
    }

    void occupiedBitsOrder()
    {
        // Intel: listed MSB first.
        QCOMPARE(codec::occupiedBits(4, 6, I), (std::vector<int>{9, 8, 7, 6, 5, 4}));
        // Motorola sawtooth: 7..0 then 15..8
        QCOMPARE(codec::occupiedBits(7, 12, M), (std::vector<int>{7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12}));
        QVERIFY(codec::occupiedBits(60, 8, I).empty());  // beyond 64 bits
        QVERIFY(codec::occupiedBits(59, 8, M).empty());  // runs past byte 7
        QVERIFY(codec::fits(8, 8, I, 2));
        QVERIFY(!codec::fits(8, 16, I, 2));
    }

    void outOfRange()
    {
        const SignalDef s = sig(0, 8, I, false, 1, -40, -40, 100);
        QVERIFY(!decodeOne(s, frame({100})).outOfRange); // 60 degC
        QVERIFY(decodeOne(s, frame({200})).outOfRange);  // 160 > 100
        const SignalDef noRange = sig(0, 8, I);
        QVERIFY(!decodeOne(noRange, frame({255})).outOfRange); // [0|0] means unbounded
    }

    void encodeClampsAndRounds()
    {
        const SignalDef s = sig(0, 8, I, false, 0.5, 0);
        std::uint8_t data[8] = {};
        codec::encode(s, 1000.0, data); // far above 255*0.5
        QCOMPARE(int(data[0]), 255);
        codec::encode(s, -5.0, data);
        QCOMPARE(int(data[0]), 0);
        codec::encode(s, 10.26, data); // 20.52 -> 21
        QCOMPARE(int(data[0]), 21);
        const SignalDef sg = sig(0, 8, I, true, 1, 0);
        codec::encode(sg, -200.0, data);
        QCOMPARE(int(static_cast<std::int8_t>(data[0])), -128);
    }

    void encodeLeavesOtherBitsAlone()
    {
        std::uint8_t data[8];
        std::fill(std::begin(data), std::end(data), std::uint8_t{0xFF});
        codec::insertRaw(data, 4, 8, I, 0);
        QCOMPARE(int(data[0]), 0x0F);
        QCOMPARE(int(data[1]), 0xF0);
        QCOMPARE(int(data[2]), 0xFF);
    }

    void randomRoundTrip()
    {
        // Property test: insert then extract returns the same raw value for many
        // random layouts in both byte orders.
        std::mt19937 rng(1234);
        for (int n = 0; n < 5000; ++n) {
            const ByteOrder order = (rng() & 1) ? I : M;
            const int len = 1 + int(rng() % 64);
            const int start = int(rng() % 64);
            if (codec::occupiedBits(start, len, order).empty())
                continue;
            const std::uint64_t mask = len == 64 ? ~0ull : ((1ull << len) - 1);
            const std::uint64_t raw = ((std::uint64_t(rng()) << 32) | rng()) & mask;
            std::uint8_t data[8] = {};
            codec::insertRaw(data, start, len, order, raw);
            QCOMPARE(codec::extractRaw(data, start, len, order), raw);
        }
    }

    void valueTableText()
    {
        SignalDef s = sig(0, 2, I);
        s.valueTable = {{0, QStringLiteral("Off")}, {1, QStringLiteral("On")}};
        QCOMPARE(decodeOne(s, frame({1})).valueText, QStringLiteral("On"));
        QCOMPARE(decodeOne(s, frame({3})).valueText, QString());
    }

    void multiplexedDecode()
    {
        MessageDef m;
        m.dlc = 8;
        SignalDef mux = sig(0, 8, I);
        mux.name = QStringLiteral("Page");
        mux.muxRole = SignalDef::MuxRole::Multiplexor;
        SignalDef a = sig(8, 8, I);
        a.name = QStringLiteral("A");
        a.muxRole = SignalDef::MuxRole::Multiplexed;
        a.muxValue = 0;
        SignalDef b = sig(8, 16, I);
        b.name = QStringLiteral("B");
        b.muxRole = SignalDef::MuxRole::Multiplexed;
        b.muxValue = 1;
        m.signalList = {mux, a, b};

        auto out = Decoder::decode(m, frame({1, 0x34, 0x12, 0, 0, 0, 0, 0}));
        QCOMPARE(int(out.size()), 2);
        QCOMPARE(out[1].signalIndex, 2);
        QCOMPARE(out[1].raw, 0x1234LL);
        out = Decoder::decode(m, frame({0, 7, 0, 0, 0, 0, 0, 0}));
        QCOMPARE(out[1].signalIndex, 1);
        QCOMPARE(out[1].raw, 7LL);
    }

    void shortFrameSkipsSignals()
    {
        MessageDef m;
        m.dlc = 8;
        m.signalList = {sig(0, 8, I), sig(16, 16, I)};
        const auto out = Decoder::decode(m, frame({1, 2}));
        QCOMPARE(int(out.size()), 1);
    }

    void formatting()
    {
        QCOMPARE(codec::formatPhysical(sig(0, 8, I, false, 0.01), 46.6), QStringLiteral("46.60"));
        QCOMPARE(codec::formatPhysical(sig(0, 8, I, false, 1, -40), 40), QStringLiteral("40"));
        QCOMPARE(codec::formatPhysical(sig(0, 8, I, false, 0.25), 1165), QStringLiteral("1165.00"));
        QCOMPARE(codec::formatPhysical(sig(0, 8, I, false, 0.125, -40), 21.5), QStringLiteral("21.500"));
        QCOMPARE(codec::decimalsFor(sig(0, 8, I, false, 0.03125)), 4); // capped
    }
};

QTEST_GUILESS_MAIN(TstDecoder)
#include "tst_decoder.moc"
