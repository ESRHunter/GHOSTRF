#include "devicemanager.h"
#include <QDebug>
#include <QThread>

DeviceManager::DeviceManager(QObject *parent)
    : QObject(parent), m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &DeviceManager::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &DeviceManager::onErrorOccurred);
}

DeviceManager::~DeviceManager() {
    disconnectPort();
}

QList<QSerialPortInfo> DeviceManager::availablePorts() {
    return QSerialPortInfo::availablePorts();
}

bool DeviceManager::connectToPort(const QString &portName, qint32 baudRate) {
    disconnectPort();

    m_port->setPortName(portName);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit logMessage(QString("Ошибка открытия порта %1: %2").arg(portName, m_port->errorString()), true);
        return false;
    }

    // Для ESP32-C3 USB CDC необходимо включить DTR и RTS
    m_port->setDataTerminalReady(true);
    m_port->setRequestToSend(true);

    emit connectionStateChanged(true, portName);
    emit logMessage(QString("Успешно подключено к %1 (%2 бод)").arg(portName).arg(baudRate));

    // Очищаем буфер и опрашиваем устройство
    m_rxBuffer.clear();
    sendPing();
    requestConfig();
    requestSignals();
    startScreenStream();
    requestScreenFrame();
    return true;
}

void DeviceManager::disconnectPort() {
    if (m_port->isOpen()) {
        QString pName = m_port->portName();
        m_port->setDataTerminalReady(false);
        m_port->setRequestToSend(false);
        m_port->close();
        emit connectionStateChanged(false, pName);
        emit logMessage(QString("Отключено от %1").arg(pName));
    }
}

bool DeviceManager::isConnected() const {
    return m_port && m_port->isOpen();
}

QString DeviceManager::currentPortName() const {
    return m_port ? m_port->portName() : QString();
}

void DeviceManager::writeLine(const QString &line) {
    if (!isConnected()) {
        emit logMessage("Попытка отправки команды при отключенном порте", true);
        return;
    }
    QByteArray data = line.toUtf8() + "\n";
    m_port->write(data);
    emit rawDataSent(line);
}

void DeviceManager::sendPing() {
    writeLine("PING");
}

void DeviceManager::requestSignals() {
    writeLine("GET_SIGNALS");
}

void DeviceManager::transmitSaved(int index) {
    writeLine(QString("TX_SAVED %1").arg(index));
}

void DeviceManager::transmitFixed(double freq, quint32 code, int bitLen, int pulseLen, int proto, int paLevel, int repeats) {
    writeLine(QString("TX_FIXED %1 %2 %3 %4 %5 %6 %7")
                  .arg(freq, 0, 'f', 3)
                  .arg(code)
                  .arg(bitLen)
                  .arg(pulseLen)
                  .arg(proto)
                  .arg(paLevel)
                  .arg(repeats));
}

void DeviceManager::transmitRaw(double freq, int paLevel, int repeats, const QVector<uint16_t> &pulses) {
    QString cmd = QString("TX_RAW %1 %2 %3 %4").arg(freq, 0, 'f', 3).arg(paLevel).arg(repeats).arg(pulses.size());
    for (uint16_t p : pulses) {
        cmd += QString(" %1").arg(p);
    }
    writeLine(cmd);
}

void DeviceManager::saveFixed(const QString &name, double freq, quint32 code, int bitLen, int pulseLen, int proto, int repeats, const QString &protoName) {
    QString safeName = name;
    safeName.replace(' ', '_');
    writeLine(QString("SAVE_FIXED %1 %2 %3 %4 %5 %6 %7 %8")
                  .arg(safeName)
                  .arg(freq, 0, 'f', 3)
                  .arg(code)
                  .arg(bitLen)
                  .arg(pulseLen)
                  .arg(proto)
                  .arg(repeats)
                  .arg(protoName.isEmpty() ? "EV1527" : protoName));
}

void DeviceManager::saveRaw(const QString &name, double freq, int repeats, const QString &protoName, bool isKeeloq, quint32 serial, int btn, quint32 enc, const QVector<uint16_t> &pulses) {
    QString safeName = name;
    safeName.replace(' ', '_');
    QString cmd = QString("SAVE_RAW %1 %2 %3 %4 %5 %6 %7 %8 %9")
                      .arg(safeName)
                      .arg(freq, 0, 'f', 3)
                      .arg(repeats)
                      .arg(protoName.isEmpty() ? "RAW" : protoName)
                      .arg(isKeeloq ? 1 : 0)
                      .arg(serial)
                      .arg(btn)
                      .arg(enc)
                      .arg(pulses.size());
    for (uint16_t p : pulses) {
        cmd += QString(" %1").arg(p);
    }
    writeLine(cmd);
}

void DeviceManager::deleteSignal(int index) {
    writeLine(QString("DELETE_SIGNAL %1").arg(index));
}

void DeviceManager::clearSignals() {
    writeLine("CLEAR_SIGNALS");
}

void DeviceManager::requestConfig() {
    writeLine("GET_CONFIG");
}

void DeviceManager::setConfig(int freqIdx, int paIdx, int squelchIdx) {
    writeLine(QString("SET_CONFIG %1 %2 %3").arg(freqIdx).arg(paIdx).arg(squelchIdx));
}

void DeviceManager::sendRemoteButton(const QString &btn) {
    writeLine(QString("REMOTE_BTN %1").arg(btn));
}

void DeviceManager::startSniffer(const QString &mode) {
    writeLine(QString("START_SNIFFER %1").arg(mode));
}

void DeviceManager::stopSniffer() {
    writeLine("STOP_SNIFFER");
}

void DeviceManager::setCarrier(int mode, double freq, int paLevel, bool on) {
    writeLine(QString("SET_CARRIER %1 %2 %3 %4").arg(mode).arg(freq, 0, 'f', 3).arg(paLevel).arg(on ? 1 : 0));
}

void DeviceManager::rebootDevice() {
    writeLine("REBOOT");
}

void DeviceManager::startScreenStream() {
    writeLine("STREAM_SCREEN 1");
}

void DeviceManager::stopScreenStream() {
    writeLine("STREAM_SCREEN 0");
}

void DeviceManager::requestScreenFrame() {
    writeLine("GET_SCREEN");
}

void DeviceManager::setDeviceMode(const QString &modeName) {
    writeLine(QString("GOTO_MODE %1").arg(modeName));
}

void DeviceManager::sendRaw(const QString &line) {
    writeLine(line);
}

void DeviceManager::onReadyRead() {
    m_rxBuffer.append(m_port->readAll());

    int newlineIdx = -1;
    while ((newlineIdx = m_rxBuffer.indexOf('\n')) != -1) {
        QByteArray lineData = m_rxBuffer.left(newlineIdx);
        m_rxBuffer.remove(0, newlineIdx + 1);

        QString line = QString::fromUtf8(lineData).trimmed();
        if (!line.isEmpty()) {
            parseLine(line);
        }
    }
}

void DeviceManager::parseLine(const QString &line) {
    emit rawDataReceived(line);

    if (!line.startsWith('{') || !line.endsWith('}')) {
        return; // Пропускаем не-JSON строки отладки
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject obj = doc.object();

    if (obj.contains("cmd")) {
        QString cmd = obj["cmd"].toString();

        if (cmd == "PONG") {
            m_info.deviceName = obj.value("device").toString("GHOSTRF");
            m_info.version = obj.value("ver").toString("1.0");
            m_info.chip = obj.value("chip").toString("ESP32-C3");
            m_info.cc1101Ok = obj.value("cc1101").toBool(false);
            m_info.freeHeap = static_cast<quint32>(obj.value("free_heap").toDouble());
            m_info.signalsCount = obj.value("signals_count").toInt();
            m_info.maxSignals = obj.value("max_signals").toInt(20);
            m_info.freq = obj.value("freq").toDouble(433.92);
            m_info.paLevel = obj.value("pa").toInt(10);
            m_info.squelch = obj.value("squelch").toInt(-72);

            emit pongReceived(m_info);
        }
        else if (cmd == "SIGNALS_BEGIN") {
            int total = obj.value("count").toInt();
            emit signalListBegin(total);
        }
        else if (cmd == "SIGNAL") {
            SignalItem item = SignalItem::fromJson(obj);
            emit signalItemReceived(item);
        }
        else if (cmd == "SIGNALS_END") {
            emit signalListEnd();
        }
        else if (cmd == "TX_DONE") {
            bool ok = (obj.value("status").toString() == "OK");
            QString msg = obj.value("msg").toString();
            emit txFinished(ok, msg);
        }
        else if (cmd == "SAVE_DONE") {
            bool ok = (obj.value("status").toString() == "OK");
            int cnt = obj.value("count").toInt();
            emit saveFinished(ok, cnt);
        }
        else if (cmd == "DELETE_DONE") {
            bool ok = (obj.value("status").toString() == "OK");
            int cnt = obj.value("count").toInt();
            emit deleteFinished(ok, cnt);
        }
        else if (cmd == "SCREEN") {
            QString hex = obj.value("hex").toString();
            QByteArray bin = QByteArray::fromHex(hex.toLatin1());
            if (bin.size() == 1024) {
                emit screenFrameReceived(bin);
            }
        }
        else if (cmd == "CONFIG") {
            int fIdx = obj.value("freq_idx").toInt();
            double fVal = obj.value("freq_val").toDouble();
            int pIdx = obj.value("pa_idx").toInt();
            int pVal = obj.value("pa_val").toInt();
            int sqIdx = obj.value("squelch_idx").toInt();
            int sqVal = obj.value("squelch_val").toInt();
            emit configReceived(fIdx, fVal, pIdx, pVal, sqIdx, sqVal);
        }
    }
    else if (obj.contains("event")) {
        QString evt = obj["event"].toString();

        if (evt == "DEVICE_READY") {
            emit logMessage(QString("Устройство готово: %1 (v%2)").arg(obj.value("device").toString(), obj.value("ver").toString()));
            sendPing();
            requestConfig();
            requestSignals();
            startScreenStream();
            requestScreenFrame();
        }
        else if (evt == "CAPTURED_FIXED") {
            quint32 code = static_cast<quint32>(obj.value("code").toVariant().toULongLong());
            QString codeHex = obj.value("codeHex").toString();
            int bitLen = obj.value("bitLen").toInt();
            int pulseLen = obj.value("pulseLen").toInt();
            int proto = obj.value("proto").toInt();
            double freq = obj.value("freq").toDouble();
            int rssi = obj.value("rssi").toInt();
            emit capturedFixedPacket(code, codeHex, bitLen, pulseLen, proto, freq, rssi);
        }
        else if (evt == "CAPTURED_RAW") {
            QString proto = obj.value("proto").toString("RAW");
            bool isK = obj.value("isKeeloq").toBool();
            quint32 ser = static_cast<quint32>(obj.value("serial").toVariant().toULongLong());
            int btn = obj.value("btn").toInt();
            quint32 enc = static_cast<quint32>(obj.value("enc").toVariant().toULongLong());
            int cnt = obj.value("count").toInt();
            double freq = obj.value("freq").toDouble();
            int rssi = obj.value("rssi").toInt();

            QVector<uint16_t> pulses;
            QJsonArray arr = obj.value("pulses").toArray();
            for (const QJsonValue &v : arr) {
                pulses.append(static_cast<uint16_t>(v.toInt()));
            }
            emit capturedRawPacket(proto, isK, ser, btn, enc, cnt, freq, rssi, pulses);
        }
    }
}

void DeviceManager::onErrorOccurred(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;
    if (error == QSerialPort::ResourceError) {
        emit logMessage("Устройство было физически отключено от USB", true);
        disconnectPort();
    } else {
        emit logMessage(QString("Ошибка Serial: %1").arg(m_port->errorString()), true);
    }
}
