#pragma once

#include "canframe.h"
#include "canlog.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

#include <memory>

class QThread;

namespace can {

class LogWriterWorker;

// Records every frame the active source delivers into a .canlog file.
//
// Threaded mode (default): encoding and disk writes happen on a worker
// thread; the GUI thread only hands over batches. The worker writes a block
// and flushes it to the OS every kFlushIntervalMs, so an abrupt stop (crash,
// power loss of the app) loses at most that much data and the file stays
// readable (see canlog::Reader truncation handling).
//
// Synchronous mode (tests, deterministic demo capture): the writer runs in
// the caller's thread and flushNow() is called by the owner.
class LogRecorder : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool recording READ isRecording NOTIFY recordingChanged)
    Q_PROPERTY(QString path READ path NOTIFY recordingChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY recordingChanged)
    Q_PROPERTY(qint64 durationUs READ durationUs NOTIFY progressChanged)
    Q_PROPERTY(qint64 bytesWritten READ bytesWritten NOTIFY progressChanged)
    Q_PROPERTY(qint64 framesWritten READ framesWritten NOTIFY progressChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorChanged)
    Q_PROPERTY(QString lastFile READ lastFile NOTIFY recordingChanged)

public:
    static constexpr int kFlushIntervalMs = 250;

    explicit LogRecorder(QObject *parent = nullptr);
    ~LogRecorder() override;

    void setSynchronous(bool synchronous); // only while not recording
    bool isSynchronous() const { return m_sync; }

    bool start(const QString &path, const canlog::Header &header);
    void append(const CanFrame *frames, std::size_t count);
    void flushNow();          // synchronous mode
    bool stop();

    bool isRecording() const { return m_recording; }
    QString path() const { return m_path; }
    QString fileName() const;
    QString lastFile() const { return m_lastFile; }
    qint64 durationUs() const { return m_durationUs; }
    qint64 bytesWritten() const { return m_bytes; }
    qint64 framesWritten() const { return m_frames; }
    QString lastError() const { return m_error; }

    // "can_2026-11-19_14-05-33.canlog" for the given local time.
    Q_INVOKABLE static QString defaultFileName(const QDateTime &localTime);

signals:
    void recordingChanged();
    void progressChanged();
    void errorChanged();
    void finished(const QString &path);

private:
    void onProgress(qint64 frames, qint64 bytes, qint64 firstUs, qint64 lastUs);
    void setError(const QString &error);

    bool m_sync = false;
    bool m_recording = false;
    QString m_path;
    QString m_lastFile;
    QString m_error;
    qint64 m_durationUs = 0;
    qint64 m_bytes = 0;
    qint64 m_frames = 0;
    std::int64_t m_baseUs = -1; // timestamp of the first recorded frame
    std::unique_ptr<canlog::Writer> m_syncWriter;
    QThread *m_thread = nullptr;
    QPointer<LogWriterWorker> m_worker;
};

} // namespace can
