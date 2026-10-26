#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QImage>
#include <QFont>
#include <QColor>
#include "mainwindow.h"
#include "theme/darktheme.h"
#include "splash_frame.h"

static QPixmap getSplashPixmap() {
    QPixmap splashPix(":/assets/oled_splash.png");
    if (!splashPix.isNull()) {
        return splashPix.scaled(512, 256, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    splashPix.load("src/assets/oled_splash.png");
    if (!splashPix.isNull()) {
        return splashPix.scaled(512, 256, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    splashPix.load("gui/src/assets/oled_splash.png");
    if (!splashPix.isNull()) {
        return splashPix.scaled(512, 256, Qt::KeepAspectRatio, Qt::FastTransformation);
    }

    // Надёжный программный fallback из аппаратного массива кадра дисплея SSD1306 (128x64)
    QImage img(128, 64, QImage::Format_RGB32);
    img.fill(qRgb(0, 0, 0)); // Черный фон
    for (int page = 0; page < 8; ++page) {
        for (int col = 0; col < 128; ++col) {
            uint8_t byteVal = GHOSTRF_SPLASH_FRAME[page * 128 + col];
            for (int b = 0; b < 8; ++b) {
                if (byteVal & (1 << b)) {
                    img.setPixel(col, page * 8 + b, qRgb(255, 255, 255)); // Белый призрак
                }
            }
        }
    }
    return QPixmap::fromImage(img.scaled(512, 256, Qt::KeepAspectRatio, Qt::FastTransformation));
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("GHOSTRF-Studio");
    app.setApplicationDisplayName("GHOSTRF STUDIO");
    app.setOrganizationName("GHOSTRF");
    app.setDesktopFileName("GHOSTRF-Studio");


    DarkTheme::apply(&app);

    // Иконка приложения (Белый пиксельный призрак на черном фоне)
    QPixmap iconPix(":/assets/ghost_white.png");
    if (iconPix.isNull()) {
        iconPix.load("src/assets/ghost_logo_white.png");
    }
    if (iconPix.isNull()) {
        iconPix.load("gui/src/assets/ghost_logo_white.png");
    }
    if (!iconPix.isNull()) {
        app.setWindowIcon(QIcon(iconPix));
    }

    // Ретро заставка OLED дисплея устройства с белым призраком на черном фоне
    QPixmap splashPix = getSplashPixmap();
    auto *splash = new QSplashScreen(splashPix, Qt::WindowStaysOnTopHint | Qt::SplashScreen);

    QFont splashFont("Monospace", 9, QFont::Bold);
    splash->setFont(splashFont);
    splash->show();
    splash->showMessage("[ GHOSTRF STUDIO // INITIALIZING CORE SYSTEMS... ]",
                        Qt::AlignBottom | Qt::AlignHCenter, QColor("#00FF66"));
    app.processEvents();

    MainWindow window;
    if (!iconPix.isNull()) {
        window.setWindowIcon(QIcon(iconPix));
    }

    // Обновление статуса в заставке через 900мс
    QTimer::singleShot(900, [splash]() {
        if (splash) {
            splash->showMessage("[ GHOSTRF STUDIO // CC1101 & SUB-1GHz ENGINE READY ]",
                                Qt::AlignBottom | Qt::AlignHCenter, QColor("#00FF66"));
        }
    });

    // Показываем главное окно только через 1800мс, гарантируя отображение заставки
    QTimer::singleShot(1800, [splash, &window]() {
        window.show();
        if (splash) {
            splash->finish(&window);
            splash->deleteLater();
        }
    });

    return app.exec();
}

