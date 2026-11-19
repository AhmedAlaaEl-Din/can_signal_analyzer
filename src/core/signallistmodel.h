#pragma once

#include "canframe.h"
#include "dbc.h"

#include <QAbstractListModel>
#include <QVariantMap>

#include <array>
#include <memory>
#include <vector>

namespace can {

// Live view of every signal in the loaded database, grouped by message.
//
// The model is a flat list of rows: a header row per message group followed by
// its signal rows. A "Pinned" group at the top holds pinned signals. A search
// filter and collapsed groups change which rows exist (model reset); live
// values only emit dataChanged.
//
// Each signal keeps a short history (about 12 s of simulated time, one sample
// per 40 ms bucket) that the sparkline item reads directly through history().
class SignalListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int signalCount READ signalCount NOTIFY structureChanged)
    Q_PROPERTY(int visibleSignalCount READ visibleSignalCount NOTIFY structureChanged)
    Q_PROPERTY(int pinnedCount READ pinnedCount NOTIFY structureChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY structureChanged)
    Q_PROPERTY(qint64 timeUs READ timeUs NOTIFY timeChanged)

public:
    static constexpr std::int64_t kHistoryWindowUs = 10'000'000; // shown in sparklines
    static constexpr std::int64_t kSampleBucketUs = 40'000;
    static constexpr int kHistoryCapacity = 300;                  // 12 s of buckets

    enum Role {
        KindRole = Qt::UserRole + 1, // "header" | "signal"
        NameRole,
        MessageNameRole,
        FrameIdRole,        // hex text of the message ID
        SignalIdRole,       // global signal index (signal rows)
        MessageIndexRole,
        GroupSizeRole,      // header: number of visible signals
        CollapsedRole,
        PinnedSectionRole,  // header of the pinned group
        PinnedRole,
        HasValueRole,
        ValueRole,          // formatted physical value
        PhysicalRole,       // double
        UnitRole,
        EnumTextRole,
        RawRole,
        MinRole,            // formatted min seen
        MaxRole,
        OutOfRangeRole,
        RevisionRole,       // increments whenever the history changes
        BitInfoRole,        // "start 0 · 16 bit · Intel"
    };

    // One 40 ms bucket: the newest value plus the min/max seen inside the
    // bucket (so fast-changing signals such as counters draw an honest band
    // instead of an aliased zig-zag).
    struct Sample
    {
        std::int64_t timeUs;
        double value;
        double min;
        double max;
    };

    explicit SignalListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDatabase(std::shared_ptr<const Database> db);
    std::shared_ptr<const Database> database() const { return m_db; }

    void update(const CanFrame *frames, std::size_t count);
    void update(const std::vector<CanFrame> &frames) { update(frames.data(), frames.size()); }
    Q_INVOKABLE void clearValues();

    int signalCount() const { return static_cast<int>(m_signals.size()); }
    int visibleSignalCount() const;
    int pinnedCount() const { return static_cast<int>(m_pinOrder.size()); }
    QString filterText() const { return m_filter; }
    void setFilterText(const QString &text);
    qint64 timeUs() const { return m_nowUs; }

    Q_INVOKABLE void toggleGroup(int messageIndex);
    Q_INVOKABLE void setAllCollapsed(bool collapsed);
    Q_INVOKABLE void togglePin(int signalId);
    Q_INVOKABLE bool isPinned(int signalId) const;
    Q_INVOKABLE int signalIdByName(const QString &messageName, const QString &signalName) const;
    Q_INVOKABLE int rowOfSignal(int signalId) const;
    // Everything the detail panel shows, including the bit layout.
    Q_INVOKABLE QVariantMap signalDetails(int signalId) const;
    // Current value text (with unit/enum) for the detail panel.
    Q_INVOKABLE QVariantMap liveValue(int signalId) const;

    // History for sparklines, oldest first (ring buffer unrolled into `out`).
    void history(int signalId, std::vector<Sample> &out) const;
    std::uint64_t revision(int signalId) const;

signals:
    void structureChanged();
    void timeChanged();
    void valuesChanged();

private:
    struct SignalState
    {
        int message = 0;
        int signal = 0;
        bool hasValue = false;
        double physical = 0;
        std::int64_t raw = 0;
        double min = 0;
        double max = 0;
        bool outOfRange = false;
        std::vector<Sample> ring;   // capacity kHistoryCapacity
        int ringStart = 0;
        int ringSize = 0;
        std::uint64_t revision = 0;
    };
    struct Row
    {
        bool header;
        bool pinnedSection;
        int message;     // header: message index (-1 for the pinned header)
        int signalId;    // signal rows
    };
    struct MessageState
    {
        std::array<std::uint8_t, 8> data{};
        int dlc = -1;    // -1 = never seen
        int firstSignal = 0;
    };

    void rebuildRows();
    bool matches(const SignalState &s) const;
    const SignalDef &def(int signalId) const;
    void pushSample(SignalState &s, std::int64_t t, double v);

    std::shared_ptr<const Database> m_db;
    std::vector<SignalState> m_signals;
    std::vector<MessageState> m_messages;
    std::vector<Row> m_rows;
    std::vector<int> m_rowOfSignal;
    std::vector<bool> m_collapsed;
    std::vector<int> m_pinOrder;
    QString m_filter;
    std::int64_t m_nowUs = 0;
};

} // namespace can
