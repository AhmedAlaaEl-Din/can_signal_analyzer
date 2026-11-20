#pragma once

#include "dbc.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

#include <memory>

namespace can {

// Owns the currently loaded signal database (or none) and reports load
// problems in a user-friendly form. The parsed Database is immutable and
// shared with the models through std::shared_ptr.
class SignalDatabase : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY changed)
    Q_PROPERTY(bool isDemo READ isDemo NOTIFY changed)
    Q_PROPERTY(QString name READ name NOTIFY changed)
    Q_PROPERTY(QString path READ path NOTIFY changed)
    Q_PROPERTY(int messageCount READ messageCount NOTIFY changed)
    Q_PROPERTY(int signalCount READ signalCount NOTIFY changed)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY changed)
    Q_PROPERTY(QString errorTitle READ errorTitle NOTIFY errorChanged)
    Q_PROPERTY(QStringList errorLines READ errorLines NOTIFY errorChanged)

public:
    explicit SignalDatabase(QObject *parent = nullptr);

    std::shared_ptr<const Database> database() const { return m_db; }
    bool isLoaded() const { return m_db != nullptr; }
    bool isDemo() const { return m_isDemo; }
    QString path() const { return m_path; } // file of the loaded database ("" for the demo)
    QString name() const { return m_db ? m_db->sourceName : QString(); }
    int messageCount() const { return m_db ? static_cast<int>(m_db->messages.size()) : 0; }
    int signalCount() const { return m_db ? m_db->signalCount() : 0; }
    int warningCount() const { return m_warningCount; }
    QString errorTitle() const { return m_errorTitle; }
    QStringList errorLines() const { return m_errorLines; }

    // Returns false (and keeps the previous database) when the file is invalid.
    Q_INVOKABLE bool loadFile(const QUrl &fileUrl);
    bool loadPath(const QString &path);
    Q_INVOKABLE void loadDemo();
    Q_INVOKABLE void unload();
    Q_INVOKABLE void dismissError();

signals:
    void changed();
    void errorChanged();

private:
    void setDatabase(std::shared_ptr<const Database> db, bool demo, int warnings);

    std::shared_ptr<const Database> m_db;
    bool m_isDemo = false;
    QString m_path;
    int m_warningCount = 0;
    QString m_errorTitle;
    QStringList m_errorLines;
};

} // namespace can
