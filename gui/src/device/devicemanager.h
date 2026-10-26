#pragma once

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>
#include "../models/signalitem.h"

struct DeviceInfo {
    QString deviceName;
    QString version;
    QString chip;
    bool cc1101Ok = false;
    quint32 freeHeap = 0;
    int signalsCount = 0;
    int maxSignals = 20;
    double freq = 433.92;
    int paLevel = 10;
    int squelch = -72;
};

class DeviceManager : public QObject {
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);
    ~DeviceManager();

    static QList<QSerialPortInfo> availablePorts();

    bool connectToPort(const QString &portName, qint32 baudRate = 115200);
    void disconnectPort();
    bool isConnected() const;
    QString currentPortName() const;

    const DeviceInfo& deviceInfo() const { return m_info; }

    // Команды устройства
    void sendPing();
    void requestSignals();
    void transmitSaved(int index);
    void transmitFixed(double freq, quint32 code, int bitLen, int pulseLen, int proto, int paLevel, int repeats);
    void transmitRaw(double freq, int paLevel, int repeats, const QVector<uint16_t> &pulses);
    void saveFixed(const QString &name, double freq, quint32 code, int bitLen, int pulseLen, int proto, int repeats, const QString &protoName);
    void saveRaw(const QString &name, double freq, int repeats, const QString &protoName, bool isKeeloq, quint32 serial, int btn, quint32 enc, const QVector<uint16_t> &pulses);
    void deleteSignal(int index);
    void clearSignals();
    void requestConfig();
    void setConfig(int freqIdx, int paIdx, int squelchIdx);
    void sendRemoteButton(const QString &btn);
    void startSniffer(const QString &mode);
    void stopSniffer();
    void setCarrier(int mode, double freq, int paLevel, bool on);
    void rebootDevice();
    void startScreenStream();
    void stopScreenStream();
    void requestScreenFrame();
    void setDeviceMode(const QString &modeName);
    void sendRaw(const QString &line);

signals:
    void connectionStateChanged(bool connected, const QString &portName);
    void pongReceived(const DeviceInfo &info);
    void screenFrameReceived(const QByteArray &frameData);
    void signalListBegin(int total);
    void signalItemReceived(const SignalItem &item);
    void signalListEnd();
    void txFinished(bool ok, const QString &msg);
    void saveFinished(bool ok, int totalCount);
    void deleteFinished(bool ok, int totalCount);
    void configReceived(int freqIdx, double freqVal, int paIdx, int paVal, int squelchIdx, int squelchVal);
    void capturedFixedPacket(quint32 code, const QString &codeHex, int bitLen, int pulseLen, int proto, double freq, int rssi);
    void capturedRawPacket(const QString &proto, bool isKeeloq, quint32 serial, int btn, quint32 enc, int count, double freq, int rssi, const QVector<uint16_t> &pulses);
    void logMessage(const QString &msg, bool isError = false);
    void rawDataReceived(const QString &raw);
    void rawDataSent(const QString &raw);

private slots:
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_port = nullptr;
    DeviceInfo m_info;
    QByteArray m_rxBuffer;

    void parseLine(const QString &line);
    void writeLine(const QString &line);
};
