#pragma once

#include "plotmath.h"

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantList>

namespace can {

class TimeSeriesStore;

// Shared time axis of all plot lanes, measurement cursors and CSV export.
// Times are microseconds since measurement start (as doubles for QML).
//
// Follow live: the view shows the last `windowSeconds`, ending at a smoothed
// "display now" that advances every rendered frame (advance()) and never runs
// ahead of the newest sample, so the plot scrolls at the display rate even
// though data arrives in 30 ms batches. Zooming or panning leaves follow mode;
// follow() resumes it.
class PlotController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool followLive READ followLive WRITE setFollowLive NOTIFY viewChanged)
    Q_PROPERTY(int windowSeconds READ windowSeconds WRITE setWindowSeconds NOTIFY viewChanged)
    Q_PROPERTY(double viewStartUs READ viewStartUs NOTIFY viewChanged)
    Q_PROPERTY(double viewEndUs READ viewEndUs NOTIFY viewChanged)
    Q_PROPERTY(double cursorAUs READ cursorAUs NOTIFY cursorsChanged)
    Q_PROPERTY(double cursorBUs READ cursorBUs NOTIFY cursorsChanged)
    Q_PROPERTY(bool hasCursors READ hasCursors NOTIFY cursorsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    static constexpr std::int64_t kMinSpanUs = 20'000;   // 20 ms
    static constexpr int kDefaultWindowSeconds = 10;

    explicit PlotController(QObject *parent = nullptr);
    void setStore(TimeSeriesStore *store);

    bool followLive() const { return m_follow; }
    void setFollowLive(bool follow);
    int windowSeconds() const { return m_windowSeconds; }
    void setWindowSeconds(int seconds);
    double viewStartUs() const { return static_cast<double>(m_view.t0); }
    double viewEndUs() const { return static_cast<double>(m_view.t1); }
    plotmath::TimeRange view() const { return m_view; }
    double cursorAUs() const { return m_cursorA; }
    double cursorBUs() const { return m_cursorB; }
    bool hasCursors() const { return m_cursorA >= 0 && m_cursorB >= 0; }
    QString lastError() const { return m_lastError; }

    // Advance the smoothed live clock by one rendered frame.
    Q_INVOKABLE void advance(double dtSeconds);
    Q_INVOKABLE void follow();
    Q_INVOKABLE void zoomAt(double anchorFraction, double factor);
    Q_INVOKABLE void panByFraction(double fraction);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void setView(double t0Us, double t1Us);

    Q_INVOKABLE void setCursor(int which, double timeUs); // 0 = A, 1 = B
    Q_INVOKABLE void placeCursors();                       // A at 1/3, B at 2/3 of the view
    Q_INVOKABLE void clearCursors();
    // {dtText, freqText, dtSeconds}
    Q_INVOKABLE QVariantMap cursorDelta() const;

    // [{x, label}] for the shared time axis.
    Q_INVOKABLE QVariantList timeTicks(double widthPx) const;
    // [{y, label}] for a value axis (y measured from the top).
    Q_INVOKABLE QVariantList valueTicks(double lo, double hi, double heightPx) const;
    // [lo, hi] with margin (autoscale).
    Q_INVOKABLE QVariantList paddedRange(double lo, double hi) const;
    Q_INVOKABLE double timeAtX(double x, double widthPx) const;
    Q_INVOKABLE double xOfTime(double timeUs, double widthPx) const;
    Q_INVOKABLE QString formatTime(double timeUs) const;
    Q_INVOKABLE QString formatValue(double value, int decimals) const;

    // Writes the visible time range of the given signals as CSV (one column
    // per signal, one row per sample time). Returns false and sets lastError
    // on failure.
    Q_INVOKABLE bool exportCsv(const QUrl &fileUrl, const QVariantList &signalIds, const QStringList &names);
    bool writeCsv(const QString &path, const QList<int> &signalIds, const QStringList &names);

signals:
    void viewChanged();
    void cursorsChanged();
    void lastErrorChanged();

private:
    void applyFollow();
    std::int64_t newestUs() const;

    QPointer<TimeSeriesStore> m_store;
    bool m_follow = true;
    int m_windowSeconds = kDefaultWindowSeconds;
    double m_displayNowUs = 0;
    plotmath::TimeRange m_view{-kDefaultWindowSeconds * 1'000'000LL, 0};
    double m_cursorA = -1;
    double m_cursorB = -1;
    QString m_lastError;
};

} // namespace can
