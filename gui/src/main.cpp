#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include "mainwindow.h"
#include "theme/darktheme.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("GhostRFStudio");
    app.setApplicationDisplayName("GHOSTRF STUDIO");
    app.setOrganizationName("GhostRF");

    DarkTheme::apply(&app);

    // Иконка приложения (Белый пиксельный призрак на черном фоне)
    QPixmap iconPix(":/assets/ghost_white.png");
    if (iconPix.isNull()) {
        iconPix.load("src/assets/ghost_logo_white.png");
    }
    if (!iconPix.isNull()) {
        app.setWindowIcon(QIcon(iconPix));
    }

    // Ретро заставка OLED дисплея устройства с призраком GHOSTRF
    QPixmap splashPix(":/assets/oled_splash.png");
    if (splashPix.isNull()) {
        splashPix.load("src/assets/oled_splash.png");
    }

    QSplashScreen *splash = nullptr;
    if (!splashPix.isNull()) {
        splash = new QSplashScreen(splashPix.scaled(512, 256, Qt::KeepAspectRatio, Qt::FastTransformation));
        splash->show();
        app.processEvents();
    }

    MainWindow window;
    if (!iconPix.isNull()) {
        window.setWindowIcon(QIcon(iconPix));
    }

    if (splash) {
        QTimer::singleShot(750, [splash, &window]() {
            splash->finish(&window);
            splash->deleteLater();
        });
    }

    window.show();

    return app.exec();
}
