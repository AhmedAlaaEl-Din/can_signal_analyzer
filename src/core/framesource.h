#pragma once

#include "canframe.h"

#include <QObject>

namespace can {

// Abstract producer of CAN frames (simulator now; hardware/replay later).
// Implementations deliver frames in batches through framesReady(), which is
// emitted in the source's own thread context; receivers in the GUI thread get
// it through a queued connection.
class FrameSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)

public:
    using QObject::QObject;
    ~FrameSource() override = default;

    virtual bool isRunning() const = 0;
    virtual QString description() const = 0;

public slots:
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void framesReady(const can::CanFrameBatch &batch);
    void runningChanged(bool running);
};

} // namespace can
