#include "demodirector.h"

#include "capturesession.h"

#include <QCoreApplication>
#include <QImage>
#include <QQuickWindow>
#include <QThreadPool>
#include <QVariant>

#include <algorithm>

DemoDirector::DemoDirector(Mode mode, QQuickWindow *window, can::CaptureSession *session,
                           const QString &outputDir, QObject *parent)
    : QObject(parent)
    , m_mode(mode)
    , m_window(window)
    , m_session(session)
{
    const QDir root(outputDir);
    if (m_mode == Mode::Demo) {
        m_framesDir = cleanDir(root, QStringLiteral("frames"));
        m_shotsDir = cleanDir(root, QStringLiteral("screenshots"));
        m_durationMs = kDemoDurationMs;
        buildDemoScript();
    } else {
        m_shotsDir = cleanDir(root, QStringLiteral("checks"));
        m_durationMs = kCheckDurationMs;
        buildCheckScript();
    }

    m_stepTimer.setInterval(10);
    m_stepTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_stepTimer, &QTimer::timeout, this, &DemoDirector::tick);
    m_frameTimer.setInterval(kFrameIntervalMs);
    m_frameTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_frameTimer, &QTimer::timeout, this, &DemoDirector::captureFrame);
    // Render-rate probe: how many frames the scene graph actually presents.
    connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_swaps; });
}

QDir DemoDirector::cleanDir(const QDir &root, const QString &name)
{
    root.mkpath(name);
    QDir dir(root.filePath(name));
    // Start from a clean slate so images from an older run never mix in.
    for (const QString &f : dir.entryList({QStringLiteral("*.png")}, QDir::Files))
        dir.remove(f);
    return dir;
}

void DemoDirector::at(double seconds, std::function<void()> fn)
{
    m_steps.push_back({static_cast<int>(seconds * 1000), std::move(fn)});
}

void DemoDirector::act(const QString &action, const QVariant &arg)
{
    if (!m_window)
        return;
    QMetaObject::invokeMethod(m_window, "demoAction", Q_ARG(QVariant, QVariant(action)),
                              Q_ARG(QVariant, arg));
}

void DemoDirector::resizeWindow(int width, int height)
{
    if (m_window)
        m_window->resize(width, height);
}

void DemoDirector::buildDemoScript()
{
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
    // A slow ID range: new rows arrive rarely enough to show the fade-in.
    at(14.0, [this] { act(QStringLiteral("idFilter"), QStringLiteral("601-6FF")); });
    at(16.5, [this] { act(QStringLiteral("clearFilter")); });
    at(17.0, [this] { act(QStringLiteral("scrollUp"), 40); });
    at(18.6, [this] { act(QStringLiteral("jumpToLive")); });

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

void DemoDirector::buildCheckScript()
{
    at(0.0, [this] {
        act(QStringLiteral("theme"), QStringLiteral("dark"));
        act(QStringLiteral("tab"), 0);
    });
    at(1.0, [this] { screenshot(QStringLiteral("empty-trace-dark")); });
    at(1.3, [this] { act(QStringLiteral("tab"), 1); });
    at(2.0, [this] { screenshot(QStringLiteral("empty-messages-dark")); });
    at(2.2, [this] {
        act(QStringLiteral("tab"), 0);
        m_session->start();
    });
    at(3.0, [this] { resizeWindow(960, 600); }); // the window's minimum size
    at(4.5, [this] { screenshot(QStringLiteral("min-size-trace-dark")); });
    at(4.7, [this] { act(QStringLiteral("tab"), 1); });
    at(6.0, [this] { screenshot(QStringLiteral("min-size-messages-dark")); });
    at(6.2, [this] {
        act(QStringLiteral("tab"), 0);
        act(QStringLiteral("theme"), QStringLiteral("light"));
        act(QStringLiteral("idFilter"), QStringLiteral("12G"));
    });
    at(7.5, [this] { screenshot(QStringLiteral("min-size-filter-error-light")); });
    at(7.7, [this] {
        act(QStringLiteral("clearFilter"));
        act(QStringLiteral("idFilter"), QStringLiteral("7FF"));
    });
    at(9.0, [this] { screenshot(QStringLiteral("min-size-no-match-light")); });
    at(9.2, [this] {
        act(QStringLiteral("clearFilter"));
        resizeWindow(1680, 1000);
    });
    at(10.5, [this] { act(QStringLiteral("scrollUp"), 30); });
    at(11.0, [this] { screenshot(QStringLiteral("wide-trace-detached-light")); });
    // Same view 0.9 s later: with a small --capacity the ring buffer evicts
    // rows meanwhile; the rows on screen must not move.
    at(11.9, [this] { screenshot(QStringLiteral("wide-trace-detached-later-light")); });
    at(12.0, [this] { m_session->setPaused(true); });
    at(13.0, [this] { screenshot(QStringLiteral("wide-trace-paused-light")); });
}

void DemoDirector::start()
{
    m_clock.start();
    m_stepTimer.start();
    if (m_mode == Mode::Demo)
        m_frameTimer.start();
    tick();
}

void DemoDirector::tick()
{
    if (m_finished)
        return;
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
    if (now >= m_durationMs)
        finish();
}

void DemoDirector::captureFrame()
{
    if (!m_window)
        return;
    const QImage image = m_window->grabWindow();
    if (image.isNull())
        return;
    const QString path = m_framesDir.filePath(
        QStringLiteral("frame_%1.png").arg(m_frameIndex++, 4, 10, QLatin1Char('0')));
    QThreadPool::globalInstance()->start([image, path] { image.save(path, "PNG"); });
}

void DemoDirector::screenshot(const QString &name)
{
    if (!m_window)
        return;
    const QImage image = m_window->grabWindow();
    const QString path = m_shotsDir.filePath(
        QStringLiteral("%1-%2.png").arg(++m_shotIndex, 2, 10, QLatin1Char('0')).arg(name));
    image.save(path, "PNG");
}

void DemoDirector::finish()
{
    m_finished = true;
    m_stepTimer.stop();
    m_frameTimer.stop();
    QThreadPool::globalInstance()->waitForDone();
    qInfo("capture finished: %d frames, %d screenshots", m_frameIndex, m_shotIndex);
    emit finished();
}
