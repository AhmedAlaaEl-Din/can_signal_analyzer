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

// Drives a scripted demo scene and captures PNG frames of the application
// window only (QQuickWindow::grabWindow renders the scene graph offscreen, so
// nothing else on the desktop can ever end up in a frame).
class DemoDirector : public QObject
{
    Q_OBJECT
public:
    static constexpr int kFrameIntervalMs = 200; // 5 frames per second
    static constexpr int kDurationMs = 45000;

    DemoDirector(QQuickWindow *window, can::CaptureSession *session, const QString &outputDir,
                 QObject *parent = nullptr);

    void start();

signals:
    void finished();

private:
    struct Step
    {
        int atMs;
        std::function<void()> run;
    };

    void buildScript();
    void tick();
    void captureFrame();
    void screenshot(const QString &name);
    void act(const QString &action, const QVariant &arg = {});
    void finish();

    QPointer<QQuickWindow> m_window;
    can::CaptureSession *m_session;
    QDir m_framesDir;
    QDir m_shotsDir;
    std::vector<Step> m_steps;
    std::size_t m_nextStep = 0;
    QElapsedTimer m_clock;
    QTimer m_stepTimer;
    QTimer m_frameTimer;
    int m_frameIndex = 0;
    int m_swaps = 0;           // rendered frames since the last log line
    qint64 m_lastLogMs = 0;
    int m_shotIndex = 0;
};
