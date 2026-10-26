#include "oleddisplaywidget.h"
#include <QPainter>
#include <QDateTime>
#include <algorithm>

OledDisplayWidget::OledDisplayWidget(QWidget *parent)
    : QWidget(parent)
{
    m_buffer.resize(1024);
    m_buffer.fill(0);
    setMinimumSize(384, 192); // 3x масштаб как минимум
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void OledDisplayWidget::updateFrame(const QByteArray &data) {
    if (data.size() == 1024) {
        m_buffer = data;
        m_frameCount++;

        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastFrameTime > 0) {
            qint64 diff = now - m_lastFrameTime;
            if (diff > 0) {
                double instFps = 1000.0 / diff;
                m_currentFps = (m_currentFps * 0.8) + (instFps * 0.2);
            }
        }
        m_lastFrameTime = now;
        update();
    }
}

void OledDisplayWidget::clearDisplay() {
    m_buffer.fill(0);
    m_frameCount = 0;
    m_currentFps = 0.0;
    update();
}

void OledDisplayWidget::setInverted(bool inverted) {
    if (m_inverted != inverted) {
        m_inverted = inverted;
        update();
    }
}

void OledDisplayWidget::setPixelGrid(bool showGrid) {
    if (m_showGrid != showGrid) {
        m_showGrid = showGrid;
        update();
    }
}

QImage OledDisplayWidget::grabDisplayImage() const {
    QImage img(128, 64, QImage::Format_Mono);
    img.fill(0);

    const uchar *buf = reinterpret_cast<const uchar*>(m_buffer.constData());
    for (int y = 0; y < 64; y++) {
        int page = y / 8;
        int bit = y % 8;
        for (int x = 0; x < 128; x++) {
            bool on = (buf[page * 128 + x] >> bit) & 1;
            if (m_inverted) on = !on;
            img.setPixel(x, y, on ? 1 : 0);
        }
    }
    return img;
}

void OledDisplayWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int w = width();
    const int h = height();

    // 1. Внешний корпус устройства
    p.fillRect(0, 0, w, h, QColor("#000000"));
    p.setPen(QPen(QColor("#333333"), 1));
    p.drawRect(0, 0, w - 1, h - 1);

    // Вычисляем масштаб экрана (128x64) с сохранением пропорций 2:1
    const int padding = 14;
    int availW = w - (padding * 2);
    int availH = h - (padding * 2) - 24; // место под статусную строку

    int scaleX = std::max(1, availW / 128);
    int scaleY = std::max(1, availH / 64);
    int scale = std::min(scaleX, scaleY);
    if (scale < 1) scale = 1;

    int displayW = 128 * scale;
    int displayH = 64 * scale;

    int startX = (w - displayW) / 2;
    int startY = padding + (availH - displayH) / 2;

    // Рамка дисплея
    p.fillRect(startX - 2, startY - 2, displayW + 4, displayH + 4, QColor("#111111"));
    p.setPen(QPen(QColor("#555555"), 1));
    p.drawRect(startX - 2, startY - 2, displayW + 3, displayH + 3);

    // Отрисовка пикселей SSD1306 (1024 байта: 8 страниц по 128 байт)
    const uchar *buf = reinterpret_cast<const uchar*>(m_buffer.constData());
    QColor onColor = m_inverted ? QColor("#000000") : QColor("#ffffff");
    QColor offColor = m_inverted ? QColor("#ffffff") : QColor("#000000");

    p.fillRect(startX, startY, displayW, displayH, offColor);

    p.setPen(Qt::NoPen);
    p.setBrush(onColor);

    int pixelDrawSize = (m_showGrid && scale >= 3) ? (scale - 1) : scale;

    for (int y = 0; y < 64; y++) {
        int page = y / 8;
        int bit = y % 8;
        for (int x = 0; x < 128; x++) {
            bool on = (buf[page * 128 + x] >> bit) & 1;
            if (on) {
                p.drawRect(startX + (x * scale), startY + (y * scale), pixelDrawSize, pixelDrawSize);
            }
        }
    }

    // Текстовая строка статуса внизу
    p.setPen(QColor("#777777"));
    p.setFont(QFont("Monospace", 8, QFont::Normal));
    QString status = QString("OLED SSD1306 [128x64] | МАСШТАБ: %1x | КАДРОВ: %2 | FPS: %3")
                         .arg(scale)
                         .arg(m_frameCount)
                         .arg(m_currentFps, 0, 'f', 1);

    p.drawText(QRect(padding, h - 22, w - (padding * 2), 18), Qt::AlignLeft | Qt::AlignVCenter, status);

    p.drawText(QRect(padding, h - 22, w - (padding * 2), 18), Qt::AlignRight | Qt::AlignVCenter,
               m_frameCount > 0 ? "[ В ЭФИРЕ ]" : "[ ОЖИДАНИЕ СИГНАЛА ]");
}
