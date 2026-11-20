#include "signallistmodel.h"

#include "signalcodec.h"
#include "timeseriesstore.h"

#include <algorithm>

namespace can {

SignalListModel::SignalListModel(QObject *parent)
    : QAbstractListModel(parent)
{}

int SignalListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QHash<int, QByteArray> SignalListModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {NameRole, "name"},
        {MessageNameRole, "messageName"},
        {FrameIdRole, "frameId"},
        {SignalIdRole, "signalId"},
        {MessageIndexRole, "messageIndex"},
        {GroupSizeRole, "groupSize"},
        {CollapsedRole, "collapsed"},
        {PinnedSectionRole, "pinnedSection"},
        {PinnedRole, "pinned"},
        {HasValueRole, "hasValue"},
        {ValueRole, "value"},
        {PhysicalRole, "physical"},
        {UnitRole, "unit"},
        {EnumTextRole, "enumText"},
        {RawRole, "raw"},
        {MinRole, "minSeen"},
        {MaxRole, "maxSeen"},
        {OutOfRangeRole, "outOfRange"},
        {RevisionRole, "revision"},
        {BitInfoRole, "bitInfo"},
    };
}

const SignalDef &SignalListModel::def(int signalId) const
{
    const SignalState &s = m_signals[static_cast<std::size_t>(signalId)];
    return m_db->messages[static_cast<std::size_t>(s.message)].signalList[static_cast<std::size_t>(s.signal)];
}

QVariant SignalListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount() || !m_db)
        return {};
    const Row &r = m_rows[static_cast<std::size_t>(index.row())];

    if (r.header) {
        switch (role) {
        case KindRole: return QStringLiteral("header");
        case PinnedSectionRole: return r.pinnedSection;
        case MessageIndexRole: return r.message;
        case CollapsedRole:
            return !r.pinnedSection && m_filter.isEmpty() && m_collapsed[static_cast<std::size_t>(r.message)];
        case GroupSizeRole: {
            if (r.pinnedSection)
                return pinnedCount();
            int n = 0;
            for (const auto &s : m_signals)
                n += (s.message == r.message && matches(s) && !isPinned(int(&s - m_signals.data())));
            return n;
        }
        case NameRole:
        case MessageNameRole:
            return r.pinnedSection ? QStringLiteral("Pinned")
                                   : m_db->messages[static_cast<std::size_t>(r.message)].name;
        case FrameIdRole: {
            if (r.pinnedSection)
                return QString();
            const MessageDef &m = m_db->messages[static_cast<std::size_t>(r.message)];
            return formatId(m.id, m.extended);
        }
        default: return {};
        }
    }

    const SignalState &s = m_signals[static_cast<std::size_t>(r.signalId)];
    const SignalDef &d = def(r.signalId);
    const MessageDef &m = m_db->messages[static_cast<std::size_t>(s.message)];
    switch (role) {
    case KindRole: return QStringLiteral("signal");
    case NameRole: return d.name;
    case MessageNameRole: return m.name;
    case FrameIdRole: return formatId(m.id, m.extended);
    case SignalIdRole: return r.signalId;
    case MessageIndexRole: return s.message;
    case PinnedRole: return isPinned(r.signalId);
    case PinnedSectionRole: return r.pinnedSection;
    case HasValueRole: return s.hasValue;
    case ValueRole: return s.hasValue ? codec::formatPhysical(d, s.physical) : QStringLiteral("—");
    case PhysicalRole: return s.physical;
    case UnitRole: return d.unit;
    case EnumTextRole: {
        if (!s.hasValue)
            return QString();
        auto it = d.valueTable.find(s.raw);
        return it == d.valueTable.end() ? QString() : it->second;
    }
    case RawRole: return QVariant::fromValue<qlonglong>(s.raw);
    case MinRole: return s.hasValue ? codec::formatPhysical(d, s.min) : QString();
    case MaxRole: return s.hasValue ? codec::formatPhysical(d, s.max) : QString();
    case OutOfRangeRole: return s.outOfRange;
    case RevisionRole: return QVariant::fromValue<qulonglong>(s.revision);
    case BitInfoRole:
        return QStringLiteral("%1|%2 %3%4")
            .arg(d.startBit).arg(d.length)
            .arg(d.byteOrder == ByteOrder::Intel ? QStringLiteral("Intel") : QStringLiteral("Motorola"),
                 d.isSigned ? QStringLiteral(" signed") : QString());
    default: return {};
    }
}

void SignalListModel::setDatabase(std::shared_ptr<const Database> db)
{
    beginResetModel();
    m_db = std::move(db);
    m_signals.clear();
    m_messages.clear();
    m_pinOrder.clear();
    m_collapsed.clear();
    if (m_db) {
        m_messages.resize(m_db->messages.size());
        m_collapsed.assign(m_db->messages.size(), false);
        for (std::size_t mi = 0; mi < m_db->messages.size(); ++mi) {
            m_messages[mi].firstSignal = static_cast<int>(m_signals.size());
            for (std::size_t si = 0; si < m_db->messages[mi].signalList.size(); ++si) {
                SignalState s;
                s.message = static_cast<int>(mi);
                s.signal = static_cast<int>(si);
                s.ring.resize(kHistoryCapacity);
                m_signals.push_back(std::move(s));
            }
        }
    }
    m_nowUs = 0;
    if (m_store)
        m_store->reset(static_cast<int>(m_signals.size()));
    rebuildRows();
    endResetModel();
    emit structureChanged();
    emit timeChanged();
}

bool SignalListModel::matches(const SignalState &s) const
{
    if (m_filter.isEmpty())
        return true;
    const MessageDef &m = m_db->messages[static_cast<std::size_t>(s.message)];
    const SignalDef &d = m.signalList[static_cast<std::size_t>(s.signal)];
    return d.name.contains(m_filter, Qt::CaseInsensitive) || m.name.contains(m_filter, Qt::CaseInsensitive)
           || d.unit.compare(m_filter, Qt::CaseInsensitive) == 0;
}

void SignalListModel::rebuildRows()
{
    m_rows.clear();
    m_rowOfSignal.assign(m_signals.size(), -1);
    if (!m_db)
        return;
    // Pinned group first.
    std::vector<int> pinned;
    for (int id : m_pinOrder)
        if (matches(m_signals[static_cast<std::size_t>(id)]))
            pinned.push_back(id);
    if (!pinned.empty()) {
        m_rows.push_back({true, true, -1, -1});
        for (int id : pinned) {
            m_rowOfSignal[static_cast<std::size_t>(id)] = static_cast<int>(m_rows.size());
            m_rows.push_back({false, true, -1, id});
        }
    }
    for (std::size_t mi = 0; mi < m_db->messages.size(); ++mi) {
        std::vector<int> members;
        const int first = m_messages[mi].firstSignal;
        for (std::size_t si = 0; si < m_db->messages[mi].signalList.size(); ++si) {
            const int id = first + static_cast<int>(si);
            if (matches(m_signals[static_cast<std::size_t>(id)]) && !isPinned(id))
                members.push_back(id);
        }
        if (members.empty())
            continue;
        m_rows.push_back({true, false, static_cast<int>(mi), -1});
        if (m_filter.isEmpty() && m_collapsed[mi])
            continue; // searching always shows the matches
        for (int id : members) {
            m_rowOfSignal[static_cast<std::size_t>(id)] = static_cast<int>(m_rows.size());
            m_rows.push_back({false, false, static_cast<int>(mi), id});
        }
    }
}

int SignalListModel::visibleSignalCount() const
{
    int n = 0;
    for (const auto &s : m_signals)
        n += matches(s);
    return n;
}

void SignalListModel::setFilterText(const QString &text)
{
    const QString t = text.trimmed();
    if (t == m_filter)
        return;
    beginResetModel();
    m_filter = t;
    rebuildRows();
    endResetModel();
    emit structureChanged();
}

void SignalListModel::toggleGroup(int messageIndex)
{
    if (messageIndex < 0 || messageIndex >= static_cast<int>(m_collapsed.size()))
        return;
    beginResetModel();
    m_collapsed[static_cast<std::size_t>(messageIndex)] = !m_collapsed[static_cast<std::size_t>(messageIndex)];
    rebuildRows();
    endResetModel();
    emit structureChanged();
}

void SignalListModel::setAllCollapsed(bool collapsed)
{
    beginResetModel();
    std::fill(m_collapsed.begin(), m_collapsed.end(), collapsed);
    rebuildRows();
    endResetModel();
    emit structureChanged();
}

bool SignalListModel::isPinned(int signalId) const
{
    return std::find(m_pinOrder.begin(), m_pinOrder.end(), signalId) != m_pinOrder.end();
}

void SignalListModel::togglePin(int signalId)
{
    if (signalId < 0 || signalId >= signalCount())
        return;
    beginResetModel();
    auto it = std::find(m_pinOrder.begin(), m_pinOrder.end(), signalId);
    if (it == m_pinOrder.end())
        m_pinOrder.push_back(signalId);
    else
        m_pinOrder.erase(it);
    rebuildRows();
    endResetModel();
    emit structureChanged();
}

int SignalListModel::signalIdByName(const QString &messageName, const QString &signalName) const
{
    for (std::size_t i = 0; i < m_signals.size(); ++i) {
        const auto &s = m_signals[i];
        const MessageDef &m = m_db->messages[static_cast<std::size_t>(s.message)];
        if ((messageName.isEmpty() || m.name == messageName)
            && m.signalList[static_cast<std::size_t>(s.signal)].name == signalName)
            return static_cast<int>(i);
    }
    return -1;
}

int SignalListModel::rowOfSignal(int signalId) const
{
    if (signalId < 0 || signalId >= static_cast<int>(m_rowOfSignal.size()))
        return -1;
    return m_rowOfSignal[static_cast<std::size_t>(signalId)];
}

void SignalListModel::pushSample(SignalState &s, std::int64_t t, double v)
{
    if (s.ringSize > 0) {
        const int lastIdx = (s.ringStart + s.ringSize - 1) % kHistoryCapacity;
        Sample &last = s.ring[static_cast<std::size_t>(lastIdx)];
        if (t / kSampleBucketUs == last.timeUs / kSampleBucketUs) {
            // same bucket: keep the newest value, widen the envelope
            last.timeUs = t;
            last.value = v;
            last.min = std::min(last.min, v);
            last.max = std::max(last.max, v);
            return;
        }
    }
    if (s.ringSize < kHistoryCapacity) {
        s.ring[static_cast<std::size_t>((s.ringStart + s.ringSize) % kHistoryCapacity)] = {t, v, v, v};
        ++s.ringSize;
    } else {
        s.ring[static_cast<std::size_t>(s.ringStart)] = {t, v, v, v};
        s.ringStart = (s.ringStart + 1) % kHistoryCapacity;
    }
    ++s.revision;
}

void SignalListModel::update(const CanFrame *frames, std::size_t n)
{
    if (!m_db || n == 0)
        return;
    int firstRow = -1;
    int lastRow = -1;
    for (std::size_t i = 0; i < n; ++i) {
        const CanFrame &f = frames[i];
        const int mi = m_db->indexOf(f.key());
        if (mi < 0)
            continue;
        const MessageDef &m = m_db->messages[static_cast<std::size_t>(mi)];
        MessageState &ms = m_messages[static_cast<std::size_t>(mi)];
        ms.data = f.data;
        ms.dlc = f.dlc;
        m_nowUs = std::max(m_nowUs, f.timestampUs);
        for (const DecodedSignal &d : Decoder::decode(m, f)) {
            const int id = ms.firstSignal + d.signalIndex;
            SignalState &s = m_signals[static_cast<std::size_t>(id)];
            if (!s.hasValue) {
                s.min = s.max = d.physical;
            } else {
                s.min = std::min(s.min, d.physical);
                s.max = std::max(s.max, d.physical);
            }
            s.hasValue = true;
            s.physical = d.physical;
            s.raw = d.raw;
            s.outOfRange = d.outOfRange;
            pushSample(s, f.timestampUs, d.physical);
            if (m_store)
                m_store->append(id, f.timestampUs, d.physical);
            const int row = m_rowOfSignal[static_cast<std::size_t>(id)];
            if (row >= 0) {
                firstRow = firstRow < 0 ? row : std::min(firstRow, row);
                lastRow = std::max(lastRow, row);
            }
        }
    }
    if (m_store)
        m_store->commit();
    if (firstRow >= 0)
        emit dataChanged(index(firstRow), index(lastRow),
                         {HasValueRole, ValueRole, PhysicalRole, EnumTextRole, RawRole, MinRole, MaxRole,
                          OutOfRangeRole, RevisionRole});
    emit timeChanged();
    emit valuesChanged();
}

void SignalListModel::clearValues()
{
    for (auto &s : m_signals) {
        s.hasValue = false;
        s.ringSize = 0;
        s.ringStart = 0;
        s.outOfRange = false;
        ++s.revision;
    }
    for (auto &m : m_messages) {
        m.dlc = -1;
        m.data = {};
    }
    m_nowUs = 0;
    if (m_store)
        m_store->clear();
    if (!m_rows.empty())
        emit dataChanged(index(0), index(rowCount() - 1));
    emit timeChanged();
    emit valuesChanged();
}

void SignalListModel::history(int signalId, std::vector<Sample> &out) const
{
    out.clear();
    if (signalId < 0 || signalId >= signalCount())
        return;
    const SignalState &s = m_signals[static_cast<std::size_t>(signalId)];
    out.reserve(static_cast<std::size_t>(s.ringSize));
    for (int i = 0; i < s.ringSize; ++i)
        out.push_back(s.ring[static_cast<std::size_t>((s.ringStart + i) % kHistoryCapacity)]);
}

std::uint64_t SignalListModel::revision(int signalId) const
{
    if (signalId < 0 || signalId >= signalCount())
        return 0;
    return m_signals[static_cast<std::size_t>(signalId)].revision;
}

QVariantMap SignalListModel::liveValue(int signalId) const
{
    QVariantMap v;
    if (signalId < 0 || signalId >= signalCount())
        return v;
    const SignalState &s = m_signals[static_cast<std::size_t>(signalId)];
    const SignalDef &d = def(signalId);
    v[QStringLiteral("hasValue")] = s.hasValue;
    v[QStringLiteral("value")] = s.hasValue ? codec::formatPhysical(d, s.physical) : QStringLiteral("—");
    v[QStringLiteral("raw")] = QVariant::fromValue<qlonglong>(s.raw);
    auto it = d.valueTable.find(s.raw);
    v[QStringLiteral("enumText")] = s.hasValue && it != d.valueTable.end() ? it->second : QString();
    v[QStringLiteral("min")] = s.hasValue ? codec::formatPhysical(d, s.min) : QString();
    v[QStringLiteral("max")] = s.hasValue ? codec::formatPhysical(d, s.max) : QString();
    v[QStringLiteral("outOfRange")] = s.outOfRange;
    // Current payload of the message, for the live bit layout.
    const MessageState &ms = m_messages[static_cast<std::size_t>(s.message)];
    QVariantList bytes;
    for (int i = 0; i < 8; ++i)
        bytes << (i < ms.dlc ? int(ms.data[static_cast<std::size_t>(i)]) : -1);
    v[QStringLiteral("bytes")] = bytes;
    return v;
}

QVariantMap SignalListModel::signalDetails(int signalId) const
{
    QVariantMap v;
    if (!m_db || signalId < 0 || signalId >= signalCount())
        return v;
    const SignalState &s = m_signals[static_cast<std::size_t>(signalId)];
    const MessageDef &m = m_db->messages[static_cast<std::size_t>(s.message)];
    const SignalDef &d = def(signalId);
    v[QStringLiteral("signalId")] = signalId;
    v[QStringLiteral("name")] = d.name;
    v[QStringLiteral("message")] = m.name;
    v[QStringLiteral("frameId")] = formatId(m.id, m.extended);
    v[QStringLiteral("extended")] = m.extended;
    v[QStringLiteral("dlc")] = m.dlc;
    v[QStringLiteral("cycleTimeMs")] = m.cycleTimeMs;
    v[QStringLiteral("startBit")] = d.startBit;
    v[QStringLiteral("length")] = d.length;
    v[QStringLiteral("byteOrder")] = d.byteOrder == ByteOrder::Intel ? QStringLiteral("Intel (little endian)")
                                                                      : QStringLiteral("Motorola (big endian)");
    v[QStringLiteral("intel")] = d.byteOrder == ByteOrder::Intel;
    v[QStringLiteral("signed")] = d.isSigned;
    v[QStringLiteral("factor")] = QString::number(d.factor, 'g', 10);
    v[QStringLiteral("offset")] = QString::number(d.offset, 'g', 10);
    v[QStringLiteral("hasRange")] = d.hasRange();
    v[QStringLiteral("minimum")] = codec::formatPhysical(d, d.minimum);
    v[QStringLiteral("maximum")] = codec::formatPhysical(d, d.maximum);
    v[QStringLiteral("unit")] = d.unit;
    v[QStringLiteral("receivers")] = d.receivers.join(QStringLiteral(", "));
    v[QStringLiteral("transmitter")] = m.transmitter;
    v[QStringLiteral("comment")] = d.comment;
    QString mux;
    if (d.muxRole == SignalDef::MuxRole::Multiplexor)
        mux = QStringLiteral("Multiplexor");
    else if (d.muxRole == SignalDef::MuxRole::Multiplexed)
        mux = QStringLiteral("Multiplexed, page %1").arg(d.muxValue);
    v[QStringLiteral("multiplexing")] = mux;
    QVariantList table;
    for (const auto &[raw, text] : d.valueTable)
        table << QVariantMap{{QStringLiteral("value"), QVariant::fromValue<qlonglong>(raw)}, {QStringLiteral("text"), text}};
    v[QStringLiteral("valueTable")] = table;
    // Bit layout: data-bit indices from MSB to LSB of the raw value.
    QVariantList bits;
    for (int b : codec::occupiedBits(d.startBit, d.length, d.byteOrder))
        bits << b;
    v[QStringLiteral("bits")] = bits;
    v[QStringLiteral("pinned")] = isPinned(signalId);
    return v;
}

} // namespace can
