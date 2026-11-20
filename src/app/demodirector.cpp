#include "demodirector.h"

#include "capturesession.h"
#include "simulatedsource.h"

#include <QAbstractAnimation>
#include <QAnimationDriver>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMutex>
#include <QQuickWindow>
#include <QThreadPool>
#include <QUrl>
#include <QVariant>

#include <algorithm>
#include <atomic>

// Animation driver whose clock is set explicitly: QML animations advance only
// when the director says so, by exactly one frame interval.
class StepAnimationDriver : public QAnimationDriver
{
public:
    void advanceTo(qint64 ms)
    {
        m_elapsed = ms;
        advance();
    }
    qint64 elapsed() const override { return m_elapsed; }

private:
    qint64 m_elapsed = 0;
};

namespace {
std::atomic<qint64> g_bytes{0};
}

DemoDirector::DemoDirector(Mode mode, QQuickWindow *window, can::CaptureSession *session,
                           const QString &outputDir, QObject *parent)
    : QObject(parent)
    , m_mode(mode)
    , m_window(window)
    , m_session(session)
    , m_root(outputDir)
{
    if (m_mode == Mode::Demo) {
        m_framesDir = cleanDir(m_root, QStringLiteral("frames"), {QStringLiteral("*.png"), QStringLiteral("*.jpg")});
        m_shotsDir = cleanDir(m_root, QStringLiteral("screenshots"), {QStringLiteral("*.png")});
        m_durationMs = kDemoDurationMs;
        buildDemoScript();
    } else {
        m_shotsDir = cleanDir(m_root, QStringLiteral("checks"), {QStringLiteral("*.png")});
        m_durationMs = kCheckDurationMs;
        buildCheckScript();
        m_checkTimer.setInterval(10);
        connect(&m_checkTimer, &QTimer::timeout, this, &DemoDirector::checkTick);
    }
}

DemoDirector::~DemoDirector()
{
    if (m_driver)
        m_driver->uninstall();
}

QDir DemoDirector::cleanDir(const QDir &root, const QString &name, const QStringList &patterns)
{
    root.mkpath(name);
    QDir dir(root.filePath(name));
    // Start from a clean slate so images from an older run never mix in.
    for (const QString &f : dir.entryList(patterns, QDir::Files))
        dir.remove(f);
    return dir;
}

void DemoDirector::at(double seconds, std::function<void()> fn)
{
    m_steps.push_back({static_cast<int>(seconds * 1000 + 0.5), std::move(fn)});
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
    // Scene (about 45 s): live trace without a database, load the demo
    // database, decoded trace and messages, the Signals tab with search and
    // pins, the signal detail with bit layout, a theme switch and back.
    at(0.0, [this] {
        act(QStringLiteral("theme"), QStringLiteral("dark"));
        act(QStringLiteral("tab"), 0);
        act(QStringLiteral("names"), QStringLiteral("on"));
    });
    at(0.6, [this] { m_session->start(); });
    at(3.5, [this] { act(QStringLiteral("loadDemoDb")); });
    at(5.0, [this] { m_session->setPaused(true); });  // freeze the view to inspect a frame
    at(5.6, [this] { act(QStringLiteral("selectRow"), 9); });
    at(7.0, [this] { screenshot(QStringLiteral("trace-decoded-dark")); });
    at(7.6, [this] { m_session->setPaused(false); });

    at(8.0, [this] { act(QStringLiteral("tab"), 1); });
    at(9.0, [this] { act(QStringLiteral("expandMessage"), QStringLiteral("100")); });
    at(9.8, [this] { act(QStringLiteral("expandMessage"), QStringLiteral("0C8")); });
    at(11.5, [this] { screenshot(QStringLiteral("messages-decoded-dark")); });

    at(12.5, [this] { act(QStringLiteral("tab"), 2); });
    at(15.5, [this] { screenshot(QStringLiteral("signals-dark")); });

    const QString query = QStringLiteral("temp");
    for (int i = 1; i <= query.size(); ++i)
        at(16.5 + 0.25 * i, [this, q = query.left(i)] { act(QStringLiteral("search"), q); });
    at(18.6, [this] { act(QStringLiteral("pin"), QStringLiteral("EngineTemperatures.CoolantTemp")); });
    at(19.6, [this] { act(QStringLiteral("search"), QString()); });
    at(20.4, [this] { act(QStringLiteral("pin"), QStringLiteral("VehicleDynamics.VehicleSpeed")); });
    at(21.0, [this] { act(QStringLiteral("pin"), QStringLiteral("EngineData.EngineSpeed")); });
    at(21.6, [this] { act(QStringLiteral("pin"), QStringLiteral("TransmissionStatus.CurrentGear")); });
    at(23.5, [this] { screenshot(QStringLiteral("signals-pinned-dark")); });

    at(24.5, [this] { act(QStringLiteral("selectSignal"), QStringLiteral("EngineData.EngineTorque")); });
    at(27.5, [this] { screenshot(QStringLiteral("signal-detail-dark")); });
    at(29.0, [this] { act(QStringLiteral("theme"), QStringLiteral("light")); });
    at(30.5, [this] { act(QStringLiteral("selectSignal"), QStringLiteral("VehicleDynamics.SteeringAngle")); });
    at(33.5, [this] { screenshot(QStringLiteral("signal-detail-light")); });

    at(35.0, [this] { act(QStringLiteral("closeSignal")); });
    at(36.0, [this] { act(QStringLiteral("tab"), 0); });
    at(38.5, [this] { act(QStringLiteral("names"), QStringLiteral("off")); });
    at(40.5, [this] { act(QStringLiteral("names"), QStringLiteral("on")); });
    at(41.5, [this] { act(QStringLiteral("theme"), QStringLiteral("dark")); });
    at(43.0, [this] { act(QStringLiteral("tab"), 2); });
}

void DemoDirector::buildCheckScript()
{
    at(0.0, [this] {
        act(QStringLiteral("theme"), QStringLiteral("dark"));
        act(QStringLiteral("tab"), 2);
    });
    at(1.0, [this] { screenshot(QStringLiteral("signals-no-database-dark")); });
    at(1.2, [this] {
        // An invalid DBC file, written into the evidence folder, to show the error banner.
        const QString bad = m_root.filePath(QStringLiteral("broken_example.dbc"));
        QFile f(bad);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write("VERSION \"\"\n\nBO_ 200 Example: 8 Node\n"
                    " SG_ Speed : 0|16@1+ (0.01,0) [0|300] km/h Node\n"
                    " SG_ Mode : 70|4@1+ (1,0) [0|0] \"\" Node\n"
                    "BO_ 200 Again: 8 Node\n");
            f.close();
        }
        act(QStringLiteral("loadDbFile"), QUrl::fromLocalFile(bad));
    });
    at(2.5, [this] { screenshot(QStringLiteral("bad-database-banner-dark")); });
    at(2.7, [this] {
        act(QStringLiteral("loadDemoDb"));
        m_session->start();
    });
    at(4.0, [this] { resizeWindow(960, 600); }); // the window's minimum size
    at(6.0, [this] { act(QStringLiteral("selectSignal"), QStringLiteral("BatteryStatus.BatteryCurrent")); });
    at(7.5, [this] { screenshot(QStringLiteral("min-size-signals-detail-dark")); });
    at(7.7, [this] { act(QStringLiteral("closeSignal")); act(QStringLiteral("tab"), 0); });
    at(8.5, [this] { act(QStringLiteral("selectRow"), 6); });
    at(9.5, [this] { screenshot(QStringLiteral("min-size-trace-decoded-dark")); });
    at(9.7, [this] {
        act(QStringLiteral("theme"), QStringLiteral("light"));
        act(QStringLiteral("tab"), 1);
        act(QStringLiteral("expandMessage"), QStringLiteral("3F0"));
    });
    at(11.5, [this] { screenshot(QStringLiteral("min-size-messages-mux-light")); });
    at(11.7, [this] {
        resizeWindow(1600, 940);
        act(QStringLiteral("tab"), 2);
        act(QStringLiteral("search"), QStringLiteral("zzz"));
    });
    at(13.0, [this] { screenshot(QStringLiteral("wide-signals-no-match-light")); });
    at(13.2, [this] {
        act(QStringLiteral("search"), QString());
        act(QStringLiteral("selectSignal"), QStringLiteral("TripComputer.Odometer"));
    });
    at(15.0, [this] { screenshot(QStringLiteral("wide-signal-detail-mux-light")); });
    at(15.2, [this] { m_session->setVehicleMode(false); act(QStringLiteral("closeSignal")); act(QStringLiteral("tab"), 0); });
    at(17.0, [this] { screenshot(QStringLiteral("wide-trace-raw-mode-light")); });
}

void DemoDirector::start()
{
    if (m_mode == Mode::LayoutCheck) {
        m_wall.start();
        m_checkTimer.start();
        checkTick();
        return;
    }
    // Deterministic mode: manual simulation clock + stepped animation driver.
    m_session->setManualTime(true);
    m_driver = std::make_unique<StepAnimationDriver>();
    m_driver->install();
    QTimer::singleShot(0, this, &DemoDirector::demoFrame);
}

void DemoDirector::runStepsUntil(qint64 ms)
{
    while (m_nextStep < m_steps.size() && m_steps[m_nextStep].atMs <= ms)
        m_steps[m_nextStep++].run();
}

void DemoDirector::demoFrame()
{
    if (m_finished || !m_window)
        return;
    const qint64 ms = qint64(m_frameIndex) * 1000 / kFps;
    if (ms > m_durationMs) {
        finish();
        return;
    }
    runStepsUntil(ms);
    // Advance the simulator and the models to exactly this instant.
    m_session->source()->stepTo(ms * 1000);
    m_session->manualTick(ms);
    m_driver->advanceTo(ms);
    // Let queued work (Qt.callLater, layout) settle before rendering.
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    const QImage image = m_window->grabWindow();
    if (!image.isNull()) {
        const QString path = m_framesDir.filePath(
            QStringLiteral("frame_%1.jpg").arg(m_frameIndex, 4, 10, QLatin1Char('0')));
        // Frames are stored at the window's logical size (1280 x 800) rather than
        // in device pixels, which keeps the zipped sequence well below 60 MB.
        const QSize logical = m_window->size();
        QThreadPool::globalInstance()->start([image, path, logical] {
            QImage out = image.size() == logical
                             ? image
                             : image.scaled(logical, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (out.convertToFormat(QImage::Format_RGB32).save(path, "JPG", kJpegQuality))
                g_bytes += QFileInfo(path).size();
        });
    }
    if (m_frameIndex % 24 == 0) {
        QThreadPool::globalInstance()->waitForDone(); // keep memory bounded
        qInfo("frame %d  t=%5.2fs  rows %llu  jpeg so far %.1f MB", m_frameIndex, ms / 1000.0,
              m_session->totalFrames(), g_bytes.load() / 1048576.0);
    }
    ++m_frameIndex;
    QTimer::singleShot(0, this, &DemoDirector::demoFrame);
}

void DemoDirector::checkTick()
{
    if (m_finished)
        return;
    const qint64 now = m_wall.elapsed();
    runStepsUntil(now);
    if (now >= m_durationMs)
        finish();
}

void DemoDirector::screenshot(const QString &name)
{
    if (!m_window)
        return;
    if (m_mode == Mode::Demo)
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const QImage image = m_window->grabWindow();
    const QString path = m_shotsDir.filePath(
        QStringLiteral("%1-%2.png").arg(++m_shotIndex, 2, 10, QLatin1Char('0')).arg(name));
    image.save(path, "PNG");
}

void DemoDirector::finish()
{
    m_finished = true;
    m_checkTimer.stop();
    QThreadPool::globalInstance()->waitForDone();
    if (m_driver) {
        m_driver->uninstall();
        m_driver.reset();
    }
    qInfo("capture finished: %d frames (%.1f MB JPEG), %d screenshots", m_frameIndex,
          g_bytes.load() / 1048576.0, m_shotIndex);
    emit finished();
}
