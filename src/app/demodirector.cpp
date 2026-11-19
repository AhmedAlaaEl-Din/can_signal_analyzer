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
    // Scene (about 45 s): Plots tab, load the demo database, three lanes
    // (speed + rpm, throttle + torque, steering) plus a step plot for the gear,
    // follow live, zoom, pan, cursors A/B with the delta readout, a legend
    // toggle, a dragged threshold, a 30 s window with the min/max band, pause,
    // and the light theme.
    auto a = [this](double t, const QString &action, const QVariant &arg = {}) {
        at(t, [this, action, arg] { act(action, arg); });
    };
    at(0.0, [this] {
        act(QStringLiteral("theme"), QStringLiteral("dark"));
        act(QStringLiteral("tab"), 3);
        act(QStringLiteral("names"), QStringLiteral("on"));
    });
    at(0.5, [this] { m_session->start(); });
    a(1.5, QStringLiteral("loadDemoDb"));
    a(3.0, QStringLiteral("plotAdd"), QStringLiteral("VehicleDynamics.VehicleSpeed@-1"));
    a(3.6, QStringLiteral("plotAdd"), QStringLiteral("EngineData.EngineSpeed@0"));
    a(4.4, QStringLiteral("plotAdd"), QStringLiteral("EngineData.ThrottlePosition@-1"));
    a(5.0, QStringLiteral("plotAdd"), QStringLiteral("EngineData.EngineTorque@1"));
    a(5.8, QStringLiteral("plotAdd"), QStringLiteral("VehicleDynamics.SteeringAngle@-1"));
    a(6.6, QStringLiteral("plotAdd"), QStringLiteral("TransmissionStatus.CurrentGear@-1"));
    at(9.5, [this] { screenshot(QStringLiteral("plots-live-dark")); });

    a(10.5, QStringLiteral("plotWindow"), 5);
    a(12.5, QStringLiteral("plotWindow"), 10);
    // Zoom in around 70 % of the width, then pan back in time.
    a(13.6, QStringLiteral("plotZoom"), QStringLiteral("0.75@0.7"));
    a(14.0, QStringLiteral("plotZoom"), QStringLiteral("0.75@0.7"));
    a(14.4, QStringLiteral("plotZoom"), QStringLiteral("0.75@0.7"));
    for (int i = 0; i < 10; ++i)
        a(15.2 + 0.1 * i, QStringLiteral("plotPan"), -0.04);
    a(17.0, QStringLiteral("plotCursors"), QStringLiteral("place"));
    for (int i = 0; i <= 8; ++i) // drag cursor B to the right
        a(17.6 + 0.1 * i, QStringLiteral("plotCursor"), QStringLiteral("1@%1").arg(0.62 + 0.02 * i));
    a(18.8, QStringLiteral("plotHover"), 0.47);
    at(20.0, [this] { screenshot(QStringLiteral("plots-cursors-dark")); });
    a(20.6, QStringLiteral("plotHover"), -1);

    a(21.2, QStringLiteral("plotToggle"), QStringLiteral("0@EngineData.EngineSpeed"));
    a(22.6, QStringLiteral("plotToggle"), QStringLiteral("0@EngineData.EngineSpeed"));
    a(23.4, QStringLiteral("plotThreshold"), QStringLiteral("1@46"));
    for (int i = 1; i <= 16; ++i) // drag the threshold up (throttle %, left axis of lane 2)
        a(23.8 + 0.1 * i, QStringLiteral("plotThresholdMove"), QStringLiteral("1@0@%1").arg(46.0 + 0.4 * i));
    at(26.5, [this] { screenshot(QStringLiteral("plots-threshold-dark")); });

    a(27.2, QStringLiteral("plotCursors"), QStringLiteral("clear"));
    a(27.6, QStringLiteral("plotFollow"));
    a(28.6, QStringLiteral("plotWindow"), 30);
    at(31.0, [this] { screenshot(QStringLiteral("plots-30s-band-dark")); });

    a(32.0, QStringLiteral("theme"), QStringLiteral("light"));
    a(33.0, QStringLiteral("plotWindow"), 10);
    at(35.0, [this] { screenshot(QStringLiteral("plots-light")); });
    a(35.8, QStringLiteral("plotHover"), 0.62);
    at(37.2, [this] { screenshot(QStringLiteral("plots-tooltip-light")); });
    a(38.0, QStringLiteral("plotHover"), -1);
    at(39.0, [this] { m_session->setPaused(true); }); // the view freezes, capture continues
    at(41.5, [this] { m_session->setPaused(false); });
    a(42.5, QStringLiteral("tab"), 2);
    a(44.0, QStringLiteral("tab"), 3);
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
    at(17.2, [this] {
        m_session->setVehicleMode(true);
        act(QStringLiteral("theme"), QStringLiteral("dark"));
        act(QStringLiteral("tab"), 3);
    });
    at(18.0, [this] { screenshot(QStringLiteral("plots-empty-dark")); });
    at(18.2, [this] {
        resizeWindow(960, 600);
        act(QStringLiteral("plotAdd"), QStringLiteral("VehicleDynamics.VehicleSpeed@-1"));
        act(QStringLiteral("plotAdd"), QStringLiteral("EngineData.EngineSpeed@0"));
        act(QStringLiteral("plotAdd"), QStringLiteral("BrakeStatus.BrakePedalPressed@-1"));
        act(QStringLiteral("plotCursors"), QStringLiteral("place"));
    });
    at(23.5, [this] { screenshot(QStringLiteral("min-size-plots-dark")); });
    at(23.6, [this] {
        act(QStringLiteral("plotExportPng"), QUrl::fromLocalFile(m_shotsDir.filePath(QStringLiteral("export-plot.png"))));
        act(QStringLiteral("plotExportCsv"), QUrl::fromLocalFile(m_root.filePath(QStringLiteral("export-visible-range.csv"))));
    });
}

void DemoDirector::start()
{
    if (m_window) // keep the real mouse out of the captured frames
        m_window->setProperty("demoMode", m_mode == Mode::Demo);
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
