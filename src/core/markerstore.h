#pragma once

#include <QAbstractListModel>
#include <QString>

#include <vector>

namespace can {

// Named time markers for a replayed log, sorted by time. When a sidecar path
// is set ("<log>.markers.json") every change is saved immediately, and the
// markers are loaded again the next time the log is opened.
class MarkerStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)

public:
    enum Role { TimeRole = Qt::UserRole + 1, NameRole, NumberRole };

    struct Marker
    {
        qint64 timeUs = 0;
        QString name;
    };

    explicit MarkerStore(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_markers.size()); }
    const std::vector<Marker> &markers() const { return m_markers; }
    QString lastError() const { return m_error; }

    static QString sidecarPathFor(const QString &logPath);
    // Loads the sidecar (missing file = no markers). Returns false on a damaged file.
    bool attach(const QString &logPath);
    void detach();
    QString sidecarPath() const { return m_sidecar; }

    // Returns the row of the new marker. An empty name becomes "Marker N".
    Q_INVOKABLE int add(qint64 timeUs, const QString &name = {});
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void rename(int row, const QString &name);
    Q_INVOKABLE void clear();
    Q_INVOKABLE qint64 timeAt(int row) const;
    // Next marker strictly after / previous strictly before timeUs (-1 = none).
    Q_INVOKABLE qint64 nextAfter(qint64 timeUs) const;
    Q_INVOKABLE qint64 previousBefore(qint64 timeUs) const;
    Q_INVOKABLE QVariantList all() const;

    bool save() const;
    static bool saveTo(const QString &path, const std::vector<Marker> &markers, QString *error);
    static bool loadFrom(const QString &path, std::vector<Marker> &markers, QString *error);

signals:
    void countChanged();
    void errorChanged();

private:
    void changed();
    void setError(const QString &e);

    std::vector<Marker> m_markers;
    QString m_sidecar;
    QString m_error;
    int m_nextNumber = 1;
};

} // namespace can
