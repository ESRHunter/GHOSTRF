#include "mainwindow.h"
#include "widgets/waveformwidget.h"
#include "widgets/oleddisplaywidget.h"
#include "widgets/signaldetaildialog.h"
#include "splash_frame.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QStackedWidget>
#include <QScrollArea>
#include <QFrame>
#include <QProgressBar>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_dev(new DeviceManager(this))
{
    setWindowTitle("GHOSTRF STUDIO");
    resize(1220, 800);

    setupUi();

    // Подключение сигналов от DeviceManager
    connect(m_dev, &DeviceManager::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_dev, &DeviceManager::pongReceived, this, &MainWindow::onPongReceived);
    connect(m_dev, &DeviceManager::configReceived, this, &MainWindow::onConfigReceived);
    connect(m_dev, &DeviceManager::screenFrameReceived, this, &MainWindow::onScreenFrameReceived);

    connect(m_dev, &DeviceManager::signalListBegin, this, &MainWindow::onSignalListBegin);
    connect(m_dev, &DeviceManager::signalItemReceived, this, &MainWindow::onSignalItemReceived);
    connect(m_dev, &DeviceManager::signalListEnd, this, &MainWindow::onSignalListEnd);

    connect(m_dev, &DeviceManager::txFinished, this, [this](bool ok, const QString &msg) {
        appendLog(ok ? "[OK] СИГНАЛ ПЕРЕДАН В ЭФИР" : QString("[ERR] ОШИБКА ПЕРЕДАЧИ: %1").arg(msg), !ok);
    });

    connect(m_dev, &DeviceManager::saveFinished, this, [this](bool ok, int cnt) {
        appendLog(ok ? QString("[OK] СИГНАЛ СОХРАНЕН В NVS (ВСЕГО: %1)").arg(cnt) : "[ERR] ОШИБКА СОХРАНЕНИЯ", !ok);
        m_dev->requestSignals();
    });

    connect(m_dev, &DeviceManager::deleteFinished, this, [this](bool ok, int cnt) {
        appendLog(ok ? QString("[OK] СИГНАЛ УДАЛЕН ИЗ NVS (ОСТАЛОСЬ: %1)").arg(cnt) : "[ERR] ОШИБКА УДАЛЕНИЯ", !ok);
        m_dev->requestSignals();
    });

    connect(m_dev, &DeviceManager::capturedFixedPacket, this, &MainWindow::onCapturedFixedPacket);
    connect(m_dev, &DeviceManager::capturedRawPacket, this, &MainWindow::onCapturedRawPacket);

    connect(m_dev, &DeviceManager::logMessage, this, &MainWindow::appendLog);
    connect(m_dev, &DeviceManager::rawDataSent, this, &MainWindow::appendRawTx);
    connect(m_dev, &DeviceManager::rawDataReceived, this, &MainWindow::appendRawRx);

    refreshSerialPorts();

    // Запускаем фоновый таймер автоподключения устройства
    m_autoConnectTimer = new QTimer(this);
    connect(m_autoConnectTimer, &QTimer::timeout, this, &MainWindow::checkAutoConnect);
    m_autoConnectTimer->start(1500);

    // Первичная попытка подключения, если устройство уже вставлено
    checkAutoConnect();
}

MainWindow::~MainWindow() {
    if (m_autoConnectTimer) {
        m_autoConnectTimer->stop();
    }
    if (m_dev->isConnected()) {
        m_dev->stopScreenStream();
        m_dev->disconnectPort();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (!m_dev->isConnected()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // Если фокус в строке ввода, даем ввести текст
    if (qobject_cast<QLineEdit*>(focusWidget()) || qobject_cast<QPlainTextEdit*>(focusWidget()) || qobject_cast<QTextEdit*>(focusWidget())) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
        case Qt::Key_Up:
        case Qt::Key_W:
            sendVirtualBtn("UP");
            event->accept();
            return;
        case Qt::Key_Down:
        case Qt::Key_S:
            sendVirtualBtn("DOWN");
            event->accept();
            return;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Space:
            sendVirtualBtn("OK");
            event->accept();
            return;
        case Qt::Key_Escape:
        case Qt::Key_Backspace:
        case Qt::Key_M:
            sendVirtualBtn("OK_HOLD");
            event->accept();
            return;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::setupUi() {
    m_rootStack = new QStackedWidget(this);

    // 1. Экран подключения (Onboarding Screen)
    m_connectScreen = createConnectScreen();

    // 2. Основная студия (Main Studio)
    m_mainStudio = new QWidget(this);
    auto *studioLayout = new QVBoxLayout(m_mainStudio);
    studioLayout->setContentsMargins(8, 8, 8, 8);
    studioLayout->setSpacing(6);

    studioLayout->addWidget(createTopBar());

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createDisplayAndRemoteTab(), "[ ДИСПЛЕЙ И ПУЛЬТ ]");
    tabs->addTab(createSavedSignalsTab(), "[ СОХРАНЕННЫЕ СИГНАЛЫ ]");
    tabs->addTab(createTransmitterTab(), "[ ПЕРЕДАТЧИК ]");
    tabs->addTab(createSnifferTab(), "[ СНИФФЕР ЭФИРА ]");
    tabs->addTab(createConsoleTab(), "[ SERIAL КОНСОЛЬ ]");
    studioLayout->addWidget(tabs);

    m_rootStack->addWidget(m_connectScreen); // Index 0
    m_rootStack->addWidget(m_mainStudio);    // Index 1
    m_rootStack->setCurrentWidget(m_connectScreen);

    setCentralWidget(m_rootStack);
}

void MainWindow::showConnectScreen(const QString &statusMsg) {
    if (m_rootStack) {
        m_rootStack->setCurrentWidget(m_connectScreen);
    }
    if (!statusMsg.isEmpty() && m_connectScanStatus) {
        m_connectScanStatus->setText(statusMsg);
    }
}

void MainWindow::showMainStudio() {
    if (m_rootStack) {
        m_rootStack->setCurrentWidget(m_mainStudio);
    }
}

void MainWindow::resetConnectionSteps() {
    if (m_step1Label) {
        m_step1Label->setText("[ ○ ] 1. Обнаружение аппаратного USB CDC / Serial порта");
        m_step1Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    }
    if (m_step2Label) {
        m_step2Label->setText("[ ○ ] 2. Подключение Serial (115200 бод) и проверка связи PING");
        m_step2Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    }
    if (m_step3Label) {
        m_step3Label->setText("[ ○ ] 3. Синхронизация сигналов NVS, параметров CC1101 и OLED дисплея");
        m_step3Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    }
}

QWidget* MainWindow::createConnectScreen() {
    auto *page = new QWidget(this);
    page->setStyleSheet("background-color: #000000;");

    auto *outerLayout = new QVBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border: none; background-color: #000000; }");

    auto *centerContainer = new QWidget(scroll);
    centerContainer->setStyleSheet("background-color: #000000;");
    auto *centerLayout = new QVBoxLayout(centerContainer);
    centerLayout->setContentsMargins(40, 30, 40, 30);
    centerLayout->setSpacing(14);
    centerLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

    auto *contentWidget = new QWidget(centerContainer);
    contentWidget->setMaximumWidth(680);
    contentWidget->setStyleSheet("background-color: #000000;");
    auto *layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // 0. Pixel Ghost Logo (Белый призрак на черном фоне)
    auto *lblGhostLogo = new QLabel(contentWidget);
    QPixmap ghostPix(":/assets/ghost_white.png");
    if (ghostPix.isNull()) {
        ghostPix.load("src/assets/ghost_logo_white.png");
    }
    if (!ghostPix.isNull()) {
        lblGhostLogo->setPixmap(ghostPix.scaled(64, 64, Qt::KeepAspectRatio, Qt::FastTransformation));
        lblGhostLogo->setAlignment(Qt::AlignCenter);
        layout->addWidget(lblGhostLogo);
    }

    // 1. Cyber Header Badge
    auto *lblBadge = new QLabel("[ GHOSTRF // SUB-1GHz ]", contentWidget);
    lblBadge->setStyleSheet("color: #00FF66; font-size: 13px; font-weight: bold; font-family: Monospace;");
    lblBadge->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblBadge);

    auto *lblTitle = new QLabel("GHOSTRF STUDIO", contentWidget);
    lblTitle->setStyleSheet("color: #ffffff; font-size: 26px; font-weight: bold; font-family: Monospace;");
    lblTitle->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblTitle);

    auto *lblSub = new QLabel("Аппаратный терминал управления и анализатором радиоэфира", contentWidget);
    lblSub->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; margin-bottom: 6px;");
    lblSub->setAlignment(Qt::AlignCenter);
    layout->addWidget(lblSub);

    // 2. Stylized Device Chassis Graphic
    auto *chassisBox = new QFrame(contentWidget);
    chassisBox->setStyleSheet("background-color: #0e0e0e; border: 1px solid #262626; padding: 14px;");
    auto *chassisLayout = new QVBoxLayout(chassisBox);
    chassisLayout->setSpacing(10);

    auto *chassisHdrLayout = new QHBoxLayout();
    auto *lblHdrLeft = new QLabel("ESP32-C3 SUPERMINI", chassisBox);
    lblHdrLeft->setStyleSheet("color: #999999; font-size: 10px; font-family: Monospace; border: none; background: transparent;");
    auto *lblHdrRight = new QLabel("CC1101 SUB-1GHz", chassisBox);
    lblHdrRight->setStyleSheet("color: #00FF66; font-size: 10px; font-family: Monospace; font-weight: bold; border: none; background: transparent;");
    chassisHdrLayout->addWidget(lblHdrLeft);
    chassisHdrLayout->addStretch();
    chassisHdrLayout->addWidget(lblHdrRight);
    chassisLayout->addLayout(chassisHdrLayout);

    // Real OLED display widget inside chassis previewing device splash
    m_chassisOledWidget = new OledDisplayWidget(chassisBox);
    m_chassisOledWidget->setMinimumSize(320, 160);
    m_chassisOledWidget->setMaximumHeight(180);
    m_chassisOledWidget->updateFrame(QByteArray::fromRawData(reinterpret_cast<const char*>(GHOSTRF_SPLASH_FRAME), 1024));
    chassisLayout->addWidget(m_chassisOledWidget, 0, Qt::AlignCenter);

    auto *lblButtons = new QLabel("[ ◄ PREV ]   [ ▲ UP ]   [ ● OK ]   [ ▼ DOWN ]   [ ► NEXT ]", chassisBox);
    lblButtons->setStyleSheet("color: #555555; font-size: 10px; font-family: Monospace; border: none; background: transparent;");
    lblButtons->setAlignment(Qt::AlignCenter);
    chassisLayout->addWidget(lblButtons);

    layout->addWidget(chassisBox);

    // 3. Scanning Indicator Banner
    auto *scanBanner = new QFrame(contentWidget);
    scanBanner->setStyleSheet("background-color: #121212; border: 1px solid #282828; padding: 12px;");
    auto *scanLayout = new QVBoxLayout(scanBanner);
    scanLayout->setSpacing(6);

    m_connectScanStatus = new QLabel("[ ⟳ ПОИСК GHOSTRF В ПОРТАХ СИСТЕМЫ... ]", scanBanner);
    m_connectScanStatus->setStyleSheet("color: #ffffff; font-size: 12px; font-weight: bold; font-family: Monospace; border: none;");
    scanLayout->addWidget(m_connectScanStatus);

    m_connectScanHint = new QLabel("Подключите плату ESP32-C3 по кабелю USB Type-C к компьютеру или выберите порт вручную.", scanBanner);
    m_connectScanHint->setStyleSheet("color: #888888; font-size: 10px; font-family: Monospace; border: none;");
    scanLayout->addWidget(m_connectScanHint);

    layout->addWidget(scanBanner);

    // 4. Found Device Card
    m_cardFoundDevice = new QFrame(contentWidget);
    m_cardFoundDevice->setStyleSheet("background-color: #161616; border: 1px solid #333333; padding: 14px;");
    auto *foundLayout = new QVBoxLayout(m_cardFoundDevice);
    foundLayout->setSpacing(10);

    auto *foundHdrLayout = new QHBoxLayout();
    m_lblFoundName = new QLabel("● GHOSTRF [ESP32-C3]", m_cardFoundDevice);
    m_lblFoundName->setStyleSheet("color: #ffffff; font-size: 15px; font-weight: bold; font-family: Monospace; border: none;");
    foundHdrLayout->addWidget(m_lblFoundName);
    foundHdrLayout->addStretch();

    m_lblFoundStatus = new QLabel("USB СОПРЯЖЕНО [ОТЛИЧНЫЙ]", m_cardFoundDevice);
    m_lblFoundStatus->setStyleSheet("color: #00FF66; font-size: 11px; font-family: Monospace; border: none; font-weight: bold;");
    foundHdrLayout->addWidget(m_lblFoundStatus);
    foundLayout->addLayout(foundHdrLayout);

    m_lblFoundPort = new QLabel("ПОРТ: /dev/ttyACM0 • 115200 бод", m_cardFoundDevice);
    m_lblFoundPort->setStyleSheet("color: #aaaaaa; font-size: 11px; font-family: Monospace; border: none;");
    foundLayout->addWidget(m_lblFoundPort);

    auto *portSelectLayout = new QHBoxLayout();
    auto *lblPortText = new QLabel("ПОРТ:", m_cardFoundDevice);
    lblPortText->setStyleSheet("border: none; background: transparent; font-family: Monospace; font-size: 11px;");
    portSelectLayout->addWidget(lblPortText);
    m_connectPortCombo = new QComboBox(m_cardFoundDevice);
    m_connectPortCombo->setMinimumWidth(180);
    portSelectLayout->addWidget(m_connectPortCombo, 1);

    m_btnConnectRefreshPorts = new QPushButton("[ ОБНОВИТЬ ]", m_cardFoundDevice);
    connect(m_btnConnectRefreshPorts, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    portSelectLayout->addWidget(m_btnConnectRefreshPorts);
    foundLayout->addLayout(portSelectLayout);

    m_btnConnectFound = new QPushButton("[ ПОДКЛЮЧИТЬ УСТРОЙСТВО ]", m_cardFoundDevice);
    m_btnConnectFound->setObjectName("btnPrimary");
    m_btnConnectFound->setMinimumHeight(44);
    m_btnConnectFound->setStyleSheet("font-size: 13px; font-weight: bold; font-family: Monospace;");
    connect(m_btnConnectFound, &QPushButton::clicked, this, [this]() {
        if (m_connectPortCombo && m_connectPortCombo->count() > 0) {
            QString portName = m_connectPortCombo->currentData().toString();
            if (portName.isEmpty()) portName = m_connectPortCombo->currentText();
            if (!portName.isEmpty() && !portName.contains("НЕ НАЙДЕНЫ")) {
                if (m_step2Label) {
                    m_step2Label->setText(QString("[ ⟳ ] 2. Подключение к %1 (115200 бод)...").arg(portName));
                    m_step2Label->setStyleSheet("color: #ffffff; font-family: Monospace; font-size: 11px;");
                }
                m_dev->connectToPort(portName, 115200);
            }
        }
    });
    foundLayout->addWidget(m_btnConnectFound);

    layout->addWidget(m_cardFoundDevice);

    // 5. Step-by-step Progress Checklist
    auto *stepBox = new QFrame(contentWidget);
    stepBox->setStyleSheet("background-color: #0e0e0e; border: 1px solid #222222; padding: 14px;");
    auto *stepLayout = new QVBoxLayout(stepBox);
    stepLayout->setSpacing(8);

    auto *stepHdr = new QLabel("ЭТАПЫ СОПРЯЖЕНИЯ И ИНИЦИАЛИЗАЦИИ:", stepBox);
    stepHdr->setStyleSheet("color: #777777; font-size: 10px; font-family: Monospace; border: none; font-weight: bold;");
    stepLayout->addWidget(stepHdr);

    m_step1Label = new QLabel("[ ○ ] 1. Обнаружение аппаратного USB CDC / Serial порта", stepBox);
    m_step1Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    stepLayout->addWidget(m_step1Label);

    m_step2Label = new QLabel("[ ○ ] 2. Подключение Serial (115200 бод) и проверка связи PING", stepBox);
    m_step2Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    stepLayout->addWidget(m_step2Label);

    m_step3Label = new QLabel("[ ○ ] 3. Синхронизация сигналов NVS, параметров CC1101 и OLED дисплея", stepBox);
    m_step3Label->setStyleSheet("color: #888888; font-size: 11px; font-family: Monospace; border: none;");
    stepLayout->addWidget(m_step3Label);

    layout->addWidget(stepBox);

    // 6. Action Buttons: Retry & Offline Mode
    auto *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(10);

    m_btnConnectRetry = new QPushButton("[ ПОВТОРИТЬ ПОИСК ]", contentWidget);
    m_btnConnectRetry->setMinimumHeight(40);
    connect(m_btnConnectRetry, &QPushButton::clicked, this, [this]() {
        resetConnectionSteps();
        m_connectScanStatus->setText("[ ⟳ ПОИСК GHOSTRF В ПОРТАХ СИСТЕМЫ... ]");
        refreshSerialPorts();
        checkAutoConnect();
    });
    actionLayout->addWidget(m_btnConnectRetry);

    m_btnConnectOffline = new QPushButton("[ АВТОНОМНЫЙ РЕЖИМ ]", contentWidget);
    m_btnConnectOffline->setMinimumHeight(40);
    connect(m_btnConnectOffline, &QPushButton::clicked, this, [this]() {
        m_isOfflineMode = true;
        m_statusText->setText("АВТОНОМНЫЙ РЕЖИМ");
        m_statusLed->setText("[○]");
        m_statusLed->setStyleSheet("font-weight: bold; color: #ffaa00; font-family: Monospace;");
        m_btnConnect->setText("[ ПОДКЛЮЧИТЬ ]");
        m_telemetryBadge->setText("[ АВТОНОМНО: БЕЗ ПОДКЛЮЧЕНИЯ К ПЛАТЕ ]");
        showMainStudio();
    });
    actionLayout->addWidget(m_btnConnectOffline);

    layout->addLayout(actionLayout);

    auto *lblOfflineHint = new QLabel("Автономный режим позволяет просматривать базу сигналов, генерировать тайминги и настраивать пресеты без подключения к плате.", contentWidget);
    lblOfflineHint->setStyleSheet("color: #555555; font-size: 9px; font-family: Monospace;");
    lblOfflineHint->setAlignment(Qt::AlignCenter);
    lblOfflineHint->setWordWrap(true);
    layout->addWidget(lblOfflineHint);

    centerLayout->addWidget(contentWidget);
    scroll->setWidget(centerContainer);
    outerLayout->addWidget(scroll);

    return page;
}

QWidget* MainWindow::createTopBar() {
    auto *bar = new QWidget(this);
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);
    auto *topGhostIcon = new QLabel(bar);
    QPixmap topPix(":/assets/ghost_white.png");
    if (topPix.isNull()) {
        topPix.load("src/assets/ghost_logo_white.png");
    }
    if (!topPix.isNull()) {
        topGhostIcon->setPixmap(topPix.scaled(20, 20, Qt::KeepAspectRatio, Qt::FastTransformation));
        layout->addWidget(topGhostIcon);
    }

    layout->addWidget(new QLabel("ПОРТ:", this));
    m_portCombo = new QComboBox(this);
    m_portCombo->setMinimumWidth(160);
    layout->addWidget(m_portCombo);

    m_btnRefreshPorts = new QPushButton("[ ОБНОВИТЬ ]", this);
    m_btnRefreshPorts->setToolTip("Обновить список доступных портов");
    connect(m_btnRefreshPorts, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);
    layout->addWidget(m_btnRefreshPorts);

    layout->addWidget(new QLabel("СКОРОСТЬ:", this));
    m_baudCombo = new QComboBox(this);
    m_baudCombo->addItems({"115200", "9600", "57600", "230400", "460800", "921600"});
    m_baudCombo->setCurrentText("115200");
    layout->addWidget(m_baudCombo);

    m_btnConnect = new QPushButton("[ ПОДКЛЮЧИТЬ ]", this);
    m_btnConnect->setObjectName("btnPrimary");
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    layout->addWidget(m_btnConnect);

    layout->addSpacing(12);

    m_statusLed = new QLabel("[ ]", this);
    m_statusLed->setStyleSheet("font-weight: bold; color: #666666; font-family: Monospace;");
    layout->addWidget(m_statusLed);

    m_statusText = new QLabel("ОТКЛЮЧЕНО", this);
    m_statusText->setStyleSheet("font-weight: bold; color: #888888;");
    layout->addWidget(m_statusText);

    layout->addStretch();

    m_telemetryBadge = new QLabel("[ УСТРОЙСТВО НЕ НАЙДЕНО ]", this);
    m_telemetryBadge->setStyleSheet("color: #777777; font-family: Monospace; font-size: 11px;");
    layout->addWidget(m_telemetryBadge);

    return bar;
}

QWidget* MainWindow::createDisplayAndRemoteTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QHBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    // ЛЕВАЯ КОЛОНКА: Стриминг экрана OLED
    auto *leftCol = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftCol);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    auto *screenGroup = new QGroupBox("МОНИТОР ДИСПЛЕЯ УСТРОЙСТВА [ OLED SSD1306 128x64 ]", this);
    auto *screenLayout = new QVBoxLayout(screenGroup);

    m_oledWidget = new OledDisplayWidget(this);
    m_oledWidget->updateFrame(QByteArray::fromRawData(reinterpret_cast<const char*>(GHOSTRF_SPLASH_FRAME), 1024));
    screenLayout->addWidget(m_oledWidget, 1);

    auto *oledToolLayout = new QHBoxLayout();
    m_btnStreamToggle = new QPushButton("[ СТРИМ: ВКЛ ]", this);
    m_btnStreamToggle->setObjectName("btnPrimary");
    connect(m_btnStreamToggle, &QPushButton::clicked, this, &MainWindow::toggleScreenStream);
    oledToolLayout->addWidget(m_btnStreamToggle);

    m_btnInvertOled = new QPushButton("[ ИНВЕРСИЯ ]", this);
    connect(m_btnInvertOled, &QPushButton::clicked, this, [this]() {
        m_oledWidget->setInverted(!m_oledWidget->isInverted());
    });
    oledToolLayout->addWidget(m_btnInvertOled);

    m_btnGridOled = new QPushButton("[ СЕТКА ПИКСЕЛЕЙ ]", this);
    connect(m_btnGridOled, &QPushButton::clicked, this, [this]() {
        m_oledWidget->setPixelGrid(!m_oledWidget->hasPixelGrid());
    });
    oledToolLayout->addWidget(m_btnGridOled);

    m_btnSnapOled = new QPushButton("[ СКРИНШОТ PNG ]", this);
    connect(m_btnSnapOled, &QPushButton::clicked, this, &MainWindow::saveScreenSnapshot);
    oledToolLayout->addWidget(m_btnSnapOled);

    oledToolLayout->addStretch();
    screenLayout->addLayout(oledToolLayout);
    leftLayout->addWidget(screenGroup);
    layout->addWidget(leftCol, 3);

    // ПРАВАЯ КОЛОНКА: Аппаратный пульт управления
    auto *rightCol = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto *remoteGroup = new QGroupBox("АППАРАТНЫЙ ПУЛЬТ УПРАВЛЕНИЯ", this);
    auto *remoteLayout = new QVBoxLayout(remoteGroup);

    auto *hintLbl = new QLabel("КЛИКАЙТЕ ПО КНОПКАМ ИЛИ ИСПОЛЬЗУЙТЕ КЛАВИШИ:\n[W/UP] ВВЕРХ  |  [S/DOWN] ВНИЗ  |  [ENTER/SPACE] ОК  |  [ESC] МЕНЮ", this);
    hintLbl->setStyleSheet("color: #777777; font-size: 10px; margin-bottom: 6px;");
    remoteLayout->addWidget(hintLbl);

    auto *keyGrid = new QGridLayout();
    m_btnRemoteUp = new QPushButton("[ ▲ ВВЕРХ (W / UP) ]", this);
    m_btnRemoteUp->setMinimumHeight(44);
    connect(m_btnRemoteUp, &QPushButton::clicked, this, [this]() { sendVirtualBtn("UP"); });
    keyGrid->addWidget(m_btnRemoteUp, 0, 1);

    m_btnRemoteOk = new QPushButton("[ ● ОК / ДЕЙСТВИЕ (ENTER) ]", this);
    m_btnRemoteOk->setObjectName("btnPrimary");
    m_btnRemoteOk->setMinimumHeight(48);
    connect(m_btnRemoteOk, &QPushButton::clicked, this, [this]() { sendVirtualBtn("OK"); });
    keyGrid->addWidget(m_btnRemoteOk, 1, 1);

    m_btnRemoteDown = new QPushButton("[ ▼ ВНИЗ (S / DOWN) ]", this);
    m_btnRemoteDown->setMinimumHeight(44);
    connect(m_btnRemoteDown, &QPushButton::clicked, this, [this]() { sendVirtualBtn("DOWN"); });
    keyGrid->addWidget(m_btnRemoteDown, 2, 1);

    m_btnRemoteHold = new QPushButton("[ ⮌ ВОЗВРАТ В МЕНЮ (УДЕРЖАНИЕ ОК / ESC) ]", this);
    m_btnRemoteHold->setMinimumHeight(40);
    connect(m_btnRemoteHold, &QPushButton::clicked, this, [this]() { sendVirtualBtn("OK_HOLD"); });
    keyGrid->addWidget(m_btnRemoteHold, 3, 0, 1, 3);

    remoteLayout->addLayout(keyGrid);

    // Быстрый выбор режима (прямое переключение экранов ESP32)
    auto *navGroup = new QGroupBox("БЫСТРЫЙ ВЫБОР РЕЖИМА", this);
    auto *navGrid = new QGridLayout(navGroup);
    navGrid->setSpacing(4);

    auto *btnMnu = new QPushButton("[ 1. ГЛАВНОЕ МЕНЮ ]", this);
    connect(btnMnu, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("MAIN"); });
    navGrid->addWidget(btnMnu, 0, 0);

    auto *btnRx = new QPushButton("[ 2. ПРИЕМНИК RX ]", this);
    connect(btnRx, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("RX"); });
    navGrid->addWidget(btnRx, 0, 1);

    auto *btnRaw = new QPushButton("[ 3. RAW СНИФФЕР ]", this);
    connect(btnRaw, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("RAW"); });
    navGrid->addWidget(btnRaw, 1, 0);

    auto *btnSpec = new QPushButton("[ 4. СПЕКТР 128x64 ]", this);
    connect(btnSpec, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("SPECTRUM"); });
    navGrid->addWidget(btnSpec, 1, 1);

    auto *btnFreq = new QPushButton("[ 5. ЧАСТОТОМЕР ]", this);
    connect(btnFreq, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("FREQ"); });
    navGrid->addWidget(btnFreq, 2, 0);

    auto *btnTx = new QPushButton("[ 6. ПУЛЬТ TX ]", this);
    connect(btnTx, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("TX"); });
    navGrid->addWidget(btnTx, 2, 1);

    auto *btnGen = new QPushButton("[ 7. ГЕНЕРАТОР / ГЛУШ. ]", this);
    connect(btnGen, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("CARRIER"); });
    navGrid->addWidget(btnGen, 3, 0);

    auto *btnSet = new QPushButton("[ 8. НАСТРОЙКИ ]", this);
    connect(btnSet, &QPushButton::clicked, this, [this]() { m_dev->setDeviceMode("SETTINGS"); });
    navGrid->addWidget(btnSet, 3, 1);

    remoteLayout->addWidget(navGroup);

    // Настройки аппаратуры
    auto *cfgGroup = new QGroupBox("ПАРАМЕТРЫ РАДИОМОДУЛЯ CC1101", this);
    auto *cfgForm = new QFormLayout(cfgGroup);

    m_cfgFreqCombo = new QComboBox(this);
    m_cfgFreqCombo->addItems({"433.92 МГц", "433.07 МГц", "433.42 МГц", "434.42 МГц", "434.77 МГц", "315.00 МГц", "868.35 МГц"});
    cfgForm->addRow("ЧАСТОТА:", m_cfgFreqCombo);

    m_cfgPaCombo = new QComboBox(this);
    m_cfgPaCombo->addItems({"-30 dBm", "-10 dBm", "0 dBm", "+10 dBm"});
    m_cfgPaCombo->setCurrentIndex(3);
    cfgForm->addRow("МОЩНОСТЬ PA:", m_cfgPaCombo);

    m_cfgSquelchCombo = new QComboBox(this);
    m_cfgSquelchCombo->addItems({"-85 dBm (MAX)", "-78 dBm (MID)", "-72 dBm (BASE)", "-65 dBm (LOW)"});
    m_cfgSquelchCombo->setCurrentIndex(1);
    cfgForm->addRow("SQUELCH:", m_cfgSquelchCombo);

    auto *btnSaveCfg = new QPushButton("[ СОХРАНИТЬ В NVS ]", this);
    connect(btnSaveCfg, &QPushButton::clicked, this, &MainWindow::saveHardwareConfig);
    cfgForm->addRow(btnSaveCfg);

    auto *btnReboot = new QPushButton("[ ПЕРЕЗАГРУЗИТЬ ESP32 ]", this);
    btnReboot->setObjectName("btnDanger");
    connect(btnReboot, &QPushButton::clicked, this, &MainWindow::rebootEspDevice);
    cfgForm->addRow(btnReboot);

    remoteLayout->addWidget(cfgGroup);
    remoteLayout->addStretch();

    rightLayout->addWidget(remoteGroup);
    layout->addWidget(rightCol, 2);

    return tab;
}

QWidget* MainWindow::createSavedSignalsTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);

    // Панель действий со списком
    auto *toolBar = new QHBoxLayout();
    auto *btnReload = new QPushButton("[ ОБНОВИТЬ С УСТРОЙСТВА ]", this);
    connect(btnReload, &QPushButton::clicked, this, &MainWindow::refreshSignalsFromDevice);
    toolBar->addWidget(btnReload);

    auto *btnAdd = new QPushButton("[ ДОБАВИТЬ ]", this);
    connect(btnAdd, &QPushButton::clicked, this, &MainWindow::addNewSignal);
    toolBar->addWidget(btnAdd);

    auto *btnEdit = new QPushButton("[ РЕДАКТИРОВАТЬ ]", this);
    connect(btnEdit, &QPushButton::clicked, this, &MainWindow::editCurrentSignal);
    toolBar->addWidget(btnEdit);

    auto *btnDel = new QPushButton("[ УДАЛИТЬ ]", this);
    btnDel->setObjectName("btnDanger");
    connect(btnDel, &QPushButton::clicked, this, &MainWindow::deleteCurrentSignal);
    toolBar->addWidget(btnDel);

    auto *btnClear = new QPushButton("[ ОЧИСТИТЬ ВСЕ ]", this);
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearAllSignals);
    toolBar->addWidget(btnClear);

    toolBar->addStretch();

    auto *btnExportJson = new QPushButton("[ БЭКАП JSON ]", this);
    connect(btnExportJson, &QPushButton::clicked, this, &MainWindow::exportBackupJson);
    toolBar->addWidget(btnExportJson);

    auto *btnImportJson = new QPushButton("[ ВОССТАНОВИТЬ ]", this);
    connect(btnImportJson, &QPushButton::clicked, this, &MainWindow::restoreBackupJson);
    toolBar->addWidget(btnImportJson);

    auto *btnExportSub = new QPushButton("[ ЭКСПОРТ .SUB ]", this);
    connect(btnExportSub, &QPushButton::clicked, this, &MainWindow::exportCurrentToSubGhz);
    toolBar->addWidget(btnExportSub);

    auto *btnImportSub = new QPushButton("[ ИМПОРТ .SUB ]", this);
    connect(btnImportSub, &QPushButton::clicked, this, &MainWindow::importFromSubGhz);
    toolBar->addWidget(btnImportSub);

    layout->addLayout(toolBar);

    // Сплиттер: Таблица слева, Детальный инспектор справа
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    m_tableSignals = new QTableWidget(this);
    m_tableSignals->setColumnCount(9);
    m_tableSignals->setHorizontalHeaderLabels({
        "№", "ИМЯ СИГНАЛА", "ТИП", "ЧАСТОТА", "ПРОТОКОЛ", "КОД (HEX)", "БИТ", "TE (µs)", "ПОВТОРЫ"
    });
    m_tableSignals->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableSignals->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tableSignals->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableSignals->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableSignals->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_tableSignals, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableSelectionChanged);
    splitter->addWidget(m_tableSignals);

    // Правая часть: Инспектор выбранного сигнала
    auto *inspectorWidget = new QWidget(this);
    auto *inspLayout = new QVBoxLayout(inspectorWidget);
    inspLayout->setContentsMargins(8, 0, 8, 0);

    m_inspectorTitle = new QLabel("[ ВЫБЕРИТЕ СИГНАЛ ИЗ ТАБЛИЦЫ ]", this);
    m_inspectorTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #ffffff;");
    inspLayout->addWidget(m_inspectorTitle);

    auto *paramGroup = new QGroupBox("ПАРАМЕТРЫ СИГНАЛА", this);
    auto *paramForm = new QFormLayout(paramGroup);
    m_lblFreqVal = new QLabel("—", this);
    m_lblProtoVal = new QLabel("—", this);
    m_lblCodeHexVal = new QLabel("—", this);
    m_lblCodeDecVal = new QLabel("—", this);
    m_lblCodeBinVal = new QLabel("—", this);
    m_lblCodeBinVal->setStyleSheet("font-family: Monospace; color: #ffffff;");
    m_lblTimingVal = new QLabel("—", this);

    paramForm->addRow("ЧАСТОТА:", m_lblFreqVal);
    paramForm->addRow("ПРОТОКОЛ:", m_lblProtoVal);
    paramForm->addRow("КОД (HEX):", m_lblCodeHexVal);
    paramForm->addRow("КОД (DEC):", m_lblCodeDecVal);
    paramForm->addRow("КОД (BIN):", m_lblCodeBinVal);
    paramForm->addRow("ТАЙМИНГИ:", m_lblTimingVal);
    inspLayout->addWidget(paramGroup);

    m_klqGroup = new QGroupBox("ДАННЫЕ KEELOQ", this);
    auto *klqForm = new QFormLayout(m_klqGroup);
    m_lblKlqSerial = new QLabel("—", this);
    m_lblKlqBtn = new QLabel("—", this);
    m_lblKlqEnc = new QLabel("—", this);
    klqForm->addRow("СЕРИЙНЫЙ НОМЕР:", m_lblKlqSerial);
    klqForm->addRow("КНОПКА ПУЛЬТА:", m_lblKlqBtn);
    klqForm->addRow("ШИФРОСЧЕТЧИК:", m_lblKlqEnc);
    m_klqGroup->setVisible(false);
    inspLayout->addWidget(m_klqGroup);

    auto *oscGroup = new QGroupBox("ОСЦИЛЛОГРАММА СИГНАЛА", this);
    auto *oscLayout = new QVBoxLayout(oscGroup);
    m_inspectorWaveform = new WaveformWidget(this);
    m_inspectorWaveform->setMinimumHeight(150);
    oscLayout->addWidget(m_inspectorWaveform);

    auto *oscCtrlLayout = new QHBoxLayout();
    auto *btnZoomIn = new QPushButton("[ + ]", this);
    btnZoomIn->setMaximumWidth(40);
    connect(btnZoomIn, &QPushButton::clicked, m_inspectorWaveform, &WaveformWidget::zoomIn);
    oscCtrlLayout->addWidget(btnZoomIn);

    auto *btnZoomOut = new QPushButton("[ - ]", this);
    btnZoomOut->setMaximumWidth(40);
    connect(btnZoomOut, &QPushButton::clicked, m_inspectorWaveform, &WaveformWidget::zoomOut);
    oscCtrlLayout->addWidget(btnZoomOut);

    auto *btnResetZoom = new QPushButton("[ СБРОС ]", this);
    connect(btnResetZoom, &QPushButton::clicked, m_inspectorWaveform, &WaveformWidget::resetZoom);
    oscCtrlLayout->addWidget(btnResetZoom);

    auto *btnInvert = new QPushButton("[ ИНВЕРСИЯ ]", this);
    connect(btnInvert, &QPushButton::clicked, this, [this]() {
        m_inspectorWaveform->setInverted(!m_inspectorWaveform->isInverted());
    });
    oscCtrlLayout->addWidget(btnInvert);

    oscCtrlLayout->addStretch();
    oscLayout->addLayout(oscCtrlLayout);
    inspLayout->addWidget(oscGroup);

    auto *txPanel = new QGroupBox("УПРАВЛЕНИЕ ПЕРЕДАЧЕЙ (TX)", this);
    auto *txLayout = new QHBoxLayout(txPanel);

    txLayout->addWidget(new QLabel("ПОВТОРЫ:"));
    m_txRepeatsSpin = new QSpinBox(this);
    m_txRepeatsSpin->setRange(1, 20);
    m_txRepeatsSpin->setValue(5);
    txLayout->addWidget(m_txRepeatsSpin);

    txLayout->addWidget(new QLabel("МОЩНОСТЬ:"));
    m_txPowerCombo = new QComboBox(this);
    m_txPowerCombo->addItems({"+10 dBm", "0 dBm", "-10 dBm", "-30 dBm"});
    m_txPowerCombo->setCurrentIndex(0);
    txLayout->addWidget(m_txPowerCombo);

    m_btnTransmitSaved = new QPushButton("[ ПЕРЕДАТЬ В ЭФИР ]", this);
    m_btnTransmitSaved->setObjectName("btnPrimary");
    m_btnTransmitSaved->setMinimumHeight(34);
    connect(m_btnTransmitSaved, &QPushButton::clicked, this, &MainWindow::transmitCurrentSignal);
    txLayout->addWidget(m_btnTransmitSaved);

    inspLayout->addWidget(txPanel);
    splitter->addWidget(inspectorWidget);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter);
    return tab;
}

QWidget* MainWindow::createTransmitterTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *subTabs = new QTabWidget(this);

    // ФИКСИРОВАННЫЙ КОД
    auto *fixedTab = new QWidget(this);
    auto *fixedLayout = new QVBoxLayout(fixedTab);

    auto *formGroup = new QGroupBox("ПАРАМЕТРЫ ФИКСИРОВАННОГО КОДА", this);
    auto *form = new QFormLayout(formGroup);

    m_genNameEdit = new QLineEdit("Signal Custom", this);
    form->addRow("ИМЯ СИГНАЛА:", m_genNameEdit);

    m_genFreqSpin = new QDoubleSpinBox(this);
    m_genFreqSpin->setRange(300.0, 928.0);
    m_genFreqSpin->setDecimals(3);
    m_genFreqSpin->setValue(433.92);
    m_genFreqSpin->setSuffix(" МГц");
    form->addRow("ЧАСТОТА:", m_genFreqSpin);

    m_genProtoCombo = new QComboBox(this);
    m_genProtoCombo->addItems({"EV1527", "PT2262", "Came 12bit", "Nice 12bit", "RC-Switch (Proto 1)"});
    connect(m_genProtoCombo, &QComboBox::currentTextChanged, this, &MainWindow::updateGenWaveform);
    form->addRow("ПРОТОКОЛ:", m_genProtoCombo);

    auto *codeLayout = new QHBoxLayout();
    m_genHexEdit = new QLineEdit("0x547A11", this);
    connect(m_genHexEdit, &QLineEdit::textChanged, this, &MainWindow::onGenHexChanged);

    m_genDecSpin = new QSpinBox(this);
    m_genDecSpin->setRange(0, 0x7FFFFFFF);
    m_genDecSpin->setValue(0x547A11);
    connect(m_genDecSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onGenDecChanged);

    codeLayout->addWidget(new QLabel("HEX:"));
    codeLayout->addWidget(m_genHexEdit);
    codeLayout->addWidget(new QLabel("DEC:"));
    codeLayout->addWidget(m_genDecSpin);
    form->addRow("КОД ПЕРЕДАЧИ:", codeLayout);

    auto *timingLayout = new QHBoxLayout();
    m_genBitLenSpin = new QSpinBox(this);
    m_genBitLenSpin->setRange(1, 64);
    m_genBitLenSpin->setValue(24);
    connect(m_genBitLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateGenWaveform);

    m_genPulseLenSpin = new QSpinBox(this);
    m_genPulseLenSpin->setRange(50, 5000);
    m_genPulseLenSpin->setSuffix(" µs");
    m_genPulseLenSpin->setValue(320);
    connect(m_genPulseLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::updateGenWaveform);

    m_genRepeatsSpin = new QSpinBox(this);
    m_genRepeatsSpin->setRange(1, 30);
    m_genRepeatsSpin->setValue(7);

    m_genPowerCombo = new QComboBox(this);
    m_genPowerCombo->addItems({"+10 dBm", "0 dBm", "-10 dBm", "-30 dBm"});

    timingLayout->addWidget(new QLabel("БИТ:"));
    timingLayout->addWidget(m_genBitLenSpin);
    timingLayout->addWidget(new QLabel("TE (µs):"));
    timingLayout->addWidget(m_genPulseLenSpin);
    timingLayout->addWidget(new QLabel("ПОВТОРЫ:"));
    timingLayout->addWidget(m_genRepeatsSpin);
    timingLayout->addWidget(new QLabel("МОЩНОСТЬ:"));
    timingLayout->addWidget(m_genPowerCombo);
    form->addRow("ТАЙМИНГИ:", timingLayout);
    fixedLayout->addWidget(formGroup);

    auto *oscGroup = new QGroupBox("ОСЦИЛЛОГРАММА СИНТЕЗА", this);
    auto *oscLayout = new QVBoxLayout(oscGroup);
    m_genWaveform = new WaveformWidget(this);
    m_genWaveform->setMinimumHeight(120);
    oscLayout->addWidget(m_genWaveform);
    fixedLayout->addWidget(oscGroup);

    auto *btnFixedLayout = new QHBoxLayout();
    auto *btnTxFixed = new QPushButton("[ ПЕРЕДАТЬ ФИКСИРОВАННЫЙ КОД В ЭФИР ]", this);
    btnTxFixed->setObjectName("btnPrimary");
    btnTxFixed->setMinimumHeight(38);
    connect(btnTxFixed, &QPushButton::clicked, this, &MainWindow::transmitCustomFixed);
    btnFixedLayout->addWidget(btnTxFixed);

    auto *btnSaveFixed = new QPushButton("[ СОХРАНИТЬ В NVS УСТРОЙСТВА ]", this);
    btnSaveFixed->setMinimumHeight(38);
    connect(btnSaveFixed, &QPushButton::clicked, this, &MainWindow::saveCustomFixed);
    btnFixedLayout->addWidget(btnSaveFixed);

    fixedLayout->addLayout(btnFixedLayout);
    subTabs->addTab(fixedTab, "[ ФИКСИРОВАННЫЙ КОД ]");

    // RAW ИМПУЛЬСЫ
    auto *rawTab = new QWidget(this);
    auto *rawLayout = new QVBoxLayout(rawTab);

    auto *rawHeader = new QHBoxLayout();
    m_rawGenNameEdit = new QLineEdit("Signal RAW", this);
    m_rawGenFreqSpin = new QDoubleSpinBox(this);
    m_rawGenFreqSpin->setRange(300.0, 928.0);
    m_rawGenFreqSpin->setDecimals(3);
    m_rawGenFreqSpin->setValue(433.92);
    m_rawGenFreqSpin->setSuffix(" МГц");

    m_rawGenPowerCombo = new QComboBox(this);
    m_rawGenPowerCombo->addItems({"+10 dBm", "0 dBm", "-10 dBm", "-30 dBm"});

    m_rawGenRepeatsSpin = new QSpinBox(this);
    m_rawGenRepeatsSpin->setRange(1, 20);
    m_rawGenRepeatsSpin->setValue(5);

    rawHeader->addWidget(new QLabel("ИМЯ:"));
    rawHeader->addWidget(m_rawGenNameEdit);
    rawHeader->addWidget(new QLabel("ЧАСТОТА:"));
    rawHeader->addWidget(m_rawGenFreqSpin);
    rawHeader->addWidget(new QLabel("МОЩНОСТЬ:"));
    rawHeader->addWidget(m_rawGenPowerCombo);
    rawHeader->addWidget(new QLabel("ПОВТОРЫ:"));
    rawHeader->addWidget(m_rawGenRepeatsSpin);
    rawLayout->addLayout(rawHeader);

    auto *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("ПРЕСЕТЫ:"));
    auto *btnPreset1 = new QPushButton("[ CAME 12-BIT ]", this);
    connect(btnPreset1, &QPushButton::clicked, this, [this]() { loadRawPreset(0); });
    presetLayout->addWidget(btnPreset1);

    auto *btnPreset2 = new QPushButton("[ NICE FLO ]", this);
    connect(btnPreset2, &QPushButton::clicked, this, [this]() { loadRawPreset(1); });
    presetLayout->addWidget(btnPreset2);

    auto *btnPreset3 = new QPushButton("[ ЗВОНОК EV1527 ]", this);
    connect(btnPreset3, &QPushButton::clicked, this, [this]() { loadRawPreset(2); });
    presetLayout->addWidget(btnPreset3);

    presetLayout->addStretch();
    rawLayout->addLayout(presetLayout);

    m_rawGenText = new QPlainTextEdit(this);
    m_rawGenText->setPlaceholderText("Длительности импульсов в микросекундах (чередование HIGH / LOW через запятую или пробел, либо RAW_Data Sub-GHz)...");
    connect(m_rawGenText, &QPlainTextEdit::textChanged, this, &MainWindow::onRawGenTextChanged);
    rawLayout->addWidget(m_rawGenText);

    auto *rawOscGroup = new QGroupBox("ФОРМА RAW СИГНАЛА", this);
    auto *rawOscLayout = new QVBoxLayout(rawOscGroup);
    m_rawGenWaveform = new WaveformWidget(this);
    m_rawGenWaveform->setMinimumHeight(120);
    rawOscLayout->addWidget(m_rawGenWaveform);
    rawLayout->addWidget(rawOscGroup);

    auto *btnRawLayout = new QHBoxLayout();
    auto *btnTxRaw = new QPushButton("[ ПЕРЕДАТЬ RAW В ЭФИР ]", this);
    btnTxRaw->setObjectName("btnPrimary");
    btnTxRaw->setMinimumHeight(38);
    connect(btnTxRaw, &QPushButton::clicked, this, &MainWindow::transmitCustomRaw);
    btnRawLayout->addWidget(btnTxRaw);

    auto *btnSaveRaw = new QPushButton("[ СОХРАНИТЬ RAW В NVS ]", this);
    btnSaveRaw->setMinimumHeight(38);
    connect(btnSaveRaw, &QPushButton::clicked, this, &MainWindow::saveCustomRaw);
    btnRawLayout->addWidget(btnSaveRaw);

    rawLayout->addLayout(btnRawLayout);
    subTabs->addTab(rawTab, "[ RAW ИМПУЛЬСЫ ]");

    // НЕСУЩАЯ / МАЯК
    auto *carrierTab = new QWidget(this);
    auto *carrierLayout = new QVBoxLayout(carrierTab);

    auto *carrierGroup = new QGroupBox("ГЕНЕРАТОР НЕСУЩЕЙ / МАЯК", this);
    auto *cForm = new QFormLayout(carrierGroup);

    m_carrierModeCombo = new QComboBox(this);
    m_carrierModeCombo->addItem("Немодулированная несущая (CW)", 0);
    m_carrierModeCombo->addItem("Тональный сигнал 1 кГц", 1);
    m_carrierModeCombo->addItem("Шумовой сигнал (Джеммер)", 2);
    cForm->addRow("РЕЖИМ:", m_carrierModeCombo);

    m_carrierFreqSpin = new QDoubleSpinBox(this);
    m_carrierFreqSpin->setRange(300.0, 928.0);
    m_carrierFreqSpin->setDecimals(3);
    m_carrierFreqSpin->setValue(433.92);
    m_carrierFreqSpin->setSuffix(" МГц");
    cForm->addRow("ЧАСТОТА:", m_carrierFreqSpin);

    m_carrierPowerCombo = new QComboBox(this);
    m_carrierPowerCombo->addItems({"+10 dBm", "0 dBm", "-10 dBm", "-30 dBm"});
    cForm->addRow("МОЩНОСТЬ:", m_carrierPowerCombo);

    carrierLayout->addWidget(carrierGroup);

    m_btnCarrierToggle = new QPushButton("[ ВКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]", this);
    m_btnCarrierToggle->setObjectName("btnDanger");
    m_btnCarrierToggle->setMinimumHeight(44);
    connect(m_btnCarrierToggle, &QPushButton::clicked, this, &MainWindow::toggleCarrierMode);
    carrierLayout->addWidget(m_btnCarrierToggle);

    carrierLayout->addStretch();
    subTabs->addTab(carrierTab, "[ НЕСУЩАЯ / МАЯК ]");

    layout->addWidget(subTabs);
    updateGenWaveform();
    return tab;
}

QWidget* MainWindow::createSnifferTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // Панель параметров радиомодуля и мощности сигнала прямо в сниффере
    auto *sniffCfgGroup = new QGroupBox("ПАРАМЕТРЫ РАДИОМОДУЛЯ И МОЩНОСТЬ СИГНАЛА", this);
    auto *sniffCfgLayout = new QHBoxLayout(sniffCfgGroup);
    sniffCfgLayout->setContentsMargins(8, 6, 8, 6);
    sniffCfgLayout->setSpacing(8);

    sniffCfgLayout->addWidget(new QLabel("ЧАСТОТА:", this));
    m_sniffFreqCombo = new QComboBox(this);
    m_sniffFreqCombo->addItems({"433.92 МГц", "433.07 МГц", "433.42 МГц", "434.42 МГц", "434.77 МГц", "315.00 МГц", "868.35 МГц"});
    sniffCfgLayout->addWidget(m_sniffFreqCombo);

    sniffCfgLayout->addWidget(new QLabel("МОЩНОСТЬ PA:", this));
    m_sniffPaCombo = new QComboBox(this);
    m_sniffPaCombo->addItems({"-30 dBm", "-10 dBm", "0 dBm", "+10 dBm"});
    m_sniffPaCombo->setCurrentIndex(3);
    sniffCfgLayout->addWidget(m_sniffPaCombo);

    sniffCfgLayout->addWidget(new QLabel("SQUELCH:", this));
    m_sniffSquelchCombo = new QComboBox(this);
    m_sniffSquelchCombo->addItems({"-85 dBm (MAX ЧУВСТВ.)", "-78 dBm (MID)", "-72 dBm (BASE)", "-65 dBm (LOW ПОМЕХИ)"});
    m_sniffSquelchCombo->setCurrentIndex(1);
    sniffCfgLayout->addWidget(m_sniffSquelchCombo);

    m_btnApplySniffConfig = new QPushButton("[ ПРИМЕНИТЬ НАСТРОЙКИ РАДИО ]", this);
    m_btnApplySniffConfig->setObjectName("btnPrimary");
    connect(m_btnApplySniffConfig, &QPushButton::clicked, this, &MainWindow::applySnifferConfig);
    sniffCfgLayout->addWidget(m_btnApplySniffConfig);

    layout->addWidget(sniffCfgGroup);

    auto *sniffBtnLayout = new QHBoxLayout();
    m_btnSnifferFixed = new QPushButton("[ СТАРТ RX СНИФФЕРА (ФИКСИРОВАННЫЕ) ]", this);
    m_btnSnifferFixed->setObjectName("btnSuccess");
    connect(m_btnSnifferFixed, &QPushButton::clicked, this, &MainWindow::startSnifferFixed);
    sniffBtnLayout->addWidget(m_btnSnifferFixed);

    m_btnSnifferRaw = new QPushButton("[ СТАРТ RAW СНИФФЕРА (KEELOQ/RAW) ]", this);
    m_btnSnifferRaw->setObjectName("btnPrimary");
    connect(m_btnSnifferRaw, &QPushButton::clicked, this, &MainWindow::startSnifferRaw);
    sniffBtnLayout->addWidget(m_btnSnifferRaw);

    m_btnSnifferStop = new QPushButton("[ СТОП ]", this);
    m_btnSnifferStop->setObjectName("btnDanger");
    connect(m_btnSnifferStop, &QPushButton::clicked, this, &MainWindow::stopSnifferMode);
    sniffBtnLayout->addWidget(m_btnSnifferStop);

    layout->addLayout(sniffBtnLayout);

    m_tableSniffer = new QTableWidget(this);
    m_tableSniffer->setColumnCount(7);
    m_tableSniffer->setHorizontalHeaderLabels({
        "ВРЕМЯ", "ЧАСТОТА", "ПРОТОКОЛ", "КОД (HEX)", "БИТ", "TE / ИМПУЛЬСЫ", "RSSI (dBm)"
    });
    m_tableSniffer->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableSniffer->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tableSniffer->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableSniffer->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_tableSniffer);

    auto *sniffActionLayout = new QHBoxLayout();
    auto *btnReplay = new QPushButton("[ ПОВТОРИТЬ ПЕРЕДАЧУ (REPLAY TX) ]", this);
    btnReplay->setObjectName("btnPrimary");
    connect(btnReplay, &QPushButton::clicked, this, &MainWindow::replayCapturedPacket);
    sniffActionLayout->addWidget(btnReplay);

    auto *btnSaveCaptured = new QPushButton("[ СОХРАНИТЬ В NVS ]", this);
    connect(btnSaveCaptured, &QPushButton::clicked, this, &MainWindow::saveCapturedPacket);
    sniffActionLayout->addWidget(btnSaveCaptured);

    auto *btnClearSniffer = new QPushButton("[ ОЧИСТИТЬ СПИСОК ]", this);
    connect(btnClearSniffer, &QPushButton::clicked, this, [this]() {
        m_tableSniffer->setRowCount(0);
        m_snifferHistory.clear();
    });
    sniffActionLayout->addWidget(btnClearSniffer);

    layout->addLayout(sniffActionLayout);
    return tab;
}

QWidget* MainWindow::createConsoleTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(6, 6, 6, 6);

    auto *toolLayout = new QHBoxLayout();
    auto *btnClear = new QPushButton("[ ОЧИСТИТЬ КОНСОЛЬ ]", this);
    connect(btnClear, &QPushButton::clicked, this, [this]() { m_consoleLog->clear(); });
    toolLayout->addWidget(btnClear);

    auto *btnPing = new QPushButton("[ PING ]", this);
    connect(btnPing, &QPushButton::clicked, this, [this]() { m_dev->sendPing(); });
    toolLayout->addWidget(btnPing);

    toolLayout->addStretch();
    layout->addLayout(toolLayout);

    m_consoleLog = new QPlainTextEdit(this);
    m_consoleLog->setReadOnly(true);
    layout->addWidget(m_consoleLog);

    auto *inputLayout = new QHBoxLayout();
    m_consoleInput = new QLineEdit(this);
    m_consoleInput->setPlaceholderText("Команда (PING, GET_SIGNALS, TX_SAVED 0, GET_SCREEN, GET_CONFIG)...");
    connect(m_consoleInput, &QLineEdit::returnPressed, this, &MainWindow::sendConsoleCommand);
    inputLayout->addWidget(m_consoleInput);

    auto *btnSend = new QPushButton("[ ОТПРАВИТЬ ]", this);
    btnSend->setObjectName("btnPrimary");
    connect(btnSend, &QPushButton::clicked, this, &MainWindow::sendConsoleCommand);
    inputLayout->addWidget(btnSend);

    layout->addLayout(inputLayout);
    return tab;
}

// -----------------------------------------------------------------
// СЛОТЫ ПОДКЛЮЧЕНИЯ И СТРИМИНГА
// -----------------------------------------------------------------
void MainWindow::refreshSerialPorts() {
    m_portCombo->clear();
    if (m_connectPortCombo) m_connectPortCombo->clear();

    const auto ports = DeviceManager::availablePorts();
    bool foundHub = false;
    QSerialPortInfo hubPort;

    for (const auto &p : ports) {
        QString desc = p.description().isEmpty() ? p.portName() : QString("%1 (%2)").arg(p.portName(), p.description());
        m_portCombo->addItem(desc, p.portName());
        if (m_connectPortCombo) m_connectPortCombo->addItem(desc, p.portName());

        QString name = p.portName();
        QString d = p.description();
        if (name.contains("ttyACM", Qt::CaseInsensitive) || 
            name.contains("ttyUSB", Qt::CaseInsensitive) ||
            d.contains("Espressif", Qt::CaseInsensitive) ||
            d.contains("USB JTAG", Qt::CaseInsensitive) ||
            d.contains("CP210", Qt::CaseInsensitive) ||
            d.contains("CH340", Qt::CaseInsensitive)) {
            foundHub = true;
            hubPort = p;
        }
    }

    if (m_portCombo->count() == 0) {
        m_portCombo->addItem("[ ПОРТЫ НЕ НАЙДЕНЫ ]", "");
        if (m_connectPortCombo) m_connectPortCombo->addItem("[ ПОРТЫ НЕ НАЙДЕНЫ ]", "");
    }

    if (foundHub && m_cardFoundDevice) {
        m_cardFoundDevice->setVisible(true);
        if (m_lblFoundName) m_lblFoundName->setText("● GHOSTRF [ESP32-C3]");
        if (m_lblFoundPort) m_lblFoundPort->setText(QString("ПОРТ: %1 (%2) • 115200 бод").arg(hubPort.portName(), hubPort.description()));
        if (m_step1Label) {
            m_step1Label->setText(QString("[ ✔ ] 1. USB порт %1 обнаружен (%2)").arg(hubPort.portName(), hubPort.description()));
            m_step1Label->setStyleSheet("color: #00FF66; font-size: 11px; font-family: Monospace; border: none;");
        }
        if (m_connectScanStatus) {
            m_connectScanStatus->setText(QString("[ НАЙДЕНО: %1. СОПРЯЖЕНИЕ... ]").arg(hubPort.portName()));
        }
        for (int i = 0; i < m_connectPortCombo->count(); i++) {
            if (m_connectPortCombo->itemData(i).toString() == hubPort.portName() || m_connectPortCombo->itemText(i).contains(hubPort.portName())) {
                m_connectPortCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MainWindow::toggleConnection() {
    if (m_dev->isConnected()) {
        m_dev->stopScreenStream();
        m_dev->disconnectPort();
        showConnectScreen("[ ОТКЛЮЧЕНО ПОЛЬЗОВАТЕЛЕМ ]");
        resetConnectionSteps();
        refreshSerialPorts();
    } else {
        showConnectScreen("[ ПОИСК GHOSTRF В ПОРТАХ СИСТЕМЫ... ]");
        resetConnectionSteps();
        refreshSerialPorts();
        checkAutoConnect();
    }
}

void MainWindow::checkAutoConnect() {
    if (m_dev->isConnected()) {
        return;
    }
    const auto ports = DeviceManager::availablePorts();
    for (const auto &p : ports) {
        QString name = p.portName();
        QString desc = p.description();
        if (name.contains("ttyACM", Qt::CaseInsensitive) || 
            name.contains("ttyUSB", Qt::CaseInsensitive) ||
            desc.contains("Espressif", Qt::CaseInsensitive) ||
            desc.contains("USB JTAG", Qt::CaseInsensitive) ||
            desc.contains("CP210", Qt::CaseInsensitive) ||
            desc.contains("CH340", Qt::CaseInsensitive)) {
            
            refreshSerialPorts();
            for (int i = 0; i < m_portCombo->count(); i++) {
                if (m_portCombo->itemData(i).toString() == name || m_portCombo->itemText(i).contains(name)) {
                    m_portCombo->setCurrentIndex(i);
                    if (m_step2Label) {
                        m_step2Label->setText(QString("[ ⟳ ] 2. Подключение к %1 (115200 бод)...").arg(name));
                        m_step2Label->setStyleSheet("color: #ffffff; font-family: Monospace; font-size: 11px;");
                    }
                    m_dev->connectToPort(name, 115200);
                    return;
                }
            }
        }
    }
}

void MainWindow::onConnectionStateChanged(bool connected, const QString &portName) {
    if (connected) {
        m_isOfflineMode = false;
        m_btnConnect->setText("[ СМЕНИТЬ ]");
        m_btnConnect->setObjectName("btnDanger");
        m_btnConnect->style()->unpolish(m_btnConnect);
        m_btnConnect->style()->polish(m_btnConnect);
        m_statusLed->setText("[■]");
        m_statusLed->setStyleSheet("font-weight: bold; color: #00FF66; font-family: Monospace;");
        m_statusText->setText(QString("ПОДКЛЮЧЕНО: %1").arg(portName));
        m_statusText->setStyleSheet("font-weight: bold; color: #00FF66; font-family: Monospace;");
        m_dev->requestSignals();
        if (m_screenStreamActive) {
            m_dev->startScreenStream();
        }

        if (m_step2Label) {
            m_step2Label->setText(QString("[ ✔ ] 2. Serial порт %1 подключен [OK]").arg(portName));
            m_step2Label->setStyleSheet("color: #00FF66; font-size: 11px; font-family: Monospace; border: none;");
        }
        if (m_step3Label) {
            m_step3Label->setText("[ ⟳ ] 3. Синхронизация сигналов NVS и стриминга экрана...");
            m_step3Label->setStyleSheet("color: #ffffff; font-size: 11px; font-family: Monospace; border: none;");
        }
    } else {
        m_btnConnect->setText("[ ПОДКЛЮЧИТЬ ]");
        m_btnConnect->setObjectName("btnPrimary");
        m_btnConnect->style()->unpolish(m_btnConnect);
        m_btnConnect->style()->polish(m_btnConnect);
        m_statusLed->setText("[ ]");
        m_statusLed->setStyleSheet("font-weight: bold; color: #666666; font-family: Monospace;");
        m_statusText->setText("ОТКЛЮЧЕНО");
        m_statusText->setStyleSheet("font-weight: bold; color: #888888; font-family: Monospace;");
        m_telemetryBadge->setText("[ УСТРОЙСТВО НЕ НАЙДЕНО ]");
        m_oledWidget->clearDisplay();

        if (m_rootStack && m_rootStack->currentWidget() == m_mainStudio && !m_isOfflineMode) {
            showConnectScreen("[ СВЯЗЬ ПОТЕРЯНА. ПОИСК УСТРОЙСТВА... ]");
            resetConnectionSteps();
            refreshSerialPorts();
        }
    }
}

void MainWindow::onPongReceived(const DeviceInfo &info) {
    QString tel = QString("[%1 v%2 | %3] CC1101: %4 | HEAP: %5KB | СИГНАЛОВ: %6/%7")
                      .arg(info.deviceName, info.version, info.chip)
                      .arg(info.cc1101Ok ? "OK" : "ERR")
                      .arg(info.freeHeap / 1024)
                      .arg(info.signalsCount)
                      .arg(info.maxSignals);
    m_telemetryBadge->setText(tel);

    if (m_step3Label) {
        m_step3Label->setText(QString("[ ✔ ] 3. Синхронизация завершена! Память: %1/%2 | OLED активен").arg(info.signalsCount).arg(info.maxSignals));
        m_step3Label->setStyleSheet("color: #00FF66; font-size: 11px; font-family: Monospace; border: none;");
    }
    if (m_connectScanStatus) {
        m_connectScanStatus->setText("[ УСТРОЙСТВО ГОТОВО К РАБОТЕ ]");
    }
    if (m_rootStack && m_rootStack->currentWidget() == m_connectScreen) {
        QTimer::singleShot(350, this, [this]() {
            showMainStudio();
        });
    }
}

void MainWindow::onConfigReceived(int freqIdx, double freqVal, int paIdx, int paVal, int squelchIdx, int squelchVal) {
    Q_UNUSED(freqVal);
    Q_UNUSED(paVal);
    Q_UNUSED(squelchVal);
    if (m_cfgFreqCombo && freqIdx >= 0 && freqIdx < m_cfgFreqCombo->count()) m_cfgFreqCombo->setCurrentIndex(freqIdx);
    if (m_cfgPaCombo && paIdx >= 0 && paIdx < m_cfgPaCombo->count()) m_cfgPaCombo->setCurrentIndex(paIdx);
    if (m_cfgSquelchCombo && squelchIdx >= 0 && squelchIdx < m_cfgSquelchCombo->count()) m_cfgSquelchCombo->setCurrentIndex(squelchIdx);

    if (m_sniffFreqCombo && freqIdx >= 0 && freqIdx < m_sniffFreqCombo->count()) m_sniffFreqCombo->setCurrentIndex(freqIdx);
    if (m_sniffPaCombo && paIdx >= 0 && paIdx < m_sniffPaCombo->count()) m_sniffPaCombo->setCurrentIndex(paIdx);
    if (m_sniffSquelchCombo && squelchIdx >= 0 && squelchIdx < m_sniffSquelchCombo->count()) m_sniffSquelchCombo->setCurrentIndex(squelchIdx);
}

void MainWindow::onScreenFrameReceived(const QByteArray &frameData) {
    m_oledWidget->updateFrame(frameData);
    if (m_chassisOledWidget) {
        m_chassisOledWidget->updateFrame(frameData);
    }
}

void MainWindow::toggleScreenStream() {
    m_screenStreamActive = !m_screenStreamActive;
    if (m_screenStreamActive) {
        m_btnStreamToggle->setText("[ СТРИМ: ВКЛ ]");
        m_btnStreamToggle->setObjectName("btnPrimary");
        m_dev->startScreenStream();
    } else {
        m_btnStreamToggle->setText("[ СТРИМ: ВЫКЛ ]");
        m_btnStreamToggle->setObjectName("");
        m_dev->stopScreenStream();
    }
    m_btnStreamToggle->style()->unpolish(m_btnStreamToggle);
    m_btnStreamToggle->style()->polish(m_btnStreamToggle);
}

void MainWindow::saveScreenSnapshot() {
    QImage img = m_oledWidget->grabDisplayImage();
    QString fileName = QFileDialog::getSaveFileName(this, "СОХРАНИТЬ СНИМОК ЭКРАНА", "oled_screen.png", "Images (*.png)");
    if (!fileName.isEmpty()) {
        img.save(fileName);
        appendLog(QString("[OK] Снимок сохранен: %1").arg(fileName));
    }
}

// -----------------------------------------------------------------
// СЛОТЫ СОХРАНЕННЫХ СИГНАЛОВ
// -----------------------------------------------------------------
void MainWindow::onSignalListBegin(int count) {
    Q_UNUSED(count);
    m_signals.clear();
    m_tableSignals->setRowCount(0);
    clearInspector();
}

void MainWindow::onSignalItemReceived(const SignalItem &item) {
    m_signals.append(item);

    int row = m_tableSignals->rowCount();
    m_tableSignals->insertRow(row);

    m_tableSignals->setItem(row, 0, new QTableWidgetItem(QString::number(item.index + 1)));
    m_tableSignals->setItem(row, 1, new QTableWidgetItem(item.name));
    m_tableSignals->setItem(row, 2, new QTableWidgetItem(item.typeName()));
    m_tableSignals->setItem(row, 3, new QTableWidgetItem(QString("%1 МГц").arg(item.freq, 0, 'f', 2)));
    m_tableSignals->setItem(row, 4, new QTableWidgetItem(item.protoName));
    m_tableSignals->setItem(row, 5, new QTableWidgetItem(item.type == SIG_KIND_RAW ? "—" : item.codeHex()));
    m_tableSignals->setItem(row, 6, new QTableWidgetItem(item.type == SIG_KIND_RAW ? "—" : QString::number(item.bitLen)));
    m_tableSignals->setItem(row, 7, new QTableWidgetItem(item.type == SIG_KIND_RAW ? QString("%1 имп.").arg(item.rawCount) : QString("%1 µs").arg(item.pulseLen)));
    m_tableSignals->setItem(row, 8, new QTableWidgetItem(QString::number(item.repeats)));
}

void MainWindow::onSignalListEnd() {
    appendLog(QString("[INFO] Загружено %1 сигналов из NVS").arg(m_signals.size()));
    if (!m_signals.isEmpty()) {
        m_tableSignals->selectRow(0);
    }
}

void MainWindow::onTableSelectionChanged() {
    int row = m_tableSignals->currentRow();
    if (row >= 0 && row < m_signals.size()) {
        m_selectedSignalIdx = row;
        updateInspector(m_signals[row]);
    } else {
        m_selectedSignalIdx = -1;
        clearInspector();
    }
}

void MainWindow::updateInspector(const SignalItem &item) {
    m_inspectorTitle->setText(QString("[ СИГНАЛ #%1: %2 ]").arg(item.index + 1).arg(item.name));
    m_lblFreqVal->setText(QString("%1 МГц").arg(item.freq, 0, 'f', 3));
    m_lblProtoVal->setText(QString("%1 (%2)").arg(item.protoName, item.typeName()));
    m_lblCodeHexVal->setText(item.type == SIG_KIND_RAW ? "—" : item.codeHex());
    m_lblCodeDecVal->setText(item.type == SIG_KIND_RAW ? "—" : QString::number(item.code));
    m_lblCodeBinVal->setText(item.type == SIG_KIND_RAW ? "—" : item.codeBin());
    m_lblTimingVal->setText(item.type == SIG_KIND_RAW ? QString("RAW импульсов: %1").arg(item.rawCount)
                                                       : QString("%1 бит | TE: %2 µs").arg(item.bitLen).arg(item.pulseLen));

    if (item.isKeeloq) {
        m_klqGroup->setVisible(true);
        m_lblKlqSerial->setText(QString("%1 (%2)").arg(item.keeloqSerialHex()).arg(item.keeloqSerial));
        m_lblKlqBtn->setText(QString("Кнопка %1").arg(item.keeloqBtn));
        m_lblKlqEnc->setText(item.keeloqEncHex());
    } else {
        m_klqGroup->setVisible(false);
    }

    m_txRepeatsSpin->setValue(item.repeats > 0 ? item.repeats : 5);

    QVector<uint16_t> wave = item.toWaveformPulses();
    m_inspectorWaveform->setPulses(wave);
}

void MainWindow::clearInspector() {
    m_inspectorTitle->setText("[ ВЫБЕРИТЕ СИГНАЛ ИЗ ТАБЛИЦЫ ]");
    m_lblFreqVal->setText("—");
    m_lblProtoVal->setText("—");
    m_lblCodeHexVal->setText("—");
    m_lblCodeDecVal->setText("—");
    m_lblCodeBinVal->setText("—");
    m_lblTimingVal->setText("—");
    m_klqGroup->setVisible(false);
    m_inspectorWaveform->clear();
}

void MainWindow::refreshSignalsFromDevice() {
    if (!m_dev->isConnected()) {
        QMessageBox::warning(this, "ВНИМАНИЕ", "ПОДКЛЮЧИТЕ УСТРОЙСТВО ПО SERIAL");
        return;
    }
    m_dev->requestSignals();
}

void MainWindow::addNewSignal() {
    SignalDetailDialog dlg(this, SignalItem(), true);
    if (dlg.exec() == QDialog::Accepted) {
        SignalItem sig = dlg.getSignal();
        if (sig.type == SIG_KIND_RAW) {
            m_dev->saveRaw(sig.name, sig.freq, sig.repeats, sig.protoName, sig.isKeeloq, sig.keeloqSerial, sig.keeloqBtn, sig.keeloqEnc, sig.pulses);
        } else {
            m_dev->saveFixed(sig.name, sig.freq, sig.code, sig.bitLen, sig.pulseLen, sig.protocol, sig.repeats, sig.protoName);
        }
    }
}

void MainWindow::editCurrentSignal() {
    if (m_selectedSignalIdx < 0 || m_selectedSignalIdx >= m_signals.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ СИГНАЛ ДЛЯ РЕДАКТИРОВАНИЯ");
        return;
    }
    SignalItem cur = m_signals[m_selectedSignalIdx];
    SignalDetailDialog dlg(this, cur, false);
    if (dlg.exec() == QDialog::Accepted) {
        SignalItem sig = dlg.getSignal();
        m_dev->deleteSignal(m_selectedSignalIdx);
        if (sig.type == SIG_KIND_RAW) {
            m_dev->saveRaw(sig.name, sig.freq, sig.repeats, sig.protoName, sig.isKeeloq, sig.keeloqSerial, sig.keeloqBtn, sig.keeloqEnc, sig.pulses);
        } else {
            m_dev->saveFixed(sig.name, sig.freq, sig.code, sig.bitLen, sig.pulseLen, sig.protocol, sig.repeats, sig.protoName);
        }
    }
}

void MainWindow::deleteCurrentSignal() {
    if (m_selectedSignalIdx < 0 || m_selectedSignalIdx >= m_signals.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ СИГНАЛ ДЛЯ УДАЛЕНИЯ");
        return;
    }
    if (QMessageBox::question(this, "ПОДТВЕРЖДЕНИЕ",
                             QString("Удалить сигнал '%1' из NVS?").arg(m_signals[m_selectedSignalIdx].name)) == QMessageBox::Yes) {
        m_dev->deleteSignal(m_selectedSignalIdx);
    }
}

void MainWindow::clearAllSignals() {
    if (QMessageBox::warning(this, "ОЧИСТКА ПАМЯТИ",
                             "УДАЛИТЬ ВСЕ СИГНАЛЫ ИЗ ПАМЯТИ УСТРОЙСТВА?",
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_dev->clearSignals();
        m_signals.clear();
        m_tableSignals->setRowCount(0);
        clearInspector();
    }
}

void MainWindow::transmitCurrentSignal() {
    if (m_selectedSignalIdx < 0 || m_selectedSignalIdx >= m_signals.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ СИГНАЛ ДЛЯ ПЕРЕДАЧИ");
        return;
    }
    appendLog(QString("[TX] Передача сигнала #%1 (%2)...").arg(m_selectedSignalIdx + 1).arg(m_signals[m_selectedSignalIdx].name));
    m_dev->transmitSaved(m_selectedSignalIdx);
}

void MainWindow::exportBackupJson() {
    QString fileName = QFileDialog::getSaveFileName(this, "ЭКСПОРТ РЕЗЕРВНОЙ КОПИИ", "rf_signals_backup.json", "JSON (*.json)");
    if (fileName.isEmpty()) return;

    QJsonArray arr;
    for (const auto &s : m_signals) {
        arr.append(s.toJson());
    }
    QJsonDocument doc(arr);
    QFile f(fileName);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
        appendLog(QString("[OK] Бэкап сохранен: %1").arg(fileName));
    }
}

void MainWindow::restoreBackupJson() {
    QString fileName = QFileDialog::getOpenFileName(this, "ВОССТАНОВИТЬ ИЗ БЭКАПА", "", "JSON (*.json)");
    if (fileName.isEmpty()) return;

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) return;
    QByteArray data = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::critical(this, "ОШИБКА", "НЕВЕРНЫЙ ФОРМАТ БЭКАПА");
        return;
    }

    QJsonArray arr = doc.array();
    for (const auto &v : arr) {
        SignalItem s = SignalItem::fromJson(v.toObject());
        if (s.type == SIG_KIND_RAW) {
            m_dev->saveRaw(s.name, s.freq, s.repeats, s.protoName, s.isKeeloq, s.keeloqSerial, s.keeloqBtn, s.keeloqEnc, s.pulses);
        } else {
            m_dev->saveFixed(s.name, s.freq, s.code, s.bitLen, s.pulseLen, s.protocol, s.repeats, s.protoName);
        }
    }
    appendLog("[OK] База сигналов передана на устройство");
    m_dev->requestSignals();
}

void MainWindow::exportCurrentToSubGhz() {
    if (m_selectedSignalIdx < 0 || m_selectedSignalIdx >= m_signals.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ СИГНАЛ");
        return;
    }
    const SignalItem &s = m_signals[m_selectedSignalIdx];
    QString defaultName = QString("%1.sub").arg(s.name.simplified().replace(' ', '_'));
    QString fileName = QFileDialog::getSaveFileName(this, "ЭКСПОРТ В SUB-GHZ KEY FILE", defaultName, "Sub-GHz Key Files (*.sub)");
    if (fileName.isEmpty()) return;

    QFile f(fileName);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(s.toSubGhzFile().toUtf8());
        f.close();
        appendLog(QString("[OK] Экспорт .sub завершен: %1").arg(fileName));
    }
}

void MainWindow::importFromSubGhz() {
    QString fileName = QFileDialog::getOpenFileName(this, "ИМПОРТ SUB-GHZ KEY FILE", "", "Sub-GHz Key Files (*.sub)");
    if (fileName.isEmpty()) return;

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QFileInfo fi(fileName);
    SignalItem item = SignalItem::fromSubGhzFile(content, fi.baseName());

    SignalDetailDialog dlg(this, item, true);
    if (dlg.exec() == QDialog::Accepted) {
        SignalItem sig = dlg.getSignal();
        if (sig.type == SIG_KIND_RAW) {
            m_dev->saveRaw(sig.name, sig.freq, sig.repeats, sig.protoName, sig.isKeeloq, sig.keeloqSerial, sig.keeloqBtn, sig.keeloqEnc, sig.pulses);
        } else {
            m_dev->saveFixed(sig.name, sig.freq, sig.code, sig.bitLen, sig.pulseLen, sig.protocol, sig.repeats, sig.protoName);
        }
    }
}

// -----------------------------------------------------------------
// СЛОТЫ ГЕНЕРАТОРА
// -----------------------------------------------------------------
void MainWindow::onGenHexChanged(const QString &text) {
    if (m_updatingGenCode) return;
    m_updatingGenCode = true;
    QString clean = text;
    clean.remove("0x", Qt::CaseInsensitive).remove(' ');
    bool ok = false;
    quint32 val = static_cast<quint32>(clean.toULongLong(&ok, 16));
    if (ok) {
        m_genDecSpin->setValue(static_cast<int>(val));
        updateGenWaveform();
    }
    m_updatingGenCode = false;
}

void MainWindow::onGenDecChanged(int value) {
    if (m_updatingGenCode) return;
    m_updatingGenCode = true;
    m_genHexEdit->setText(QString("0x%1").arg(static_cast<quint32>(value), 6, 16, QChar('0')).toUpper());
    updateGenWaveform();
    m_updatingGenCode = false;
}

void MainWindow::updateGenWaveform() {
    SignalItem item;
    item.protoName = m_genProtoCombo->currentText();
    item.code = static_cast<quint32>(m_genDecSpin->value());
    item.bitLen = m_genBitLenSpin->value();
    item.pulseLen = m_genPulseLenSpin->value();
    m_genWaveform->setPulses(item.toWaveformPulses());
}

void MainWindow::transmitCustomFixed() {
    double freq = m_genFreqSpin->value();
    quint32 code = static_cast<quint32>(m_genDecSpin->value());
    int bitLen = m_genBitLenSpin->value();
    int pulseLen = m_genPulseLenSpin->value();
    int repeats = m_genRepeatsSpin->value();
    int pa = 10;
    if (m_genPowerCombo->currentIndex() == 1) pa = 0;
    else if (m_genPowerCombo->currentIndex() == 2) pa = -10;
    else if (m_genPowerCombo->currentIndex() == 3) pa = -30;

    int proto = 1;
    if (m_genProtoCombo->currentText().contains("Came")) proto = 6;
    else if (m_genProtoCombo->currentText().contains("Nice")) proto = 7;

    appendLog(QString("[TX] Фиксированный код: %1 МГц, 0x%2, %3 бит, TE=%4 µs, Повторов: %5")
                  .arg(freq).arg(code, 6, 16, QChar('0')).arg(bitLen).arg(pulseLen).arg(repeats));

    m_dev->transmitFixed(freq, code, bitLen, pulseLen, proto, pa, repeats);
}

void MainWindow::saveCustomFixed() {
    QString name = m_genNameEdit->text().trimmed();
    if (name.isEmpty()) name = "Signal Fixed";
    double freq = m_genFreqSpin->value();
    quint32 code = static_cast<quint32>(m_genDecSpin->value());
    int bitLen = m_genBitLenSpin->value();
    int pulseLen = m_genPulseLenSpin->value();
    int repeats = m_genRepeatsSpin->value();
    m_dev->saveFixed(name, freq, code, bitLen, pulseLen, 1, repeats, m_genProtoCombo->currentText());
}

void MainWindow::onRawGenTextChanged() {
    QString text = m_rawGenText->toPlainText();
    QStringList tokens = text.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
    QVector<uint16_t> pulses;
    for (const auto &t : tokens) {
        int v = t.toInt();
        if (v > 0) pulses.append(static_cast<uint16_t>(v));
    }
    m_rawGenWaveform->setPulses(pulses);
}

void MainWindow::transmitCustomRaw() {
    QString text = m_rawGenText->toPlainText();
    QStringList tokens = text.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
    QVector<uint16_t> pulses;
    for (const auto &t : tokens) {
        int v = t.toInt();
        if (v > 0) pulses.append(static_cast<uint16_t>(v));
    }
    if (pulses.isEmpty()) {
        QMessageBox::warning(this, "ВНИМАНИЕ", "ВВЕДИТЕ RAW ИМПУЛЬСЫ");
        return;
    }
    double freq = m_rawGenFreqSpin->value();
    int reps = m_rawGenRepeatsSpin->value();
    int pa = 10;
    if (m_rawGenPowerCombo->currentIndex() == 1) pa = 0;
    else if (m_rawGenPowerCombo->currentIndex() == 2) pa = -10;
    else if (m_rawGenPowerCombo->currentIndex() == 3) pa = -30;

    appendLog(QString("[TX RAW] %1 импульсов, %2 повторов на %3 МГц").arg(pulses.size()).arg(reps).arg(freq));
    m_dev->transmitRaw(freq, pa, reps, pulses);
}

void MainWindow::saveCustomRaw() {
    QString name = m_rawGenNameEdit->text().trimmed();
    if (name.isEmpty()) name = "Signal RAW";
    QString text = m_rawGenText->toPlainText();
    QStringList tokens = text.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
    QVector<uint16_t> pulses;
    for (const auto &t : tokens) {
        int v = t.toInt();
        if (v > 0) pulses.append(static_cast<uint16_t>(v));
    }
    if (pulses.isEmpty()) {
        QMessageBox::warning(this, "ВНИМАНИЕ", "ВВЕДИТЕ RAW ИМПУЛЬСЫ");
        return;
    }
    double freq = m_rawGenFreqSpin->value();
    int reps = m_rawGenRepeatsSpin->value();
    m_dev->saveRaw(name, freq, reps, "RAW", false, 0, 0, 0, pulses);
}

void MainWindow::loadRawPreset(int index) {
    if (index == 0) {
        m_rawGenNameEdit->setText("Came 12b");
        m_rawGenText->setPlainText("320, 11520, 640, 320, 320, 640, 640, 320, 320, 640, 640, 320, 320, 640, 640, 320, 320, 640, 640, 320, 320, 640, 640, 320, 320, 640");
    } else if (index == 1) {
        m_rawGenNameEdit->setText("Nice Flo");
        m_rawGenText->setPlainText("700, 16800, 700, 1400, 1400, 700, 700, 1400, 1400, 700, 700, 1400, 1400, 700, 700, 1400, 1400, 700, 700, 1400, 1400, 700, 700, 1400, 1400, 700");
    } else if (index == 2) {
        m_rawGenNameEdit->setText("Doorbell EV1527");
        m_rawGenText->setPlainText("320, 9920, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960, 960, 320, 320, 960");
    }
}

void MainWindow::toggleCarrierMode() {
    if (!m_dev->isConnected()) {
        QMessageBox::warning(this, "ВНИМАНИЕ", "ПОДКЛЮЧИТЕ УСТРОЙСТВО");
        return;
    }
    m_carrierActive = !m_carrierActive;
    if (m_carrierActive) {
        int mode = m_carrierModeCombo->currentData().toInt();
        double freq = m_carrierFreqSpin->value();
        int pa = 10;
        if (m_carrierPowerCombo->currentIndex() == 1) pa = 0;
        else if (m_carrierPowerCombo->currentIndex() == 2) pa = -10;
        else if (m_carrierPowerCombo->currentIndex() == 3) pa = -30;

        m_dev->setCarrier(mode, freq, pa, true);
        m_btnCarrierToggle->setText("[ ОСТАНОВИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]");
        appendLog(QString("[TX CARRIER] Передача несущей на %1 МГц включена").arg(freq));
    } else {
        m_dev->setCarrier(0, 433.92, 10, false);
        m_btnCarrierToggle->setText("[ ВКЛЮЧИТЬ ПЕРЕДАЧУ НЕСУЩЕЙ ]");
        appendLog("[CARRIER] Передача несущей выключена.");
    }
}

// -----------------------------------------------------------------
// СЛОТЫ УПРАВЛЕНИЯ И СНИФФЕРА
// -----------------------------------------------------------------
void MainWindow::sendVirtualBtn(const QString &btn) {
    if (!m_dev->isConnected()) return;
    appendLog(QString("[REMOTE] Кнопка: %1").arg(btn));
    m_dev->sendRemoteButton(btn);

    // Сразу запрашиваем обновленный кадр экрана
    m_dev->requestScreenFrame();
}

void MainWindow::applySnifferConfig() {
    if (!m_dev->isConnected()) return;
    int f = m_sniffFreqCombo ? m_sniffFreqCombo->currentIndex() : 0;
    int p = m_sniffPaCombo ? m_sniffPaCombo->currentIndex() : 3;
    int sq = m_sniffSquelchCombo ? m_sniffSquelchCombo->currentIndex() : 1;
    m_dev->setConfig(f, p, sq);
    if (m_cfgFreqCombo) m_cfgFreqCombo->setCurrentIndex(f);
    if (m_cfgPaCombo) m_cfgPaCombo->setCurrentIndex(p);
    if (m_cfgSquelchCombo) m_cfgSquelchCombo->setCurrentIndex(sq);
    appendLog(QString("[CFG] НАСТРОЙКИ РАДИОМОДУЛЯ ПРИМЕНЕНЫ: Freq=%1, PA=%2, Squelch=%3")
                  .arg(m_sniffFreqCombo->currentText())
                  .arg(m_sniffPaCombo->currentText())
                  .arg(m_sniffSquelchCombo->currentText()));
}

void MainWindow::startSnifferFixed() {
    if (!m_dev->isConnected()) return;
    if (m_sniffFreqCombo && m_sniffPaCombo && m_sniffSquelchCombo) {
        m_dev->setConfig(m_sniffFreqCombo->currentIndex(), m_sniffPaCombo->currentIndex(), m_sniffSquelchCombo->currentIndex());
    }
    m_dev->setDeviceMode("RX");
    appendLog("[RX] ЗАПУЩЕН СНИФФЕР OOK/ASK (ФИКСИРОВАННЫЕ КОДЫ)");
}

void MainWindow::startSnifferRaw() {
    if (!m_dev->isConnected()) return;
    if (m_sniffFreqCombo && m_sniffPaCombo && m_sniffSquelchCombo) {
        m_dev->setConfig(m_sniffFreqCombo->currentIndex(), m_sniffPaCombo->currentIndex(), m_sniffSquelchCombo->currentIndex());
    }
    m_dev->setDeviceMode("RAW");
    appendLog("[RAW] ЗАПУЩЕН RAW СНИФФЕР (KEELOQ / RAW ИМПУЛЬСЫ)");
}

void MainWindow::stopSnifferMode() {
    if (!m_dev->isConnected()) return;
    m_dev->setDeviceMode("MAIN");
    appendLog("[SNIFFER] СНИФФЕР ОСТАНОВЛЕН, ВОЗВРАТ В ГЛАВНОЕ МЕНЮ.");
}

void MainWindow::onCapturedFixedPacket(quint32 code, const QString &codeHex, int bitLen, int pulseLen, int proto, double freq, int rssi) {
    SnifferEntry se;
    se.timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    se.rssi = rssi;
    se.item.name = QString("Sniffed 0x%1").arg(code, 6, 16, QChar('0')).toUpper();
    se.item.type = SIG_KIND_FIXED;
    se.item.freq = freq;
    se.item.code = code;
    se.item.bitLen = bitLen;
    se.item.pulseLen = pulseLen;
    se.item.protocol = proto;
    se.item.protoName = (proto == 1) ? "EV1527" : QString("Proto %1").arg(proto);
    se.item.repeats = 5;

    m_snifferHistory.prepend(se);

    int row = 0;
    m_tableSniffer->insertRow(row);
    m_tableSniffer->setItem(row, 0, new QTableWidgetItem(se.timeStr));
    m_tableSniffer->setItem(row, 1, new QTableWidgetItem(QString("%1 МГц").arg(freq, 0, 'f', 2)));
    m_tableSniffer->setItem(row, 2, new QTableWidgetItem(se.item.protoName));
    m_tableSniffer->setItem(row, 3, new QTableWidgetItem(codeHex));
    m_tableSniffer->setItem(row, 4, new QTableWidgetItem(QString::number(bitLen)));
    m_tableSniffer->setItem(row, 5, new QTableWidgetItem(QString("%1 µs").arg(pulseLen)));
    m_tableSniffer->setItem(row, 6, new QTableWidgetItem(QString("%1 dBm").arg(rssi)));

    appendLog(QString("[CAPTURED] Code=%1 (%2b, %3µs) RSSI=%4 dBm")
                  .arg(codeHex).arg(bitLen).arg(pulseLen).arg(rssi));
}

void MainWindow::onCapturedRawPacket(const QString &proto, bool isKeeloq, quint32 serial, int btn, quint32 enc, int count, double freq, int rssi, const QVector<uint16_t> &pulses) {
    SnifferEntry se;
    se.timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    se.rssi = rssi;
    se.item.name = isKeeloq ? QString("KLQ #%1").arg(serial & 0xFFFF, 4, 16, QChar('0')) : QString("RAW #%1").arg(count);
    se.item.type = SIG_KIND_RAW;
    se.item.freq = freq;
    se.item.protoName = proto;
    se.item.isKeeloq = isKeeloq;
    se.item.keeloqSerial = serial;
    se.item.keeloqBtn = btn;
    se.item.keeloqEnc = enc;
    se.item.rawCount = count;
    se.item.pulses = pulses;
    se.item.repeats = 5;

    m_snifferHistory.prepend(se);

    int row = 0;
    m_tableSniffer->insertRow(row);
    m_tableSniffer->setItem(row, 0, new QTableWidgetItem(se.timeStr));
    m_tableSniffer->setItem(row, 1, new QTableWidgetItem(QString("%1 МГц").arg(freq, 0, 'f', 2)));
    m_tableSniffer->setItem(row, 2, new QTableWidgetItem(isKeeloq ? "KeeLoq" : proto));
    m_tableSniffer->setItem(row, 3, new QTableWidgetItem(isKeeloq ? QString("SN: 0x%1 (Btn %2)").arg(serial, 7, 16, QChar('0')).arg(btn) : "—"));
    m_tableSniffer->setItem(row, 4, new QTableWidgetItem(isKeeloq ? "66" : "—"));
    m_tableSniffer->setItem(row, 5, new QTableWidgetItem(QString("%1 импульсов").arg(count)));
    m_tableSniffer->setItem(row, 6, new QTableWidgetItem(QString("%1 dBm").arg(rssi)));

    appendLog(QString("[CAPTURED RAW] %1: %2 имп., RSSI=%3 dBm")
                  .arg(isKeeloq ? "KeeLoq" : proto).arg(count).arg(rssi));
}

void MainWindow::replayCapturedPacket() {
    int row = m_tableSniffer->currentRow();
    if (row < 0 || row >= m_snifferHistory.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ ПАКЕТ ИЗ ТАБЛИЦЫ");
        return;
    }
    const SignalItem &s = m_snifferHistory[row].item;
    if (s.type == SIG_KIND_RAW) {
        appendLog(QString("[REPLAY] RAW (%1 имп.)...").arg(s.pulses.size()));
        m_dev->transmitRaw(s.freq, 10, 5, s.pulses);
    } else {
        appendLog(QString("[REPLAY] Код 0x%1...").arg(s.code, 6, 16, QChar('0')));
        m_dev->transmitFixed(s.freq, s.code, s.bitLen, s.pulseLen, s.protocol, 10, 7);
    }
}

void MainWindow::saveCapturedPacket() {
    int row = m_tableSniffer->currentRow();
    if (row < 0 || row >= m_snifferHistory.size()) {
        QMessageBox::information(this, "ИНФОРМАЦИЯ", "ВЫБЕРИТЕ ПАКЕТ ИЗ ТАБЛИЦЫ");
        return;
    }
    SignalItem s = m_snifferHistory[row].item;
    SignalDetailDialog dlg(this, s, true);
    if (dlg.exec() == QDialog::Accepted) {
        SignalItem toSave = dlg.getSignal();
        if (toSave.type == SIG_KIND_RAW) {
            m_dev->saveRaw(toSave.name, toSave.freq, toSave.repeats, toSave.protoName, toSave.isKeeloq, toSave.keeloqSerial, toSave.keeloqBtn, toSave.keeloqEnc, toSave.pulses);
        } else {
            m_dev->saveFixed(toSave.name, toSave.freq, toSave.code, toSave.bitLen, toSave.pulseLen, toSave.protocol, toSave.repeats, toSave.protoName);
        }
    }
}

void MainWindow::saveHardwareConfig() {
    if (!m_dev->isConnected()) return;
    int f = m_cfgFreqCombo->currentIndex();
    int p = m_cfgPaCombo->currentIndex();
    int sq = m_cfgSquelchCombo->currentIndex();
    m_dev->setConfig(f, p, sq);
    appendLog("[OK] Конфигурация сохранена в NVS");
}

void MainWindow::rebootEspDevice() {
    if (!m_dev->isConnected()) return;
    if (QMessageBox::question(this, "ПЕРЕЗАГРУЗКА", "ПЕРЕЗАГРУЗИТЬ ESP32-C3?") == QMessageBox::Yes) {
        m_dev->rebootDevice();
    }
}

// -----------------------------------------------------------------
// СЛОТЫ КОНСОЛИ
// -----------------------------------------------------------------
void MainWindow::sendConsoleCommand() {
    QString cmd = m_consoleInput->text().trimmed();
    if (cmd.isEmpty()) return;
    m_dev->sendRaw(cmd);
    m_consoleInput->clear();
}

void MainWindow::appendLog(const QString &msg, bool isError) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString color = isError ? "#ffffff" : "#aaaaaa";
    m_consoleLog->appendHtml(QString("<span style='color: #666666;'>[%1]</span> <span style='color: %2; font-weight: %3;'>%4</span>")
                                 .arg(timeStr, color, isError ? "bold" : "normal", msg.toHtmlEscaped()));
}

void MainWindow::appendRawTx(const QString &raw) {
    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_consoleLog->appendHtml(QString("<span style='color: #555555;'>[%1]</span> <b style='color: #ffffff;'>TX &gt;&gt;</b> <span style='color: #cccccc;'>%2</span>")
                                 .arg(timeStr, raw.toHtmlEscaped()));
}

void MainWindow::appendRawRx(const QString &raw) {
    // Не выводим фреймы SCREEN в текстовую консоль, чтобы не перегружать лог
    if (raw.contains("\"cmd\":\"SCREEN\"")) return;

    QString timeStr = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_consoleLog->appendHtml(QString("<span style='color: #555555;'>[%1]</span> <b style='color: #888888;'>RX &lt;&lt;</b> <span style='color: #ffffff;'>%2</span>")
                                 .arg(timeStr, raw.toHtmlEscaped()));
}
