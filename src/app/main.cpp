#include "capturesession.h"
#include "demodirector.h"
#include "simulatedsource.h"

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtQml/qqmlextensionplugin.h>

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
    parser.addOptions({demoOption, outOption, seedOption, rateOption});
    parser.process(app);

    const bool demo = parser.isSet(demoOption);
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

    auto *source = new can::SimulatedSource(seed);
    can::CaptureSession session(source);
    session.setTargetRate(parser.value(rateOption).toInt());
    session.setBurstsEnabled(true);
    qmlRegisterSingletonInstance("CanAnalyzer.Core", 1, 0, "Session", &session);

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
        director = std::make_unique<DemoDirector>(window, &session, evidenceDir);
        QObject::connect(director.get(), &DemoDirector::finished, &app, &QCoreApplication::quit,
                         Qt::QueuedConnection);
        // Give the window a moment to be exposed and laid out before the scene starts.
        QTimer::singleShot(800, director.get(), [d = director.get()] { d->start(); });
    }

    const int rc = app.exec();
    session.stop();
    return rc;
}
