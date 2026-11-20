#include "ringbuffer.h"

#include <QtTest>

using can::RingBuffer;

class TstRingBuffer : public QObject
{
    Q_OBJECT
private slots:
    void startsEmpty()
    {
        RingBuffer<int> rb(4);
        QCOMPARE(rb.capacity(), std::size_t(4));
        QCOMPARE(rb.size(), std::size_t(0));
        QVERIFY(rb.empty());
        QCOMPARE(rb.firstSeq(), 0u);
        QCOMPARE(rb.nextSeq(), 0u);
    }

    void fillsInOrder()
    {
        RingBuffer<int> rb(4);
        for (int i = 0; i < 3; ++i)
            QVERIFY(!rb.push(i * 10));
        QCOMPARE(rb.size(), std::size_t(3));
        QCOMPARE(rb.at(0), 0);
        QCOMPARE(rb.at(2), 20);
        QVERIFY(!rb.full());
    }

    void overwritesOldest()
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.push(3);
        QVERIFY(rb.full());
        QVERIFY(rb.push(4)); // evicts 1
        QCOMPARE(rb.size(), std::size_t(3));
        QCOMPARE(rb.at(0), 2);
        QCOMPARE(rb.at(1), 3);
        QCOMPARE(rb.at(2), 4);
        QCOMPARE(rb.firstSeq(), 1u);
        QCOMPARE(rb.nextSeq(), 4u);
    }

    void sequenceLookup()
    {
        RingBuffer<int> rb(5);
        for (int i = 0; i < 12; ++i)
            rb.push(100 + i);
        // holds seq 7..11
        QVERIFY(!rb.containsSeq(6));
        QVERIFY(rb.containsSeq(7));
        QVERIFY(rb.containsSeq(11));
        QVERIFY(!rb.containsSeq(12));
        QCOMPARE(rb.bySeq(7), 107);
        QCOMPARE(rb.bySeq(11), 111);
    }

    void wrapsManyTimes()
    {
        RingBuffer<int> rb(7);
        for (int i = 0; i < 1000; ++i)
            rb.push(i);
        QCOMPARE(rb.size(), std::size_t(7));
        for (std::size_t i = 0; i < 7; ++i)
            QCOMPARE(rb.at(i), 993 + int(i));
    }

    void clearKeepsSequenceMonotonic()
    {
        RingBuffer<int> rb(3);
        rb.push(1);
        rb.push(2);
        rb.clear();
        QVERIFY(rb.empty());
        QCOMPARE(rb.firstSeq(), 2u);
        rb.push(9);
        QCOMPARE(rb.bySeq(2), 9);
        QVERIFY(!rb.containsSeq(0));
    }

    void zeroCapacityIsClamped()
    {
        RingBuffer<int> rb(0);
        QCOMPARE(rb.capacity(), std::size_t(1));
        rb.push(1);
        QVERIFY(rb.push(2));
        QCOMPARE(rb.at(0), 2);
    }

    void setCapacityResets()
    {
        RingBuffer<int> rb(2);
        rb.push(1);
        rb.setCapacity(10);
        QCOMPARE(rb.capacity(), std::size_t(10));
        QVERIFY(rb.empty());
    }

    void defaultCapacity()
    {
        RingBuffer<int> rb;
        QCOMPARE(rb.capacity(), std::size_t(100000));
    }
};

QTEST_GUILESS_MAIN(TstRingBuffer)
#include "tst_ringbuffer.moc"
