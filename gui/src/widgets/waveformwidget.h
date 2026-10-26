#pragma once

#include <QWidget>
#include <QVector>
#include <cstdint>

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget *parent = nullptr);

    void setPulses(const QVector<uint16_t> &pulses, bool firstLevelHigh = true);
    void clear();

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void setInverted(bool inverted);
    bool isInverted() const { return m_inverted; }

    quint64 totalDurationUs() const { return m_totalDurationUs; }
    int pulseCount() const { return m_pulses.size(); }

signals:
    void pulseHovered(int index, bool isHigh, uint16_t durationUs, quint64 startUs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct PulseInterval {
        int index;
        bool isHigh;
        uint16_t duration;
        quint64 startUs;
        quint64 endUs;
    };

    QVector<uint16_t> m_pulses;
    QVector<PulseInterval> m_intervals;
    quint64 m_totalDurationUs = 0;
    bool m_firstLevelHigh = true;
    bool m_inverted = false;

    // Масштабирование и панорамирование
    double m_zoom = 1.0;
    double m_scrollX = 0.0;
    bool m_isDragging = false;
    int m_dragStartX = 0;
    double m_dragStartScrollX = 0.0;

    int m_hoveredPulseIdx = -1;

    void rebuildIntervals();
    double usToScreenX(quint64 us, double plotWidth) const;
    qint64 screenXToUs(double screenX, double plotWidth) const;
};
