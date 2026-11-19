#pragma once

#include <QDir>
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <memory>
#include <vector>

class QQuickWindow;
class StepAnimationDriver;

namespace can {
class CaptureSession;
}

// Drives a scripted scene and captures images of the application window only.
// QQuickWindow::grabWindow renders the window's own scene graph, so nothing
// else on the desktop can ever end up in an image.
//
// Modes:
//  - Demo: deterministic. Simulated time, the scripted scene and all QML
//    animations advance by exactly 1/12 s per captured frame (manual clock +
//    custom animation driver), so playback at 12 fps is smooth however slow
//    rendering is. JPEG frames go to <out>/frames, six PNG screenshots to
//    <out>/screenshots.
//  - LayoutCheck: short wall-clock scene that captures edge cases (empty
//    states, minimum window size, wide window, bad database file) into
//    <out>/checks.
class DemoDirector : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Demo, LayoutCheck };

    static constexpr int kFps = 12;
    static constexpr int kDemoDurationMs = 45000;
    static constexpr int kCheckDurationMs = 25000;
    static constexpr int kJpegQuality = 80; // phase 3 plots: 85 gave 62.6 MB (see decisions)

    DemoDirector(Mode mode, QQuickWindow *window, can::CaptureSession *session,
                 const QString &outputDir, QObject *parent = nullptr);
    ~DemoDirector() override;

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
    void runStepsUntil(qint64 ms);
    void demoFrame();      // one deterministic frame
    void checkTick();      // wall-clock tick for LayoutCheck
    void screenshot(const QString &name);
    void act(const QString &action, const QVariant &arg = {});
    void resizeWindow(int width, int height);
    void finish();
    static QDir cleanDir(const QDir &root, const QString &name, const QStringList &patterns);

    Mode m_mode;
    QPointer<QQuickWindow> m_window;
    can::CaptureSession *m_session;
    QDir m_root;
    QDir m_framesDir;
    QDir m_shotsDir;
    std::vector<Step> m_steps;
    std::size_t m_nextStep = 0;
    int m_durationMs = kDemoDurationMs;
    int m_frameIndex = 0;
    int m_shotIndex = 0;
    bool m_finished = false;
    qint64 m_bytesWritten = 0;
    QElapsedTimer m_wall;
    QTimer m_checkTimer;
    std::unique_ptr<StepAnimationDriver> m_driver;
};
