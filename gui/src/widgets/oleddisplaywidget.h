#pragma once

#include <QWidget>
#include <QByteArray>
#include <QImage>

class OledDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OledDisplayWidget(QWidget *parent = nullptr);

    void updateFrame(const QByteArray &data);
    void clearDisplay();
    void setInverted(bool inverted);
    bool isInverted() const { return m_inverted; }
    void setPixelGrid(bool showGrid);
    bool hasPixelGrid() const { return m_showGrid; }

    QImage grabDisplayImage() const;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QByteArray m_buffer;
    bool m_inverted = false;
    bool m_showGrid = true;
    quint64 m_frameCount = 0;
    qint64 m_lastFrameTime = 0;
    double m_currentFps = 0.0;
};
