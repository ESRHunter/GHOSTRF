#include "waveformwidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <algorithm>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setMinimumHeight(140);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void WaveformWidget::setPulses(const QVector<uint16_t> &pulses, bool firstLevelHigh) {
    m_pulses = pulses;
    m_firstLevelHigh = firstLevelHigh;
    rebuildIntervals();
    resetZoom();
    update();
}

void WaveformWidget::clear() {
    m_pulses.clear();
    m_intervals.clear();
    m_totalDurationUs = 0;
    m_hoveredPulseIdx = -1;
    resetZoom();
    update();
}

void WaveformWidget::zoomIn() {
    m_zoom = std::min(50.0, m_zoom * 1.3);
    update();
}

void WaveformWidget::zoomOut() {
    m_zoom = std::max(1.0, m_zoom / 1.3);
    if (m_zoom <= 1.0) {
        m_scrollX = 0;
    }
    update();
}

void WaveformWidget::resetZoom() {
    m_zoom = 1.0;
    m_scrollX = 0.0;
    update();
}

void WaveformWidget::setInverted(bool inverted) {
    if (m_inverted != inverted) {
        m_inverted = inverted;
        rebuildIntervals();
        update();
    }
}

void WaveformWidget::rebuildIntervals() {
    m_intervals.clear();
    m_totalDurationUs = 0;
    m_hoveredPulseIdx = -1;

    bool currentHigh = m_firstLevelHigh;
    if (m_inverted) currentHigh = !currentHigh;

    quint64 curTime = 0;
    for (int i = 0; i < m_pulses.size(); i++) {
        uint16_t dur = m_pulses[i];
        PulseInterval pi;
        pi.index = i;
        pi.isHigh = currentHigh;
        pi.duration = dur;
        pi.startUs = curTime;
        pi.endUs = curTime + dur;
        m_intervals.append(pi);

        curTime += dur;
        currentHigh = !currentHigh;
    }
    m_totalDurationUs = curTime;
}

double WaveformWidget::usToScreenX(quint64 us, double plotWidth) const {
    if (m_totalDurationUs == 0) return 0;
    double norm = static_cast<double>(us) / static_cast<double>(m_totalDurationUs);
    return (norm * plotWidth * m_zoom) - m_scrollX;
}

qint64 WaveformWidget::screenXToUs(double screenX, double plotWidth) const {
    if (m_totalDurationUs == 0 || plotWidth <= 0 || m_zoom <= 0) return 0;
    double norm = (screenX + m_scrollX) / (plotWidth * m_zoom);
    return static_cast<qint64>(norm * m_totalDurationUs);
}

void WaveformWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const int w = width();
    const int h = height();

    // 1. Фон осциллографа
    p.fillRect(0, 0, w, h, QColor("#000000"));
    p.setPen(QPen(QColor("#333333"), 1));
    p.drawRect(0, 0, w - 1, h - 1);

    if (m_intervals.isEmpty() || m_totalDurationUs == 0) {
        p.setPen(QColor("#666666"));
        QFont f = p.font();
        f.setPointSize(10);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, "[ НЕТ ДАННЫХ ОСЦИЛЛОГРАММЫ / ВЫБЕРИТЕ СИГНАЛ ]");
        return;
    }

    const int paddingLeft = 12;
    const int paddingRight = 12;
    const int paddingTop = 26; // Место под временную линейку
    const int paddingBottom = 22; // Место под статусную строку
    const double plotWidth = w - paddingLeft - paddingRight;
    const double plotHeight = h - paddingTop - paddingBottom;

    const double yHigh = paddingTop + 10;
    const double yLow = paddingTop + plotHeight - 10;

    // 2. Сетка и шкала времени
    p.save();
    p.setClipRect(paddingLeft, 0, plotWidth, h);

    // Выбираем шаг сетки времени
    double visibleUs = m_totalDurationUs / m_zoom;
    double rawStepUs = visibleUs / 8.0;
    double power = std::pow(10, std::floor(std::log10(rawStepUs)));
    double fraction = rawStepUs / power;
    double stepUs = power;
    if (fraction >= 5.0) stepUs = 5 * power;
    else if (fraction >= 2.0) stepUs = 2 * power;
    if (stepUs < 10) stepUs = 10;

    qint64 startVisibleUs = std::max<qint64>(0, screenXToUs(0, plotWidth));
    qint64 endVisibleUs = std::min<qint64>(m_totalDurationUs, screenXToUs(plotWidth, plotWidth));
    qint64 firstTick = (startVisibleUs / static_cast<qint64>(stepUs)) * static_cast<qint64>(stepUs);

    p.setFont(QFont("Monospace", 8));
    for (qint64 t = firstTick; t <= endVisibleUs + static_cast<qint64>(stepUs); t += static_cast<qint64>(stepUs)) {
        double sx = paddingLeft + usToScreenX(t, plotWidth);
        if (sx < paddingLeft || sx > paddingLeft + plotWidth) continue;

        // Вертикальная линия сетки
        p.setPen(QPen(QColor("#222222"), 1, Qt::DashLine));
        p.drawLine(QPointF(sx, paddingTop), QPointF(sx, paddingTop + plotHeight));

        // Насечка на линейке
        p.setPen(QPen(QColor("#555555"), 1));
        p.drawLine(QPointF(sx, paddingTop - 6), QPointF(sx, paddingTop));

        // Метка времени
        QString label;
        if (t >= 1000000) {
            label = QString("%1 с").arg(t / 1000000.0, 0, 'f', 2);
        } else if (t >= 1000) {
            label = QString("%1 мс").arg(t / 1000.0, 0, 'f', 1);
        } else {
            label = QString("%1 µs").arg(t);
        }
        p.setPen(QColor("#888888"));
        p.drawText(QRectF(sx - 40, 2, 80, paddingTop - 8), Qt::AlignCenter, label);
    }
    p.restore();

    // Горизонтальные опорные линии High / Low
    p.setPen(QPen(QColor("#222222"), 1, Qt::DotLine));
    p.drawLine(QPointF(paddingLeft, yHigh), QPointF(w - paddingRight, yHigh));
    p.drawLine(QPointF(paddingLeft, yLow), QPointF(w - paddingRight, yLow));

    // Метки уровней
    p.setPen(QColor("#ffffff"));
    p.drawText(QRectF(w - paddingRight - 36, yHigh - 12, 36, 12), Qt::AlignRight, "HIGH");
    p.setPen(QColor("#666666"));
    p.drawText(QRectF(w - paddingRight - 36, yLow + 2, 36, 12), Qt::AlignRight, "LOW");

    // 3. Рисуем форму сигнала (Digital Waveform)
    p.save();
    p.setClipRect(paddingLeft, paddingTop, plotWidth, plotHeight);

    QPainterPath wavePath;
    QPainterPath fillPath;

    bool started = false;
    double lastX = 0;
    double lastY = yLow;

    for (int i = 0; i < m_intervals.size(); i++) {
        const PulseInterval &pi = m_intervals[i];
        double x1 = paddingLeft + usToScreenX(pi.startUs, plotWidth);
        double x2 = paddingLeft + usToScreenX(pi.endUs, plotWidth);
        double y = pi.isHigh ? yHigh : yLow;

        if (x2 < paddingLeft - 50) continue;
        if (x1 > w - paddingRight + 50) break;

        if (!started) {
            wavePath.moveTo(x1, y);
            fillPath.moveTo(x1, yLow);
            fillPath.lineTo(x1, y);
            started = true;
        } else {
            // Фронт (переход)
            wavePath.lineTo(x1, y);
            fillPath.lineTo(x1, y);
        }

        // Полка импульса
        wavePath.lineTo(x2, y);
        fillPath.lineTo(x2, y);

        lastX = x2;
        lastY = y;
    }

    if (started) {
        fillPath.lineTo(lastX, yLow);
        fillPath.closeSubpath();

        // Заливка монохромным градиентом
        p.fillPath(fillPath, QColor(255, 255, 255, 20));

        // Линия сигнала (Белая)
        QPen tracePen(QColor("#ffffff"), 1.5);
        tracePen.setCapStyle(Qt::SquareCap);
        tracePen.setJoinStyle(Qt::MiterJoin);
        p.setPen(tracePen);
        p.drawPath(wavePath);
    }

    // 4. Подсветка импульса при наведении курсора
    if (m_hoveredPulseIdx >= 0 && m_hoveredPulseIdx < m_intervals.size()) {
        const PulseInterval &hpi = m_intervals[m_hoveredPulseIdx];
        double hx1 = paddingLeft + usToScreenX(hpi.startUs, plotWidth);
        double hx2 = paddingLeft + usToScreenX(hpi.endUs, plotWidth);

        p.fillRect(QRectF(hx1, paddingTop, hx2 - hx1, plotHeight), QColor(255, 255, 255, 50));
        p.setPen(QPen(QColor("#ffffff"), 1.5, Qt::SolidLine));
        p.drawLine(QPointF(hx1, paddingTop), QPointF(hx1, paddingTop + plotHeight));
        p.drawLine(QPointF(hx2, paddingTop), QPointF(hx2, paddingTop + plotHeight));

        // Выводим длительность импульса над ним, если помещается
        QString durText = QString("%1 µs").arg(hpi.duration);
        p.setPen(QColor("#ffffff"));
        p.drawText(QRectF(hx1 - 30, paddingTop + (plotHeight / 2) - 8, std::max(60.0, hx2 - hx1 + 60), 16),
                   Qt::AlignCenter, durText);
    }

    p.restore();

    // 5. Информационная панель внизу (Статус)
    p.setPen(QColor("#94a3b8"));
    p.setFont(QFont("Sans", 8));
    QString statusText = QString("Всего: %1 импульсов | Длительность: %2 мс (%3 µs) | Зум: %4x")
                             .arg(m_intervals.size())
                             .arg(m_totalDurationUs / 1000.0, 0, 'f', 2)
                             .arg(m_totalDurationUs)
                             .arg(m_zoom, 0, 'f', 1);

    p.drawText(QRectF(paddingLeft, h - paddingBottom + 2, plotWidth, paddingBottom - 2),
               Qt::AlignLeft | Qt::AlignVCenter, statusText);

    p.setPen(QColor("#64748b"));
    p.drawText(QRectF(paddingLeft, h - paddingBottom + 2, plotWidth, paddingBottom - 2),
               Qt::AlignRight | Qt::AlignVCenter, "Колесико мыши: зум | Перетаскивание: панорама");
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event) {
    const int paddingLeft = 12;
    const int paddingRight = 12;
    const double plotWidth = width() - paddingLeft - paddingRight;

    if (m_isDragging) {
        double delta = event->pos().x() - m_dragStartX;
        m_scrollX = m_dragStartScrollX - delta;

        double maxScroll = std::max(0.0, (plotWidth * m_zoom) - plotWidth);
        m_scrollX = std::clamp(m_scrollX, 0.0, maxScroll);
        update();
        return;
    }

    if (m_totalDurationUs > 0 && plotWidth > 0) {
        double mouseX = event->pos().x() - paddingLeft;
        qint64 mouseUs = screenXToUs(mouseX, plotWidth);

        int foundIdx = -1;
        for (int i = 0; i < m_intervals.size(); i++) {
            if (mouseUs >= static_cast<qint64>(m_intervals[i].startUs) &&
                mouseUs < static_cast<qint64>(m_intervals[i].endUs)) {
                foundIdx = i;
                break;
            }
        }

        if (foundIdx != m_hoveredPulseIdx) {
            m_hoveredPulseIdx = foundIdx;
            if (foundIdx >= 0) {
                const PulseInterval &pi = m_intervals[foundIdx];
                emit pulseHovered(pi.index, pi.isHigh, pi.duration, pi.startUs);

                QString tip = QString("Импульс #%1 (%2)\nДлительность: %3 µs\nНачало: %4 мс\nКонец: %5 мс")
                                  .arg(pi.index + 1)
                                  .arg(pi.isHigh ? "HIGH" : "LOW")
                                  .arg(pi.duration)
                                  .arg(pi.startUs / 1000.0, 0, 'f', 3)
                                  .arg(pi.endUs / 1000.0, 0, 'f', 3);
                QToolTip::showText(event->globalPosition().toPoint(), tip, this);
            }
            update();
        }
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartX = event->pos().x();
        m_dragStartScrollX = m_scrollX;
        setCursor(Qt::ClosedHandCursor);
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformWidget::wheelEvent(QWheelEvent *event) {
    const int paddingLeft = 12;
    const int paddingRight = 12;
    const double plotWidth = width() - paddingLeft - paddingRight;

    if (m_totalDurationUs == 0 || plotWidth <= 0) return;

    double mouseX = event->position().x() - paddingLeft;
    qint64 anchorUs = screenXToUs(mouseX, plotWidth);

    double oldZoom = m_zoom;
    if (event->angleDelta().y() > 0) {
        m_zoom = std::min(50.0, m_zoom * 1.25);
    } else {
        m_zoom = std::max(1.0, m_zoom / 1.25);
    }

    if (m_zoom <= 1.0) {
        m_scrollX = 0;
    } else {
        // Удерживаем курсор на той же временной точке при зуме
        double newNorm = static_cast<double>(anchorUs) / static_cast<double>(m_totalDurationUs);
        m_scrollX = (newNorm * plotWidth * m_zoom) - mouseX;
        double maxScroll = std::max(0.0, (plotWidth * m_zoom) - plotWidth);
        m_scrollX = std::clamp(m_scrollX, 0.0, maxScroll);
    }

    update();
    event->accept();
}

void WaveformWidget::leaveEvent(QEvent *) {
    if (m_hoveredPulseIdx != -1) {
        m_hoveredPulseIdx = -1;
        update();
    }
}
