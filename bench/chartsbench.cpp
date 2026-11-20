// Comparison benchmark: the same workload as `can_analyzer --plot-bench`
// (8 signals x 1000 Hz, 10 s window = 10,000 points per signal, 4 lanes),
// rendered with Qt Charts (QML ChartView + LineSeries), with and without
// useOpenGL. Qt Graphs, the newer module, is not installed on the
// development machine, so Qt Charts is the installed alternative compared.
//
// Usage: can_charts_bench <result-file>

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <QtCharts/QXYSeries>

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace {
constexpr int kSignals = 8;
constexpr int kRateHz = 1000;
constexpr qint64 kWindowUs = 10'000'000;

double sampleValue(int sig, qint64 tUs)
{
    const double t = tUs / 1e6;
    return 50.0 + 40.0 * std::sin(t * (0.7 + sig * 0.31)) + 6.0 * std::sin(t * (13.0 + sig))
           + ((tUs / 1000 + sig * 7) % 97 == 0 ? 25.0 : 0.0);
}

const char *kQml = R"QML(
import QtQuick
import QtCharts
Window {
    id: window
    width: 1280; height: 800; visible: true; color: "#0E1014"
    title: "CAN Analyzer"
    property bool gl: false
    property var seriesList: [null, null, null, null, null, null, null, null]
    property var axisList: [null, null, null, null]
    Column {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12
        Repeater {
            model: 4
            ChartView {
                required property int index
                width: parent.width
                height: (parent.height - 36) / 4
                antialiasing: true
                legend.visible: false
                backgroundColor: "#15181E"
                ValueAxis { id: ax; min: 0; max: 10 }
                ValueAxis { id: ay; min: 0; max: 120 }
                LineSeries { id: s0; axisX: ax; axisY: ay; useOpenGL: window.gl; width: 1.75; color: "#56B4E9" }
                LineSeries { id: s1; axisX: ax; axisY: ay; useOpenGL: window.gl; width: 1.75; color: "#E69F00" }
                Component.onCompleted: {
                    window.seriesList[index * 2] = s0
                    window.seriesList[index * 2 + 1] = s1
                    window.axisList[index] = ax
                }
            }
        }
    }
}
)QML";

double percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return 0;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, static_cast<std::size_t>(p * static_cast<double>(v.size() - 1)))];
}
} // namespace

int main(int argc, char *argv[])
{
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    const QString resultPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("charts-bench.txt");

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(kQml, QUrl(QStringLiteral("qrc:/charts.qml")));
    QObject *root = component.create();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QFile f(resultPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return 1;
    QTextStream out(&f);
    if (!window) {
        out << "charts window failed: " << component.errorString() << '\n';
        return 1;
    }
    std::vector<QXYSeries *> series;
    for (const QVariant &v : root->property("seriesList").toList())
        series.push_back(qobject_cast<QXYSeries *>(v.value<QObject *>()));
    if (series.size() != kSignals || std::count(series.begin(), series.end(), nullptr) > 0) {
        out << "series not found" << Qt::endl;
        return 1;
    }
    QList<QObject *> axes;
    for (const QVariant &v : root->property("axisList").toList())
        axes << v.value<QObject *>();

    // Rolling 10 s of points per series (x in seconds).
    std::vector<std::deque<QPointF>> data(kSignals);
    qint64 nowUs = 0;
    auto produce = [&](qint64 untilUs) {
        const qint64 stepUs = 1'000'000 / kRateHz;
        for (; nowUs + stepUs <= untilUs; nowUs += stepUs)
            for (int s = 0; s < kSignals; ++s) {
                data[std::size_t(s)].emplace_back((nowUs + stepUs) / 1e6, sampleValue(s, nowUs + stepUs));
                while (data[std::size_t(s)].front().x() < (nowUs + stepUs - kWindowUs) / 1e6)
                    data[std::size_t(s)].pop_front();
            }
    };
    produce(kWindowUs);

    struct Phase
    {
        QString name;
        std::vector<double> frameMs;
        qint64 feedNs = 0;
        double seconds = 0;
    };
    std::vector<Phase> phases = {{QStringLiteral("Qt Charts, raster (useOpenGL: false)"), {}, 0, 0},
                                 {QStringLiteral("Qt Charts, useOpenGL: true"), {}, 0, 0}};
    std::size_t current = 0;
    bool measuring = false;
    QElapsedTimer wall, frame, phaseClock;
    wall.start();
    frame.start();
    phaseClock.start();
    const qint64 base = nowUs;
    QList<QPointF> buffer;

    QObject::connect(window, &QQuickWindow::frameSwapped, window, [&] {
        const double dt = frame.nsecsElapsed() / 1e6;
        frame.restart();
        const double inPhase = phaseClock.nsecsElapsed() / 1e9;
        if (!measuring && inPhase >= 1.0) {
            measuring = true;
            phases[current].frameMs.clear();
            phases[current].feedNs = 0;
            phaseClock.restart();
        } else if (measuring) {
            phases[current].frameMs.push_back(dt);
            if (inPhase >= 8.0) {
                phases[current].seconds = inPhase;
                measuring = false;
                ++current;
                phaseClock.restart();
                if (current >= phases.size()) {
                    QTimer::singleShot(0, &app, &QCoreApplication::quit);
                    return;
                }
                root->setProperty("gl", true);
            }
        }
        QElapsedTimer feed;
        feed.start();
        produce(base + wall.nsecsElapsed() / 1000);
        for (int s = 0; s < kSignals; ++s) {
            buffer.assign(data[std::size_t(s)].begin(), data[std::size_t(s)].end());
            series[std::size_t(s)]->replace(buffer);
        }
        for (QObject *ax : axes) {
            if (ax) {
                ax->setProperty("min", (nowUs - kWindowUs) / 1e6);
                ax->setProperty("max", nowUs / 1e6);
            }
        }
        if (measuring)
            phases[current].feedNs += feed.nsecsElapsed();
        window->requestUpdate();
    });
    window->requestUpdate();
    app.exec();

    out << "Qt Charts benchmark: same workload as the scene-graph benchmark (8 x 10,000 points, 4 charts)\n\n";
    for (const Phase &p : phases) {
        const double frames = static_cast<double>(p.frameMs.size());
        out << p.name << ":\n"
            << "  fps " << QString::number(frames / std::max(0.001, p.seconds), 'f', 1)
            << "  frame time p50 " << QString::number(percentile(p.frameMs, 0.5), 'f', 2) << " ms"
            << "  p95 " << QString::number(percentile(p.frameMs, 0.95), 'f', 2) << " ms"
            << "  max " << QString::number(percentile(p.frameMs, 1.0), 'f', 2) << " ms\n"
            << "  feeding the series " << QString::number(p.feedNs / 1e6 / std::max(1.0, frames), 'f', 2)
            << " ms/frame\n";
    }
    delete root;
    return 0;
}
