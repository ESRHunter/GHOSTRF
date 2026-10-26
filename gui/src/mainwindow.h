#pragma once

#include <QMainWindow>
#include <QVector>
#include <QKeyEvent>
#include "device/devicemanager.h"
#include "models/signalitem.h"

class QComboBox;
class QPushButton;
class QLabel;
class QTableWidget;
class QTabWidget;
class QPlainTextEdit;
class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;
class WaveformWidget;
class OledDisplayWidget;
class QTimer;
class QStackedWidget;
class QProgressBar;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    // Подключение
    void refreshSerialPorts();
    void toggleConnection();
    void onConnectionStateChanged(bool connected, const QString &portName);
    void onPongReceived(const DeviceInfo &info);
    void onBatteryUpdated(int pct, int mv, bool charging);
    void onConfigReceived(int freqIdx, double freqVal, int paIdx, int paVal, int squelchIdx, int squelchVal);

    // Стриминг дисплея OLED
    void onScreenFrameReceived(const QByteArray &frameData);
    void toggleScreenStream();
    void saveScreenSnapshot();

    // Сохранённые сигналы
    void onSignalListBegin(int count);
    void onSignalItemReceived(const SignalItem &item);
    void onSignalListEnd();
    void onTableSelectionChanged();
    void refreshSignalsFromDevice();
    void addNewSignal();
    void editCurrentSignal();
    void deleteCurrentSignal();
    void clearAllSignals();
    void transmitCurrentSignal();
    void exportBackupJson();
    void restoreBackupJson();
    void exportCurrentToSubGhz();
    void importFromSubGhz();

    // Генератор и передатчик
    void onGenHexChanged(const QString &text);
    void onGenDecChanged(int value);
    void updateGenWaveform();
    void transmitCustomFixed();
    void saveCustomFixed();

    void onRawGenTextChanged();
    void transmitCustomRaw();
    void saveCustomRaw();
    void loadRawPreset(int index);

    void toggleCarrierMode();

    // Управление устройством и Сниффер
    void sendVirtualBtn(const QString &btn);
    void startSnifferFixed();
    void startSnifferRaw();
    void stopSnifferMode();
    void onCapturedFixedPacket(quint32 code, const QString &codeHex, int bitLen, int pulseLen, int proto, double freq, int rssi);
    void onCapturedRawPacket(const QString &proto, bool isKeeloq, quint32 serial, int btn, quint32 enc, int count, double freq, int rssi, const QVector<uint16_t> &pulses);
    void replayCapturedPacket();
    void saveCapturedPacket();
    void saveHardwareConfig();
    void applySnifferConfig();
    void rebootEspDevice();
    void checkAutoConnect();

    // Консоль
    void sendConsoleCommand();
    void appendLog(const QString &msg, bool isError = false);
    void appendRawTx(const QString &raw);
    void appendRawRx(const QString &raw);

private:
    DeviceManager *m_dev;
    QTimer *m_autoConnectTimer = nullptr;
    QVector<SignalItem> m_signals;
    int m_selectedSignalIdx = -1;
    bool m_updatingGenCode = false;
    bool m_carrierActive = false;
    bool m_screenStreamActive = true;

    // Сниффер пойманные пакеты
    struct SnifferEntry {
        QString timeStr;
        SignalItem item;
        int rssi;
    };
    QVector<SnifferEntry> m_snifferHistory;

    // UI Элементы верхней панели
    QComboBox *m_portCombo;
    QPushButton *m_btnRefreshPorts;
    QComboBox *m_baudCombo;
    QPushButton *m_btnConnect;
    QLabel *m_statusLed;
    QLabel *m_statusText;
    QLabel *m_batteryBadge;
    QLabel *m_telemetryBadge;

    // Вкладка: Стриминг дисплея и пульт
    OledDisplayWidget *m_oledWidget;
    QPushButton *m_btnStreamToggle;
    QPushButton *m_btnInvertOled;
    QPushButton *m_btnGridOled;
    QPushButton *m_btnSnapOled;
    QPushButton *m_btnRemoteUp;
    QPushButton *m_btnRemoteOk;
    QPushButton *m_btnRemoteDown;
    QPushButton *m_btnRemoteHold;

    // Вкладка: Сохраненные сигналы
    QTableWidget *m_tableSignals;
    QLabel *m_inspectorTitle;
    QLabel *m_lblFreqVal;
    QLabel *m_lblProtoVal;
    QLabel *m_lblCodeHexVal;
    QLabel *m_lblCodeDecVal;
    QLabel *m_lblCodeBinVal;
    QLabel *m_lblTimingVal;
    QWidget *m_klqGroup;
    QLabel *m_lblKlqSerial;
    QLabel *m_lblKlqBtn;
    QLabel *m_lblKlqEnc;
    WaveformWidget *m_inspectorWaveform;
    QSpinBox *m_txRepeatsSpin;
    QComboBox *m_txPowerCombo;
    QPushButton *m_btnTransmitSaved;

    // Вкладка: Передатчик / Генератор
    QLineEdit *m_genNameEdit;
    QDoubleSpinBox *m_genFreqSpin;
    QComboBox *m_genProtoCombo;
    QLineEdit *m_genHexEdit;
    QSpinBox *m_genDecSpin;
    QSpinBox *m_genBitLenSpin;
    QSpinBox *m_genPulseLenSpin;
    QSpinBox *m_genRepeatsSpin;
    QComboBox *m_genPowerCombo;
    WaveformWidget *m_genWaveform;

    // RAW Генератор
    QLineEdit *m_rawGenNameEdit;
    QDoubleSpinBox *m_rawGenFreqSpin;
    QComboBox *m_rawGenPowerCombo;
    QSpinBox *m_rawGenRepeatsSpin;
    QPlainTextEdit *m_rawGenText;
    WaveformWidget *m_rawGenWaveform;

    // Несущая / Джеммер
    QComboBox *m_carrierModeCombo;
    QDoubleSpinBox *m_carrierFreqSpin;
    QComboBox *m_carrierPowerCombo;
    QPushButton *m_btnCarrierToggle;

    // Вкладка: Сниффер
    QTableWidget *m_tableSniffer;
    QPushButton *m_btnSnifferFixed;
    QPushButton *m_btnSnifferRaw;
    QPushButton *m_btnSnifferStop;
    QComboBox *m_cfgFreqCombo;
    QComboBox *m_cfgPaCombo;
    QComboBox *m_cfgSquelchCombo;
    QComboBox *m_sniffFreqCombo;
    QComboBox *m_sniffPaCombo;
    QComboBox *m_sniffSquelchCombo;
    QPushButton *m_btnApplySniffConfig;

    // Вкладка: Консоль
    QPlainTextEdit *m_consoleLog;
    QLineEdit *m_consoleInput;

    // Экран подключения (Onboarding)
    QStackedWidget *m_rootStack = nullptr;
    QWidget *m_connectScreen = nullptr;
    QWidget *m_mainStudio = nullptr;
    OledDisplayWidget *m_chassisOledWidget = nullptr;
    bool m_isOfflineMode = false;

    QLabel *m_connectScanStatus = nullptr;
    QLabel *m_connectScanHint = nullptr;
    QWidget *m_cardFoundDevice = nullptr;
    QLabel *m_lblFoundName = nullptr;
    QLabel *m_lblFoundPort = nullptr;
    QLabel *m_lblFoundStatus = nullptr;
    QComboBox *m_connectPortCombo = nullptr;
    QPushButton *m_btnConnectRefreshPorts = nullptr;
    QPushButton *m_btnConnectFound = nullptr;
    QLabel *m_step1Label = nullptr;
    QLabel *m_step2Label = nullptr;
    QLabel *m_step3Label = nullptr;
    QPushButton *m_btnConnectRetry = nullptr;
    QPushButton *m_btnConnectOffline = nullptr;

    void setupUi();
    QWidget* createConnectScreen();
    void showConnectScreen(const QString &statusMsg = QString());
    void showMainStudio();
    void resetConnectionSteps();
    QWidget* createTopBar();
    QWidget* createDisplayAndRemoteTab();
    QWidget* createSavedSignalsTab();
    QWidget* createTransmitterTab();
    QWidget* createSnifferTab();
    QWidget* createConsoleTab();
    void updateInspector(const SignalItem &item);
    void clearInspector();
};
