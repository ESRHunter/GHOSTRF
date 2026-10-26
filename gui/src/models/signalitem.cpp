#include "signalitem.h"
#include <QStringList>
#include <QRegularExpression>
#include <cmath>

QString SignalItem::typeName() const {
    switch (type) {
        case SIG_KIND_RAW:
            return isKeeloq ? "KeeLoq RAW" : "RAW Импульсы";
        case SIG_KIND_KLQ_GEN:
            return "Виртуальный KeeLoq";
        case SIG_KIND_SMART_EV1527:
            return "EV1527 Smart Remote";
        case SIG_KIND_FIXED:
        default:
            return QString("%1 (Фиксированный)").arg(protoName.isEmpty() ? "EV1527" : protoName);
    }
}

QString SignalItem::codeHex() const {
    if (bitLen <= 16) {
        return QString("0x%1").arg(code, 4, 16, QChar('0')).toUpper();
    } else if (bitLen <= 24) {
        return QString("0x%1").arg(code, 6, 16, QChar('0')).toUpper();
    } else {
        return QString("0x%1").arg(code, 8, 16, QChar('0')).toUpper();
    }
}

QString SignalItem::codeBin() const {
    QString bin = QString::number(code, 2).rightJustified(bitLen > 0 ? bitLen : 24, '0');
    QString formatted;
    for (int i = 0; i < bin.length(); i++) {
        if (i > 0 && (bin.length() - i) % 4 == 0) {
            formatted += ' ';
        }
        formatted += bin[i];
    }
    return formatted;
}

QString SignalItem::keeloqSerialHex() const {
    return QString("0x%1").arg(keeloqSerial, 7, 16, QChar('0')).toUpper();
}

QString SignalItem::keeloqEncHex() const {
    return QString("0x%1").arg(keeloqEnc, 8, 16, QChar('0')).toUpper();
}

QVector<uint16_t> SignalItem::toWaveformPulses() const {
    if (!pulses.isEmpty()) {
        return pulses;
    }

    QVector<uint16_t> synthetic;
    int te = (pulseLen > 0) ? pulseLen : 320;

    // Синтезируем импульсы для фиксированных протоколов
    if (protoName.contains("Came", Qt::CaseInsensitive)) {
        // CAME 12-bit
        synthetic.append(te);
        synthetic.append(te * 36);
        for (int i = 11; i >= 0; i--) {
            bool bit = (code >> i) & 1;
            if (bit) {
                synthetic.append(te * 2);
                synthetic.append(te);
            } else {
                synthetic.append(te);
                synthetic.append(te * 2);
            }
        }
    } else if (protoName.contains("Nice", Qt::CaseInsensitive)) {
        // NICE 12-bit
        synthetic.append(te * 2);
        synthetic.append(te * 24);
        for (int i = 11; i >= 0; i--) {
            bool bit = (code >> i) & 1;
            if (bit) {
                synthetic.append(te * 2);
                synthetic.append(te);
            } else {
                synthetic.append(te);
                synthetic.append(te * 2);
            }
        }
    } else {
        // Стандартный протокол RC-Switch 1 / EV1527 / PT2262:
        // Sync: 1 High, 31 Low
        synthetic.append(te * 1);
        synthetic.append(te * 31);

        int totalBits = (bitLen > 0) ? bitLen : 24;
        for (int i = totalBits - 1; i >= 0; i--) {
            bool bit = (code >> i) & 1;
            if (bit) {
                synthetic.append(te * 3);
                synthetic.append(te * 1);
            } else {
                synthetic.append(te * 1);
                synthetic.append(te * 3);
            }
        }
    }

    return synthetic;
}

QJsonObject SignalItem::toJson() const {
    QJsonObject obj;
    obj["index"] = index;
    obj["name"] = name;
    obj["type"] = type;
    obj["freq"] = freq;
    obj["repeats"] = repeats;
    obj["proto"] = protoName;
    obj["code"] = static_cast<double>(code);
    obj["bitLen"] = bitLen;
    obj["pulseLen"] = pulseLen;
    obj["protocol"] = protocol;
    obj["isKeeloq"] = isKeeloq;
    obj["keeloqSerial"] = static_cast<double>(keeloqSerial);
    obj["keeloqBtn"] = keeloqBtn;
    obj["keeloqEnc"] = static_cast<double>(keeloqEnc);
    obj["rawCount"] = pulses.size();

    QJsonArray pArr;
    for (uint16_t p : pulses) {
        pArr.append(p);
    }
    obj["pulses"] = pArr;

    return obj;
}

SignalItem SignalItem::fromJson(const QJsonObject &obj) {
    SignalItem item;
    item.index = obj.value("index").toInt(-1);
    item.name = obj.value("name").toString("Unnamed");
    item.type = obj.value("type").toInt(SIG_KIND_FIXED);
    item.freq = obj.value("freq").toDouble(433.92);
    item.repeats = obj.value("repeats").toInt(5);
    item.protoName = obj.value("proto").toString("EV1527");
    item.code = static_cast<quint32>(obj.value("code").toVariant().toULongLong());
    item.bitLen = obj.value("bitLen").toInt(24);
    item.pulseLen = obj.value("pulseLen").toInt(320);
    item.protocol = obj.value("protocol").toInt(1);
    item.isKeeloq = obj.value("isKeeloq").toBool(false);
    item.keeloqSerial = static_cast<quint32>(obj.value("keeloqSerial").toVariant().toULongLong());
    item.keeloqBtn = obj.value("keeloqBtn").toInt(0);
    item.keeloqEnc = static_cast<quint32>(obj.value("keeloqEnc").toVariant().toULongLong());

    QJsonArray pArr = obj.value("pulses").toArray();
    for (const QJsonValue &v : pArr) {
        item.pulses.append(static_cast<uint16_t>(v.toInt()));
    }
    item.rawCount = item.pulses.size();

    return item;
}

QString SignalItem::toSubGhzFile() const {
    QString out;
    out += "Filetype: Flipper SubGhz Key File\n";
    out += "Version: 1\n";
    quint64 hz = static_cast<quint64>(std::round(freq * 1000000.0));
    out += QString("Frequency: %1\n").arg(hz);
    out += "Preset: FuriHalSubGhzPresetOok650Async\n";

    if (type == SIG_KIND_RAW || !pulses.isEmpty()) {
        out += "Protocol: RAW\n";
        out += "RAW_Data:";
        for (int i = 0; i < pulses.size(); i++) {
            if (i % 24 == 0) out += "\n";
            // Положительное число = HIGH, отрицательное = LOW
            int sign = (i % 2 == 0) ? 1 : -1;
            out += QString(" %1").arg(sign * pulses[i]);
        }
        out += "\n";
    } else {
        out += QString("Protocol: %1\n").arg(protoName.isEmpty() ? "Princeton" : protoName);
        out += QString("Bit: %1\n").arg(bitLen > 0 ? bitLen : 24);
        out += QString("Key: %1\n").arg(code, 16, 16, QChar('0')).toUpper();
        out += QString("TE: %1\n").arg(pulseLen > 0 ? pulseLen : 320);
        out += QString("Repeat: %1\n").arg(repeats > 0 ? repeats : 5);
    }

    return out;
}

SignalItem SignalItem::fromSubGhzFile(const QString &content, const QString &defaultName) {
    SignalItem item;
    item.name = defaultName;
    item.type = SIG_KIND_RAW;
    item.freq = 433.92;
    item.repeats = 5;

    QStringList lines = content.split('\n');
    QString currentProtocol = "RAW";

    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith("#")) continue;

        if (line.startsWith("Frequency:")) {
            quint64 hz = line.section(':', 1).trimmed().toULongLong();
            if (hz > 0) item.freq = static_cast<double>(hz) / 1000000.0;
        } else if (line.startsWith("Protocol:")) {
            currentProtocol = line.section(':', 1).trimmed();
            item.protoName = currentProtocol;
            if (currentProtocol.compare("RAW", Qt::CaseInsensitive) == 0) {
                item.type = SIG_KIND_RAW;
            } else {
                item.type = SIG_KIND_FIXED;
            }
        } else if (line.startsWith("Bit:")) {
            item.bitLen = line.section(':', 1).trimmed().toInt();
        } else if (line.startsWith("Key:")) {
            QString keyStr = line.section(':', 1).trimmed().remove(' ');
            bool ok = false;
            item.code = static_cast<quint32>(keyStr.toULongLong(&ok, 16));
        } else if (line.startsWith("TE:")) {
            item.pulseLen = line.section(':', 1).trimmed().toInt();
        } else if (line.startsWith("Repeat:")) {
            item.repeats = line.section(':', 1).trimmed().toInt();
        } else if (line.startsWith("RAW_Data:")) {
            item.type = SIG_KIND_RAW;
            QString dataPart = line.section(':', 1).trimmed();
            QStringList tokens = dataPart.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            for (const QString &tok : tokens) {
                int val = tok.toInt();
                item.pulses.append(static_cast<uint16_t>(std::abs(val)));
            }
        } else if (item.type == SIG_KIND_RAW && !line.contains(':')) {
            // Продолжение RAW_Data на последующих строках
            QStringList tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            for (const QString &tok : tokens) {
                int val = tok.toInt();
                if (val != 0) {
                    item.pulses.append(static_cast<uint16_t>(std::abs(val)));
                }
            }
        }
    }

    item.rawCount = item.pulses.size();
    return item;
}
