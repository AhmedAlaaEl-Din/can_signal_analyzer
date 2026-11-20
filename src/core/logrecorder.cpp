#include "logrecorder.h"

#include <QDateTime>
#include <QFileInfo>
#include <QThread>
#include <QTimer>

namespace can {

// Lives in the recorder thread: owns the writer and flushes on a timer.
class LogWriterWorker : public QObject
{
    Q_OBJECT
public:
    LogWriterWorker() = default;

public slots:
    void begin(const QString &path, const can::canlog::Header &header)
    {
        QString error;
        if (!m_writer.open(path, header, &error)) {
            emit failed(error);
            return;
        }
        m_timer = new QTimer(this);
        m_timer->setInterval(LogRecorder::kFlushIntervalMs);
        connect(m_timer, &QTimer::timeout, this, &LogWriterWorker::flush);
        m_timer->start();
        report();
    }
    void append(const can::CanFrameBatch &batch)
    {
        m_writer.append(batch.constData(), static_cast<std::size_t>(batch.size()));
    }
    void flush()
    {
        if (!m_writer.isOpen())
            return;
        if (!m_writer.flush())
            emit failed(m_writer.errorString());
        report();
    }
    void end()
    {
        if (m_timer)
            m_timer->stop();
        if (m_writer.isOpen()) {
            if (!m_writer.close())
                emit failed(m_writer.errorString());
            report();
        }
    }

signals:
    void progress(qint64 frames, qint64 bytes, qint64 firstUs, qint64 lastUs);
    void failed(const QString &error);

private:
    void report()
    {
        emit progress(static_cast<qint64>(m_writer.framesWritten()), static_cast<qint64>(m_writer.bytesWritten()),
                      m_writer.firstTimestampUs(), m_writer.lastTimestampUs());
    }
    canlog::Writer m_writer;
    QTimer *m_timer = nullptr;
};

LogRecorder::LogRecorder(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<can::CanFrameBatch>("can::CanFrameBatch");
    qRegisterMetaType<can::canlog::Header>("can::canlog::Header");
}

LogRecorder::~LogRecorder()
{
    stop();
}

void LogRecorder::setSynchronous(bool synchronous)
{
    if (!m_recording)
        m_sync = synchronous;
}

QString LogRecorder::fileName() const
{
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).fileName();
}

QString LogRecorder::defaultFileName(const QDateTime &localTime)
{
    return QStringLiteral("can_%1.canlog").arg(localTime.toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss")));
}

bool LogRecorder::start(const QString &path, const canlog::Header &header)
{
    if (m_recording)
        stop();
    m_baseUs = -1;
    m_durationUs = 0;
    m_bytes = 0;
    m_frames = 0;
    m_path = path;
    setError({});
    if (m_sync) {
        m_syncWriter = std::make_unique<canlog::Writer>();
        QString error;
        if (!m_syncWriter->open(path, header, &error)) {
            m_syncWriter.reset();
            m_path.clear();
            setError(error);
            return false;
        }
        m_bytes = static_cast<qint64>(m_syncWriter->bytesWritten());
    } else {
        // Probe the location first so the user gets an immediate, clear error.
        {
            QFile probe(path);
            if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                setError(tr("Could not create the log file: %1").arg(probe.errorString()));
                m_path.clear();
                return false;
            }
        }
        m_thread = new QThread(this);
        m_thread->setObjectName(QStringLiteral("LogWriter"));
        auto *worker = new LogWriterWorker;
        worker->moveToThread(m_thread);
        m_worker = worker;
        connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(worker, &LogWriterWorker::progress, this, &LogRecorder::onProgress);
        connect(worker, &LogWriterWorker::failed, this, &LogRecorder::setError);
        m_thread->start();
        QMetaObject::invokeMethod(worker, "begin", Qt::QueuedConnection, Q_ARG(QString, path),
                                  Q_ARG(can::canlog::Header, header));
    }
    m_recording = true;
    emit recordingChanged();
    emit progressChanged();
    return true;
}

void LogRecorder::append(const CanFrame *frames, std::size_t count)
{
    if (!m_recording || count == 0)
        return;
    if (m_baseUs < 0)
        m_baseUs = frames[0].timestampUs;
    // Log timestamps start at 0 with the first recorded frame.
    CanFrameBatch batch;
    batch.reserve(static_cast<qsizetype>(count));
    for (std::size_t i = 0; i < count; ++i) {
        CanFrame f = frames[i];
        f.timestampUs -= m_baseUs;
        batch.append(f);
    }
    m_durationUs = batch.constLast().timestampUs;
    if (m_sync) {
        m_syncWriter->append(batch.constData(), count);
        m_frames = static_cast<qint64>(m_syncWriter->framesWritten());
    } else if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "append", Qt::QueuedConnection, Q_ARG(can::CanFrameBatch, batch));
        m_frames += static_cast<qint64>(count);
    }
    emit progressChanged();
}

void LogRecorder::flushNow()
{
    if (m_sync && m_syncWriter) {
        if (!m_syncWriter->flush())
            setError(m_syncWriter->errorString());
        m_bytes = static_cast<qint64>(m_syncWriter->bytesWritten());
        emit progressChanged();
    }
}

bool LogRecorder::stop()
{
    if (!m_recording)
        return true;
    bool ok = true;
    if (m_sync) {
        ok = m_syncWriter->close();
        if (!ok)
            setError(m_syncWriter->errorString());
        m_bytes = static_cast<qint64>(m_syncWriter->bytesWritten());
        m_syncWriter.reset();
    } else if (m_thread) {
        if (m_worker)
            QMetaObject::invokeMethod(m_worker, "end", Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        ok = m_error.isEmpty();
    }
    m_recording = false;
    m_lastFile = m_path;
    emit progressChanged();
    emit recordingChanged();
    emit finished(m_lastFile);
    return ok;
}

void LogRecorder::onProgress(qint64 frames, qint64 bytes, qint64, qint64)
{
    m_bytes = bytes;
    if (frames > m_frames)
        m_frames = frames;
    emit progressChanged();
}

void LogRecorder::setError(const QString &error)
{
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

} // namespace can

#include "logrecorder.moc"
