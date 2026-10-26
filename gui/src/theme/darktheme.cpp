#include "darktheme.h"
#include <QPalette>
#include <QColor>

void DarkTheme::apply(QApplication *app) {
    if (!app) return;

    app->setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window, QColor("#000000"));
    p.setColor(QPalette::WindowText, QColor("#ffffff"));
    p.setColor(QPalette::Base, QColor("#000000"));
    p.setColor(QPalette::AlternateBase, QColor("#0d0d0d"));
    p.setColor(QPalette::ToolTipBase, QColor("#000000"));
    p.setColor(QPalette::ToolTipText, QColor("#ffffff"));
    p.setColor(QPalette::Text, QColor("#ffffff"));
    p.setColor(QPalette::Button, QColor("#111111"));
    p.setColor(QPalette::ButtonText, QColor("#ffffff"));
    p.setColor(QPalette::BrightText, QColor("#ffffff"));
    p.setColor(QPalette::Link, QColor("#cccccc"));
    p.setColor(QPalette::Highlight, QColor("#ffffff"));
    p.setColor(QPalette::HighlightedText, QColor("#000000"));
    app->setPalette(p);

    app->setStyleSheet(getStyleSheet());
}

QString DarkTheme::getStyleSheet() {
    return R"(
        * {
            border-radius: 0px !important;
            font-family: "DejaVu Sans Mono", "Liberation Mono", "Consolas", "Courier New", monospace;
            font-size: 12px;
        }

        QMainWindow {
            background-color: #000000;
        }

        QWidget {
            background-color: #000000;
            color: #ffffff;
        }

        QTabWidget::pane {
            border: 1px solid #333333;
            background-color: #000000;
            top: -1px;
        }

        QTabBar::tab {
            background: #0a0a0a;
            color: #777777;
            padding: 8px 18px;
            margin-right: 2px;
            border: 1px solid #222222;
            border-bottom: 1px solid #333333;
            font-weight: bold;
        }

        QTabBar::tab:selected {
            background: #000000;
            color: #ffffff;
            border: 1px solid #333333;
            border-bottom: 1px solid #000000;
        }

        QTabBar::tab:hover:!selected {
            background: #141414;
            color: #cccccc;
        }

        QGroupBox {
            border: 1px solid #333333;
            margin-top: 18px;
            padding-top: 12px;
            font-weight: bold;
            color: #ffffff;
            background-color: #050505;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 6px;
            background-color: #000000;
            color: #ffffff;
        }

        QPushButton {
            background-color: #0d0d0d;
            border: 1px solid #444444;
            padding: 6px 14px;
            font-weight: bold;
            color: #ffffff;
            min-height: 22px;
        }

        QPushButton:hover {
            background-color: #222222;
            border-color: #ffffff;
            color: #ffffff;
        }

        QPushButton:pressed {
            background-color: #ffffff;
            color: #000000;
            border-color: #ffffff;
        }

        QPushButton:disabled {
            background-color: #050505;
            border-color: #222222;
            color: #444444;
        }

        QPushButton#btnPrimary {
            background-color: #ffffff;
            color: #000000;
            border: 1px solid #ffffff;
            font-weight: bold;
        }

        QPushButton#btnPrimary:hover {
            background-color: #cccccc;
            border-color: #cccccc;
        }

        QPushButton#btnPrimary:pressed {
            background-color: #888888;
        }

        QPushButton#btnDanger {
            background-color: #1a0000;
            color: #ffffff;
            border: 1px solid #770000;
            font-weight: bold;
        }

        QPushButton#btnDanger:hover {
            background-color: #ffffff;
            color: #000000;
            border-color: #ffffff;
        }

        QPushButton#btnSuccess {
            background-color: #001a00;
            color: #ffffff;
            border: 1px solid #007700;
            font-weight: bold;
        }

        QPushButton#btnSuccess:hover {
            background-color: #ffffff;
            color: #000000;
            border-color: #ffffff;
        }

        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #050505;
            border: 1px solid #444444;
            padding: 5px 8px;
            color: #ffffff;
            selection-background-color: #ffffff;
            selection-color: #000000;
        }

        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border: 1px solid #ffffff;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: 1px solid #444444;
        }

        QComboBox QAbstractItemView {
            background-color: #0a0a0a;
            border: 1px solid #ffffff;
            selection-background-color: #ffffff;
            selection-color: #000000;
            color: #ffffff;
        }

        QTableWidget, QTableView {
            background-color: #000000;
            gridline-color: #222222;
            border: 1px solid #333333;
            selection-background-color: #ffffff;
            selection-color: #000000;
        }

        QTableWidget::item:selected, QTableView::item:selected {
            background-color: #ffffff;
            color: #000000;
        }

        QHeaderView::section {
            background-color: #111111;
            color: #ffffff;
            padding: 6px;
            border: none;
            border-right: 1px solid #333333;
            border-bottom: 1px solid #333333;
            font-weight: bold;
        }

        QTextEdit, QPlainTextEdit {
            background-color: #000000;
            border: 1px solid #333333;
            color: #ffffff;
            font-family: "DejaVu Sans Mono", "Liberation Mono", monospace;
            font-size: 11px;
        }

        QScrollBar:vertical {
            background: #000000;
            width: 10px;
            margin: 0;
            border-left: 1px solid #222222;
        }

        QScrollBar::handle:vertical {
            background: #333333;
            min-height: 20px;
        }

        QScrollBar::handle:vertical:hover {
            background: #777777;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }

        QScrollBar:horizontal {
            background: #000000;
            height: 10px;
            margin: 0;
            border-top: 1px solid #222222;
        }

        QScrollBar::handle:horizontal {
            background: #333333;
            min-width: 20px;
        }

        QSplitter::handle {
            background-color: #222222;
        }

        QSplitter::handle:hover {
            background-color: #ffffff;
        }

        QStatusBar {
            background-color: #050505;
            border-top: 1px solid #333333;
            color: #888888;
        }
    )";
}
