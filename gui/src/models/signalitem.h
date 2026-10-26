#pragma once

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdint>

enum SignalKind {
    SIG_KIND_FIXED = 0,
    SIG_KIND_RAW = 1,
    SIG_KIND_KLQ_GEN = 2,
    SIG_KIND_SMART_EV1527 = 3
};

class SignalItem {
public:
    int index = -1;
    QString name;
    int type = SIG_KIND_FIXED;
    double freq = 433.92;
    int repeats = 5;
    QString protoName = "EV1527";
    quint32 code = 0;
    int bitLen = 24;
    int pulseLen = 320;
    int protocol = 1;
    bool isKeeloq = false;
    quint32 keeloqSerial = 0;
    int keeloqBtn = 0;
    quint32 keeloqEnc = 0;
    int rawCount = 0;
    QVector<uint16_t> pulses;

    SignalItem() = default;

    QString typeName() const;
    QString codeHex() const;
    QString codeBin() const;
    QString keeloqSerialHex() const;
    QString keeloqEncHex() const;

    // Генерация массива импульсов микросекунд для осциллограммы
    QVector<uint16_t> toWaveformPulses() const;

    // Сериализация JSON
    QJsonObject toJson() const;
    static SignalItem fromJson(const QJsonObject &obj);

    // Экспорт / импорт формата Sub-GHz (.sub)
    QString toSubGhzFile() const;
    static SignalItem fromSubGhzFile(const QString &content, const QString &defaultName = "Imported");
};
