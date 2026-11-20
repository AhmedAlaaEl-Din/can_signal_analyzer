#pragma once

#include <QDir>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <vector>

class QQuickWindow;

namespace can {
class CaptureSession;
}

// Drives a scripted scene and captures PNG images of the application window
// only. QQuickWindow::grabWindow renders the window's own scene graph, so
// nothing else on the desktop can ever end up in an image.
//
// Modes:
//  - Demo: ~45 s scene, frames at 5 fps into <out>/frames, five named
//    screenshots into <out>/screenshots.
//  - LayoutCheck: short scene that captures edge cases (empty state, minimum
//    window size, wide window, filter error) into <out>/checks.
class DemoDirector : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Demo, LayoutCheck };

    static constexpr int kFrameIntervalMs = 200; // 5 frames per second
    static constexpr int kDemoDurationMs = 45000;
    static constexpr int kCheckDurationMs = 14000;

    DemoDirector(Mode mode, QQuickWindow *window, can::CaptureSession *session,
                 const QString &outputDir, QObject *parent = nullptr);

    void start();

signals:
    void finished();

private:
    struct Step
    {
        int atMs;
        std::function<void()> run;
    };

    void at(double seconds, std::function<void()> fn);
    void buildDemoScript();
    void buildCheckScript();
    void tick();
    void captureFrame();
    void screenshot(const QString &name);
    void act(const QString &action, const QVariant &arg = {});
    void resizeWindow(int width, int height);
    void finish();
    static QDir cleanDir(const QDir &root, const QString &name);

    Mode m_mode;
    QPointer<QQuickWindow> m_window;
    can::CaptureSession *m_session;
    QDir m_framesDir;
    QDir m_shotsDir;
    std::vector<Step> m_steps;
    std::size_t m_nextStep = 0;
    QElapsedTimer m_clock;
    QTimer m_stepTimer;
    QTimer m_frameTimer;
    int m_durationMs = kDemoDurationMs;
    int m_frameIndex = 0;
    int m_swaps = 0;           // rendered frames since the last log line
    qint64 m_lastLogMs = 0;
    int m_shotIndex = 0;
    bool m_finished = false;
};
