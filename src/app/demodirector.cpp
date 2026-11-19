#include "demodirector.h"

#include "capturesession.h"

#include <QCoreApplication>
#include <QImage>
#include <QQuickWindow>
#include <QThreadPool>
#include <QVariant>

#include <algorithm>

DemoDirector::DemoDirector(QQuickWindow *window, can::CaptureSession *session,
                           const QString &outputDir, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_session(session)
{
    QDir root(outputDir);
    root.mkpath(QStringLiteral("frames"));
    root.mkpath(QStringLiteral("screenshots"));
    m_framesDir = QDir(root.filePath(QStringLiteral("frames")));
    m_shotsDir = QDir(root.filePath(QStringLiteral("screenshots")));
    // Start from a clean slate so old frames never mix with a new run.
    for (const QString &f : m_framesDir.entryList({QStringLiteral("*.png")}, QDir::Files))
        m_framesDir.remove(f);
    for (const QString &f : m_shotsDir.entryList({QStringLiteral("*.png")}, QDir::Files))
        m_shotsDir.remove(f);

    m_stepTimer.setInterval(10);
    m_stepTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_stepTimer, &QTimer::timeout, this, &DemoDirector::tick);
    m_frameTimer.setInterval(kFrameIntervalMs);
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_frameTimer, &QTimer::timeout, this, &DemoDirector::captureFrame);
    // Render-rate probe: how many frames the scene graph actually presents.
    connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_swaps; });
    buildScript();
}

void DemoDirector::act(const QString &action, const QVariant &arg)
{
    if (!m_window)
        return;
    QMetaObject::invokeMethod(m_window, "demoAction", Q_ARG(QVariant, QVariant(action)),
                              Q_ARG(QVariant, arg));
}

void DemoDirector::buildScript()
{
    auto at = [this](double seconds, std::function<void()> fn) {
        m_steps.push_back({static_cast<int>(seconds * 1000), std::move(fn)});
    };

    // Scene: light theme, start, trace, ID filter, pause/resume, messages,
    // dark theme and back. Screenshots are taken at representative moments.
    at(0.0, [this] {
        act(QStringLiteral("theme"), QStringLiteral("light"));
        act(QStringLiteral("tab"), 0);
    });
    at(1.2, [this] { m_session->start(); });
    at(5.0, [this] { screenshot(QStringLiteral("trace-live-light")); });

    // Type an ID filter character by character.
    const QString filter = QStringLiteral("123, 1A4");
    for (int i = 1; i <= filter.size(); ++i)
        at(6.0 + 0.12 * i, [this, f = filter.left(i)] { act(QStringLiteral("idFilter"), f); });

    at(9.0, [this] { m_session->setPaused(true); });
    at(9.6, [this] { act(QStringLiteral("selectRow"), 6); });
    at(11.0, [this] { screenshot(QStringLiteral("trace-filtered-paused-light")); });
    at(13.0, [this] { m_session->setPaused(false); });
    at(14.5, [this] { act(QStringLiteral("clearFilter")); });
    at(16.0, [this] { act(QStringLiteral("scrollUp"), 40); });
    at(18.0, [this] { act(QStringLiteral("jumpToLive")); });

    at(19.5, [this] { act(QStringLiteral("tab"), 1); });
    at(21.5, [this] { act(QStringLiteral("sort"), 3); }); // by count, largest first
    at(23.5, [this] { screenshot(QStringLiteral("messages-light")); });

    at(25.0, [this] { act(QStringLiteral("theme"), QStringLiteral("dark")); });
    at(27.5, [this] { screenshot(QStringLiteral("messages-dark")); });
    at(29.0, [this] { act(QStringLiteral("sort"), 0); }); // back to ID order
    at(31.0, [this] { act(QStringLiteral("tab"), 0); });
    at(34.0, [this] { screenshot(QStringLiteral("trace-live-dark")); });
    at(37.0, [this] { act(QStringLiteral("theme"), QStringLiteral("light")); });
    at(39.5, [this] { act(QStringLiteral("tab"), 1); });
    at(44.0, [this] { m_session->stop(); });
}

void DemoDirector::start()
{
    m_clock.start();
    m_stepTimer.start();
    m_frameTimer.start();
    tick();
}

void DemoDirector::tick()
{
    const qint64 now = m_clock.elapsed();
    if (now - m_lastLogMs >= 5000) {
        qInfo("t=%5.1fs  render %.1f fps  bus %.0f frames/s  rows %llu",
              now / 1000.0, m_swaps * 1000.0 / std::max<qint64>(1, now - m_lastLogMs),
              m_session->framesPerSecond(), m_session->totalFrames());
        m_swaps = 0;
        m_lastLogMs = now;
    }
    while (m_nextStep < m_steps.size() && m_steps[m_nextStep].atMs <= now)
        m_steps[m_nextStep++].run();
    if (now >= kDurationMs)
        finish();
}

void DemoDirector::captureFrame()
{
    if (!m_window)
        return;
    const QImage image = m_window->grabWindow();
    if (image.isNull())
        return;
    const QString path = m_framesDir.filePath(QStringLiteral("frame_%1.png").arg(m_frameIndex++, 4, 10, QLatin1Char('0')));
    QThreadPool::globalInstance()->start([image, path] { image.save(path, "PNG"); });
}

void DemoDirector::screenshot(const QString &name)
{
    if (!m_window)
        return;
    const QImage image = m_window->grabWindow();
    const QString path = m_shotsDir.filePath(QStringLiteral("%1-%2.png").arg(++m_shotIndex, 2, 10, QLatin1Char('0')).arg(name));
    image.save(path, "PNG");
}

void DemoDirector::finish()
{
    m_stepTimer.stop();
    m_frameTimer.stop();
    QThreadPool::globalInstance()->waitForDone();
    qInfo("demo finished: %d frames, %d screenshots", m_frameIndex, m_shotIndex);
    emit finished();
}
