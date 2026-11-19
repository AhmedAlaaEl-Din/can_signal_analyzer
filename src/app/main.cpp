#include "appservices.h"
#include "capturesession.h"
#include "replaysource.h"
#include "version.h"
#include "framemodel.h"
#include "demodirector.h"
#include "plotbench.h"
#include "signaldatabase.h"
#include "simulatedsource.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

#include <algorithm>
#include <cstdio>
#include <memory>

Q_IMPORT_QML_PLUGIN(CanUiPlugin)
Q_IMPORT_QML_PLUGIN(CanStylePlugin)

namespace {

std::unique_ptr<QFile> g_logFile;

// In demo mode all diagnostics go to evidence/demo-log.txt (the GUI build has
// no console); they are never shown to the user.
void fileMessageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (!g_logFile)
        return;
    static const char *names[] = {"debug", "warning", "critical", "fatal", "info"};
    const QByteArray line = QStringLiteral("%1 [%2] %3\n")
                                .arg(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                                     QLatin1String(names[type < 5 ? type : 0]), msg)
                                .toUtf8();
    g_logFile->write(line);
    g_logFile->flush();
}

} // namespace

int main(int argc, char *argv[])
{
    // Deterministic demo capture drives all animations from a custom driver;
    // that needs the single-threaded render loop (set for this process only).
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--demo") == 0)
            qputenv("QSG_RENDER_LOOP", "basic");
    }
    // 4x MSAA for the whole window: plot lines (triangle geometry) get smooth,
    // anti-aliased edges without per-vertex feathering.
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("CAN Analyzer"));
    QGuiApplication::setApplicationVersion(QStringLiteral(CAN_ANALYZER_VERSION));
    QGuiApplication::setOrganizationName(QStringLiteral("CanAnalyzer"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("CAN signal analyzer (simulated bus)"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption demoOption(QStringLiteral("demo"),
                                        QStringLiteral("Run the scripted demo scene and capture PNG frames."));
    const QCommandLineOption outOption(QStringLiteral("evidence-dir"),
                                       QStringLiteral("Output folder for demo frames and screenshots."),
                                       QStringLiteral("dir"), QStringLiteral("evidence"));
    const QCommandLineOption seedOption(QStringLiteral("seed"),
                                        QStringLiteral("Simulator seed (decimal or 0x-prefixed hex)."),
                                        QStringLiteral("seed"));
    const QCommandLineOption rateOption(QStringLiteral("rate"),
                                        QStringLiteral("Initial simulated rate in frames/s (0 = nominal)."),
                                        QStringLiteral("fps"), QStringLiteral("1000"));
    const QCommandLineOption checkOption(QStringLiteral("layout-check"),
                                         QStringLiteral("Capture edge-case screenshots (empty, minimum size, wide, errors)."));
    const QCommandLineOption capacityOption(QStringLiteral("capacity"),
                                            QStringLiteral("Trace ring-buffer capacity in frames."),
                                            QStringLiteral("frames"), QString::number(can::FrameModel::kDefaultCapacity));
    const QCommandLineOption benchOption(QStringLiteral("plot-bench"),
                                         QStringLiteral("Run the plot rendering benchmark and write the results to <file>."),
                                         QStringLiteral("file"));
    const QCommandLineOption smokeOption(QStringLiteral("smoke-test"),
                                         QStringLiteral("Start, load the demo database, replay <log> for a moment, "
                                                        "write the result next to it (.smoke.txt) and exit."),
                                         QStringLiteral("log"));
    parser.addOptions({demoOption, checkOption, outOption, seedOption, rateOption, capacityOption, benchOption,
                       smokeOption});
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A .canlog or .csv log to open in replay."),
                                 QStringLiteral("[file]"));
    parser.process(app);
    if (parser.isSet(benchOption))
        return runPlotBench(parser.value(benchOption));

    const bool demo = parser.isSet(demoOption) || parser.isSet(checkOption);
    const QString evidenceDir = QDir(parser.value(outOption)).absolutePath();
    if (demo) {
        QDir().mkpath(evidenceDir);
        g_logFile = std::make_unique<QFile>(QDir(evidenceDir).filePath(QStringLiteral("demo-log.txt")));
        if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            qInstallMessageHandler(fileMessageHandler);
        else
            g_logFile.reset();
    }

    std::uint64_t seed = can::SimulatorEngine::kDefaultSeed;
    if (parser.isSet(seedOption)) {
        bool ok = false;
        const quint64 v = parser.value(seedOption).toULongLong(&ok, 0);
        if (ok)
            seed = v;
    }

    QQuickStyle::setStyle(QStringLiteral("CanStyle"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/qt/qml/CanAnalyzer/icons/app.svg")));

    auto *source = new can::SimulatedSource(seed, can::SimulatorEngine::Mode::Vehicle);
    can::CaptureSession session(source);
    session.setTargetRate(parser.value(rateOption).toInt());
    session.setBurstsEnabled(true);
    session.frameModel()->setCapacity(std::max(1000, parser.value(capacityOption).toInt()));
    qmlRegisterSingletonInstance("CanAnalyzer.Core", 1, 0, "Session", &session);
    AppInfo appInfo;
    qmlRegisterSingletonInstance("CanAnalyzer.Core", 1, 0, "AppInfo", &appInfo);
    // Scripted runs never read or write the user's settings.
    const bool smoke = parser.isSet(smokeOption);
    AppSettings settings(!demo && !smoke);
    qmlRegisterSingletonInstance("CanAnalyzer.Core", 1, 0, "Settings", &settings);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("CanAnalyzer", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    std::unique_ptr<DemoDirector> director;
    if (demo && window) {
        window->resize(1280, 800);
        const auto mode = parser.isSet(checkOption) ? DemoDirector::Mode::LayoutCheck
                                                    : DemoDirector::Mode::Demo;
        director = std::make_unique<DemoDirector>(mode, window, &session, evidenceDir);
        QObject::connect(director.get(), &DemoDirector::finished, &app, &QCoreApplication::quit,
                         Qt::QueuedConnection);
        // Give the window a moment to be exposed and laid out before the scene starts.
        QTimer::singleShot(800, director.get(), [d = director.get()] { d->start(); });
    }

    if (smoke) {
        // Release-package check: the app starts, decodes and replays a log
        // with only the files of the package (no Qt installation on PATH).
        const QString log = parser.value(smokeOption);
        QTimer::singleShot(500, &app, [&session, log] {
            session.loadDemoSetup();
            session.stop();
            if (!session.openLogPath(log)) {
                QCoreApplication::exit(3);
                return;
            }
            session.replay()->setSpeed(8);
        });
        QTimer::singleShot(3500, &app, [&session, log] {
            QFile out(log + QStringLiteral(".smoke.txt"));
            const bool ok = session.replayMode() && session.totalFrames() > 1000
                            && session.signalModel() && session.database()->isLoaded();
            if (out.open(QIODevice::WriteOnly | QIODevice::Text))
                out.write(QStringLiteral("ok=%1 frames=%2 qt=%3 version=%4\n")
                              .arg(ok).arg(session.totalFrames()).arg(QString::fromLatin1(qVersion()),
                                                                      QCoreApplication::applicationVersion())
                              .toUtf8());
            QCoreApplication::exit(ok ? 0 : 4);
        });
    } else if (!demo && !parser.positionalArguments().isEmpty()) {
        const QString file = QDir::current().absoluteFilePath(parser.positionalArguments().constFirst());
        QTimer::singleShot(300, &session, [&session, file] { session.openLogPath(file); });
    }

    const int rc = app.exec();
    session.stop();
    return rc;
}
