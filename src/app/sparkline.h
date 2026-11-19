#pragma once

#include <QColor>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

#include <vector>

namespace can {
class SignalListModel;
}

// Tiny antialiased line chart of one signal's recent history.
// Reads samples directly from SignalListModel (no QVariant copies per frame);
// QML only passes the model, the signal id and a "revision" that changes when
// new samples arrive, plus the end of the time window.
class Sparkline : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QObject *model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(int signalId READ signalId WRITE setSignalId NOTIFY signalIdChanged)
    Q_PROPERTY(qulonglong revision READ revision WRITE setRevision NOTIFY revisionChanged)
    Q_PROPERTY(qint64 timeUs READ timeUs WRITE setTimeUs NOTIFY timeUsChanged)
    Q_PROPERTY(qint64 windowUs READ windowUs WRITE setWindowUs NOTIFY windowUsChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QColor areaColor READ areaColor WRITE setAreaColor NOTIFY colorChanged)
    Q_PROPERTY(QColor gridColor READ gridColor WRITE setGridColor NOTIFY colorChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY colorChanged)
    Q_PROPERTY(bool stepped READ stepped WRITE setStepped NOTIFY colorChanged)
    Q_PROPERTY(bool showDot READ showDot WRITE setShowDot NOTIFY colorChanged)

public:
    explicit Sparkline(QQuickItem *parent = nullptr);

    QObject *model() const;
    void setModel(QObject *model);
    int signalId() const { return m_signalId; }
    void setSignalId(int id);
    qulonglong revision() const { return m_revision; }
    void setRevision(qulonglong r);
    qint64 timeUs() const { return m_timeUs; }
    void setTimeUs(qint64 t);
    qint64 windowUs() const { return m_windowUs; }
    void setWindowUs(qint64 w);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    QColor areaColor() const { return m_fill; }
    void setAreaColor(const QColor &c);
    QColor gridColor() const { return m_grid; }
    void setGridColor(const QColor &c);
    qreal lineWidth() const { return m_lineWidth; }
    void setLineWidth(qreal w);
    bool stepped() const { return m_stepped; }
    void setStepped(bool s);
    bool showDot() const { return m_showDot; }
    void setShowDot(bool s);

    void paint(QPainter *painter) override;

signals:
    void modelChanged();
    void signalIdChanged();
    void revisionChanged();
    void timeUsChanged();
    void windowUsChanged();
    void colorChanged();

private:
    QPointer<can::SignalListModel> m_model;
    int m_signalId = -1;
    qulonglong m_revision = 0;
    qint64 m_timeUs = 0;
    qint64 m_windowUs = 10'000'000;
    QColor m_color = QColor(0x5A, 0x43, 0xD6);
    QColor m_fill = QColor(0x5A, 0x43, 0xD6, 40);
    QColor m_grid = Qt::transparent;
    qreal m_lineWidth = 1.5;
    bool m_stepped = false;
    bool m_showDot = true;
};
