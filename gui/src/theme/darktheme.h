#pragma once

#include <QString>
#include <QApplication>

class DarkTheme {
public:
    static void apply(QApplication *app);
    static QString getStyleSheet();
};
