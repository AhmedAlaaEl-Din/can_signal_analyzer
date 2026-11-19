// Plot rendering benchmark for the custom scene-graph engine (PlotCanvas).
// Usage: can_analyzer --plot-bench <result-file>
//
// 8 signals in 4 lanes x 2, each sampled at 1000 Hz (8000 samples/s, the
// decoded output of roughly 2000 frames/s), 10 s window => 10,000 visible
// points per signal. Measured twice: with min/max decimation (the app's
// default) and drawing all raw points.

#include "plotbench.h"

#include "plotcanvas.h"
#include "timeseriesstore.h"

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr int kSignals = 8;
constexpr int kRateHz = 1000;
constexpr qint64 kWindowUs = 10'000'000;

double sampleValue(int sig, qint64 tUs)
{
    const double t = tUs / 1e6;
    return 50.0 + 40.0 * std::sin(t * (0.7 + sig * 0.31)) + 6.0 * std::sin(t * (13.0 + sig))
           + ((tUs / 1000 + sig * 7) % 97 == 0 ? 25.0 : 0.0); // occasional spikes
}

const char *kQml = R"QML(
import QtQuick
import CanAnalyzer
Window {
    width: 1280; height: 800; visible: true; color: "#0E1014"
    title: "CAN Analyzer"
    property var store
    property real t0: 0
    property real t1: 1
    property bool decimate: true
    property var pal: ["#56B4E9", "#E69F00", "#3FC1A0", "#F0E442", "#E08AB8", "#F07A3A", "#7AA6FF", "#C7CED9"]
    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12
        Repeater {
            model: 4
            PlotCanvas {
                required property int index
                width: parent.width
                height: (parent.height - 36) / 4
                store: window.store
                palette: window.pal
                series: [ { signalId: index * 2, color: index * 2, visible: true },
                          { signalId: index * 2 + 1, color: index * 2 + 1, visible: true, dashed: index % 2 === 1 } ]
                viewStartUs: window.t0
                viewEndUs: window.t1
                yMin: 0
                yMax: 120
                decimate: window.decimate
                revision: window.store ? window.store.revision : 0
            }
        }
    }
    id: window
}
)QML";

struct Phase
{
    QString name;
    std::vector<double> frameMs;
    qint64 prepareNs = 0;
    qint64 geometryNs = 0;
    qint64 points = 0;
    double seconds = 0;
};

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size() - 1)))];
}

} // namespace

int runPlotBench(const QString &resultPath)
{
    auto *store = new can::TimeSeriesStore;
    store->reset(kSignals);
    qint64 nowUs = 0;
    auto produce = [&](qint64 untilUs) {
        const qint64 stepUs = 1'000'000 / kRateHz;
        for (; nowUs + stepUs <= untilUs; nowUs += stepUs)
            for (int s = 0; s < kSignals; ++s)
                store->append(s, nowUs + stepUs, sampleValue(s, nowUs + stepUs));
        store->commit();
    };
    produce(kWindowUs); // a full window before the first frame

    auto *engine = new QQmlEngine;
    QQmlComponent component(engine);
    component.setData(kQml, QUrl(QStringLiteral("qrc:/bench.qml")));
    QObject *root = component.create();
    auto *window = qobject_cast<QQuickWindow *>(root);
    if (!window) {
        QFile f(resultPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << "bench window failed: " << component.errorString() << '\n';
        return 1;
    }
    window->setProperty("store", QVariant::fromValue<QObject *>(store));

    std::vector<Phase> phases = {{QStringLiteral("custom scene graph, min/max decimation"), {}, 0, 0, 0, 0},
                                 {QStringLiteral("custom scene graph, all raw points"), {}, 0, 0, 0, 0}};
    std::size_t current = 0;
    constexpr double kPhaseSeconds = 8.0;
    constexpr double kWarmupSeconds = 1.0;
    QElapsedTimer wall, frame, phaseClock;
    wall.start();
    frame.start();
    phaseClock.start();
    qint64 simBaseUs = nowUs;
    bool measuring = false;

    auto resetCounters = [] {
        PlotCanvas::prepareNanos() = 0;
        PlotCanvas::geometryNanos() = 0;
        PlotCanvas::pointsDrawn() = 0;
    };

    QObject::connect(window, &QQuickWindow::frameSwapped, window, [&] {
        const double dt = frame.nsecsElapsed() / 1e6;
        frame.restart();
        const double inPhase = phaseClock.nsecsElapsed() / 1e9;
        if (!measuring && inPhase >= kWarmupSeconds) {
            measuring = true;
            resetCounters();
            phases[current].frameMs.clear();
            phaseClock.restart();
        } else if (measuring) {
            phases[current].frameMs.push_back(dt);
            if (inPhase >= kPhaseSeconds) {
                Phase &p = phases[current];
                p.seconds = inPhase;
                p.prepareNs = PlotCanvas::prepareNanos();
                p.geometryNs = PlotCanvas::geometryNanos();
                p.points = PlotCanvas::pointsDrawn();
                measuring = false;
                ++current;
                phaseClock.restart();
                if (current >= phases.size()) {
                    QTimer::singleShot(0, qApp, &QCoreApplication::quit);
                    return;
                }
                window->setProperty("decimate", false);
            }
        }
        // New data for the elapsed wall time, then follow the live end.
        produce(simBaseUs + wall.nsecsElapsed() / 1000);
        window->setProperty("t0", double(nowUs - kWindowUs));
        window->setProperty("t1", double(nowUs));
        window->requestUpdate();
    });
    window->requestUpdate();
    QGuiApplication::exec();

    QFile f(resultPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return 1;
    QTextStream out(&f);
    out << "Plot benchmark: 8 signals x 1000 Hz (8000 samples/s), 10 s window = 10,000 points per signal,\n"
        << "4 lanes, window 1280x800 logical, device pixel ratio " << window->effectiveDevicePixelRatio()
        << ", MSAA samples " << window->format().samples() << ", graphics API "
        << int(window->rendererInterface()->graphicsApi()) << "\n\n";
    for (const Phase &p : phases) {
        const double frames = static_cast<double>(p.frameMs.size());
        out << p.name << ":\n"
            << "  fps " << QString::number(frames / std::max(0.001, p.seconds), 'f', 1)
            << "  frame time p50 " << QString::number(percentile(p.frameMs, 0.5), 'f', 2) << " ms"
            << "  p95 " << QString::number(percentile(p.frameMs, 0.95), 'f', 2) << " ms"
            << "  max " << QString::number(percentile(p.frameMs, 1.0), 'f', 2) << " ms\n"
            << "  data preparation " << QString::number(p.prepareNs / 1e6 / std::max(1.0, frames), 'f', 2) << " ms/frame"
            << "  geometry " << QString::number(p.geometryNs / 1e6 / std::max(1.0, frames), 'f', 2) << " ms/frame"
            << "  points drawn " << qint64(p.points / std::max(1.0, frames)) << " /frame\n";
    }
    delete root;
    delete engine;
    delete store;
    return 0;
}
