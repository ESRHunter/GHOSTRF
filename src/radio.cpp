#include "radio.h"
#include "config.h"
#include "keeloq.h"
#include "keeloq_analyzer.h"
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#include <RCSwitch.h>

static RCSwitch mySwitch;
static bool radioDetected = false;

void radio_init() {
    ELECHOUSE_cc1101.setSpiPin(CC_SCK, CC_MISO, CC_MOSI, CC_CS);
    ELECHOUSE_cc1101.setGDO0(CC_GDO0);

    ELECHOUSE_cc1101.Init();
    radioDetected = ELECHOUSE_cc1101.getCC1101();
    if (radioDetected) {
        Serial.println("[OK] CC1101 detected.");
        radio_modes_init();
    } else {
        Serial.println("[ERR] CC1101 not detected! Check SPI wiring (SCK, MISO, MOSI, CS).");
    }
}

bool radio_is_connected() {
    return ELECHOUSE_cc1101.getCC1101();
}

static bool rcSwitchEnabled = false;

void radio_apply_config(float freq, int paLevel) {
    radio_set_preset(radio_get_current_modulation(), freq, paLevel);
}

void radio_set_rx(float freq, int paLevel) {
    radio_apply_config(freq, paLevel);
    ELECHOUSE_cc1101.SetRx();
    if (radio_get_current_modulation() == MOD_OOK_ASK) {
        if (!rcSwitchEnabled) {
            mySwitch.enableReceive(CC_GDO0);
            rcSwitchEnabled = true;
        }
    } else {
        if (rcSwitchEnabled) {
            mySwitch.disableReceive();
            rcSwitchEnabled = false;
        }
    }
}

void radio_set_tx(float freq, int paLevel) {
    if (rcSwitchEnabled) {
        mySwitch.disableReceive();
        rcSwitchEnabled = false;
    }
    radio_apply_config(freq, paLevel);
    ELECHOUSE_cc1101.SetTx();
}

void radio_set_idle() {
    if (rcSwitchEnabled) {
        mySwitch.disableReceive();
        rcSwitchEnabled = false;
    }
    radio_safe_sidle();
}

void radio_transmit_fixed(uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol, float freq, int paLevel, uint8_t repeats) {
    if (protocol == 3) {
        radio_transmit_came12((uint16_t)code, freq, paLevel, repeats);
        return;
    }
    if (protocol == 4) {
        radio_transmit_came24(code, freq, paLevel, repeats);
        return;
    }
    if (protocol == 5) {
        radio_transmit_nice12((uint16_t)code, freq, paLevel, repeats);
        return;
    }
    if (protocol == 7) {
        radio_transmit_linear10((uint16_t)code, freq, paLevel, repeats);
        return;
    }
    if (protocol == 8) {
        radio_transmit_pt2240(code, freq, paLevel, repeats);
        return;
    }

    radio_set_tx(freq, paLevel);
    mySwitch.enableTransmit(CC_GDO0);
    mySwitch.setProtocol(protocol);
    mySwitch.setPulseLength(pulseLength);
    mySwitch.setRepeatTransmit(repeats);

    mySwitch.send(code, bitLength);
    mySwitch.disableTransmit();
    radio_set_idle();
}

static unsigned long lastCarrierToggle = 0;
static bool carrierLevel = false;

void radio_set_carrier(CarrierType mode, float freq, int paLevel) {
    radio_set_tx(freq, paLevel);
    pinMode(CC_GDO0, OUTPUT);
    carrierLevel = false;
    lastCarrierToggle = micros();

    if (mode == CARRIER_CW) {
        digitalWrite(CC_GDO0, HIGH);
    } else {
        digitalWrite(CC_GDO0, LOW);
    }
}

void radio_update_carrier_loop(CarrierType mode) {
    if (mode == CARRIER_TONE_1KHZ) {
        unsigned long now = micros();
        if (now - lastCarrierToggle >= 500) { // 1 kHz square tone (500us toggles)
            lastCarrierToggle = now;
            carrierLevel = !carrierLevel;
            digitalWrite(CC_GDO0, carrierLevel ? HIGH : LOW);
        }
    } else if (mode == CARRIER_JAM_NOISE) {
        unsigned long now = micros();
        if (now - lastCarrierToggle >= (unsigned long)random(120, 650)) {
            lastCarrierToggle = now;
            carrierLevel = !carrierLevel;
            digitalWrite(CC_GDO0, carrierLevel ? HIGH : LOW);
        }
    }
}

bool radio_check_rx(uint32_t &code, uint8_t &bitLen, uint16_t &pulseLen, uint8_t &proto) {
    if (mySwitch.available()) {
        code = mySwitch.getReceivedValue();
        bitLen = mySwitch.getReceivedBitlength();
        pulseLen = mySwitch.getReceivedDelay();
        proto = mySwitch.getReceivedProtocol();
        mySwitch.resetAvailable();

        if (code != 0) {
            if (rcSwitchEnabled) {
                mySwitch.disableReceive();
                rcSwitchEnabled = false;
            }
            return true;
        }
    }
    return false;
}

int radio_scan_rssi(float freq) {
    ELECHOUSE_cc1101.SetRx(freq);
    uint32_t start = micros();
    // Wait for CC1101 state machine to calibrate and enter RX (MARCSTATE 0x0D = 13)
    while (micros() - start < 1200) {
        if ((ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F) == 0x0D) {
            break;
        }
        delayMicroseconds(20);
    }
    // Dwell and peak-detect over ~250 microseconds (reliably catches OOK pulses)
    int maxRssi = -130;
    for (int i = 0; i < 4; i++) {
        int r = ELECHOUSE_cc1101.getRssi();
        if (r > maxRssi) maxRssi = r;
        delayMicroseconds(50);
    }
    return maxRssi;
}

int radio_get_rssi() {
    return ELECHOUSE_cc1101.getRssi();
}

void radio_debug_cc1101(float f, int &marc, int &pkt, int &rawRssi, int &dbm) {
    ELECHOUSE_cc1101.SetRx(f);
    delay(5);
    marc = (int)ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE);
    pkt = (int)ELECHOUSE_cc1101.SpiReadStatus(CC1101_PKTSTATUS);
    rawRssi = (int)ELECHOUSE_cc1101.SpiReadStatus(CC1101_RSSI);
    dbm = ELECHOUSE_cc1101.getRssi();
}

float radio_scan_fast_peak(int &outRssi) {
    float peakFreq = FAST_SCAN_FREQS[0];
    int maxR = -135;

    for (int i = 0; i < NUM_FAST_SCAN_FREQS; i++) {
        float f = FAST_SCAN_FREQS[i];
        int r = radio_scan_rssi(f);
        if (r > maxR) {
            maxR = r;
            peakFreq = f;
        }
    }
    outRssi = maxR;
    return peakFreq;
}

// =========================================================================
// RAW SNIFFER & PROTOCOL DECODER (KEELOQ, CAME, NICE, EV1527)
// =========================================================================

static volatile uint16_t isrPulses[MAX_RAW_PULSES];
static volatile uint16_t isrIndex = 0;
static volatile uint32_t isrLastEdge = 0;
static volatile bool isrPacketReady = false;
static volatile bool isrListening = false;

static void IRAM_ATTR isrRawEdge() {
    if (!isrListening || isrPacketReady) return;

    uint32_t now = micros();
    uint32_t diff = now - isrLastEdge;
    isrLastEdge = now;

    // Адаптивный фильтр высокочастотных шумов (110 мкс для OOK, 20 мкс для FSK/GFSK)
    if (diff < radio_get_glitch_filter()) return;

    // Пауза между посылками (4мс для FSK/GFSK, 7.5мс для OOK)
    uint32_t gapThreshold = (radio_get_glitch_filter() <= 30) ? 4000 : 7500;
    if (diff > gapThreshold) {
        if (isrIndex >= 24) {
            isrPacketReady = true;
        } else {
            isrIndex = 0; // Отсекаем одиночные случайные всплески
        }
        return;
    }

    if (isrIndex < MAX_RAW_PULSES) {
        isrPulses[isrIndex++] = (uint16_t)diff;
    } else {
        // Буфер заполнен
        isrPacketReady = true;
    }
}

static bool decodeKeeLoq(RawPacket &pkt) {
    // KeeLoq: Preamble + Header gap (2.2мс - 6.0мс) + 66 data bits (PWM ~400us base)
    int headerIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 66; i++) {
        if (pkt.pulses[i] >= 2200 && pkt.pulses[i] <= 6000) {
            headerIdx = i;
            break;
        }
    }
    if (headerIdx < 0) return false;

    uint64_t data = 0;
    int idx = headerIdx + 1;
    int bitsDecoded = 0;

    for (int bit = 0; bit < 66; bit++) {
        if (idx + 1 >= pkt.pulseCount) break;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;

        // PWM соотношение (2:1 или 1:2)
        if (t2 > t1) {
            data |= ((uint64_t)1 << bit);
        }
        bitsDecoded++;
    }

    if (bitsDecoded < 60) return false;

    pkt.keeloqEnc = (uint32_t)(data & 0xFFFFFFFF);
    pkt.keeloqSerial = (uint32_t)((data >> 32) & 0x0FFFFFFF);
    pkt.keeloqBtn = (uint8_t)((data >> 60) & 0x0F);
    return true;
}

static bool decodeEV1527(RawPacket &pkt, uint32_t &outCode, uint16_t &outPulseLen) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 48; i++) {
        if (pkt.pulses[i] >= 7000 && pkt.pulses[i] <= 16000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    uint32_t sumT = 0;

    for (int bit = 0; bit < 24; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 600 || total > 2400) return false;

        sumT += total;
        if (t1 > (t2 * 3) / 2) {
            code = (code << 1) | 1;
        } else if (t2 > (t1 * 3) / 2) {
            code = (code << 1) | 0;
        } else {
            return false;
        }
    }

    outCode = code;
    outPulseLen = (uint16_t)(sumT / 96); // 24 bits * 4T
    return true;
}

static bool decodeCame12(RawPacket &pkt, uint32_t &outCode) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 24; i++) {
        if (pkt.pulses[i] >= 9000 && pkt.pulses[i] <= 15000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    for (int bit = 0; bit < 12; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 650 || total > 1500) return false;

        if (t1 > (t2 * 4) / 3) {
            code = (code << 1) | 1;
        } else if (t2 > (t1 * 4) / 3) {
            code = (code << 1) | 0;
        } else {
            return false;
        }
    }
    outCode = code;
    return true;
}

static bool decodeCame24(RawPacket &pkt, uint32_t &outCode) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 48; i++) {
        if (pkt.pulses[i] >= 9000 && pkt.pulses[i] <= 15000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    for (int bit = 0; bit < 24; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 650 || total > 1500) return false;

        if (t1 > (t2 * 4) / 3) {
            code = (code << 1) | 1;
        } else if (t2 > (t1 * 4) / 3) {
            code = (code << 1) | 0;
        } else {
            return false;
        }
    }
    outCode = code;
    return true;
}

static bool decodeNice12(RawPacket &pkt, uint32_t &outCode) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 24; i++) {
        if (pkt.pulses[i] >= 18000 && pkt.pulses[i] <= 32000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    for (int bit = 0; bit < 12; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 1400 || total > 3200) return false;

        if (t1 > (t2 * 4) / 3) {
            code = (code << 1) | 1;
        } else if (t2 > (t1 * 4) / 3) {
            code = (code << 1) | 0;
        } else {
            return false;
        }
    }
    outCode = code;
    return true;
}

static bool decodeLinear10(RawPacket &pkt, uint32_t &outCode) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 20; i++) {
        if (pkt.pulses[i] >= 15000 && pkt.pulses[i] <= 28000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    for (int bit = 0; bit < 10; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 2200 || total > 3800) return false;

        code = (code << 1) | (t1 < t2 ? 1 : 0);
    }
    outCode = code;
    return true;
}

static bool decodePT2240(RawPacket &pkt, uint32_t &outCode, uint16_t &outPulseLen) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 48; i++) {
        if (pkt.pulses[i] >= 10000 && pkt.pulses[i] <= 18000) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    uint32_t sumT = 0;
    for (int bit = 0; bit < 24; bit++) {
        if (idx + 1 >= (int)pkt.pulseCount) return false;
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 1200 || total > 2600) return false;
        sumT += total;
        if (t1 > (t2 * 2)) {
            code = (code << 1) | 1;
        } else if (t2 > (t1 * 2)) {
            code = (code << 1) | 0;
        } else {
            return false;
        }
    }
    outCode = code;
    outPulseLen = (uint16_t)(sumT / 96);
    return true;
}

static bool decodeSomfyRTS(RawPacket &pkt, uint32_t &outCode) {
    int syncIdx = -1;
    for (int i = 0; i < (int)pkt.pulseCount - 50; i++) {
        if (pkt.pulses[i] >= 3800 && pkt.pulses[i] <= 6200) {
            syncIdx = i;
            break;
        }
    }
    if (syncIdx < 0) return false;

    int idx = syncIdx + 1;
    uint32_t code = 0;
    int bits = 0;
    for (int b = 0; b < 24 && idx + 1 < (int)pkt.pulseCount; b++) {
        uint16_t t1 = pkt.pulses[idx];
        uint16_t t2 = pkt.pulses[idx + 1];
        idx += 2;
        uint32_t total = t1 + t2;
        if (total < 800 || total > 1800) break;
        code = (code << 1) | (t1 > t2 ? 1 : 0);
        bits++;
    }
    if (bits < 16) return false;
    outCode = code;
    return true;
}

static void detectProtocol(RawPacket &pkt) {
    pkt.decodedCode = 0;
    pkt.decodedBitLen = 0;
    pkt.decodedPulseLen = 0;
    pkt.extraInfo[0] = '\0';

    if (decodeKeeLoq(pkt)) {
        pkt.isKeeloq = true;
        // Определение вариантов: DoorHan, An-Motors, KeeLoq HCS
        uint16_t hGap = 0;
        for (int i = 0; i < (int)pkt.pulseCount; i++) {
            if (pkt.pulses[i] >= 2200 && pkt.pulses[i] <= 6000) {
                hGap = pkt.pulses[i];
                break;
            }
        }
        if (hGap >= 3700 && hGap <= 4350 && pkt.pulseCount >= 115) {
            strncpy(pkt.protoName, "DoorHan", sizeof(pkt.protoName));
        } else if (hGap >= 4400 && hGap <= 5200) {
            strncpy(pkt.protoName, "AnMotor", sizeof(pkt.protoName));
        } else {
            strncpy(pkt.protoName, "KeeLoq", sizeof(pkt.protoName));
        }
        snprintf(pkt.extraInfo, sizeof(pkt.extraInfo), "SN:%07lX B:%d", (unsigned long)pkt.keeloqSerial, pkt.keeloqBtn);
        return;
    }

    pkt.isKeeloq = false;
    pkt.keeloqEnc = 0;
    pkt.keeloqSerial = 0;
    pkt.keeloqBtn = 0;

    uint32_t decCode = 0;
    uint16_t decPulse = 0;

    // 1. Came 24-bit (TWIN / Atomo)
    if (pkt.pulseCount >= 50 && decodeCame24(pkt, decCode)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 24;
        pkt.decodedPulseLen = 320;
        strncpy(pkt.protoName, "Came24", sizeof(pkt.protoName));
        return;
    }

    // 2. Came 12-bit (TOP432NA / TAM)
    if (decodeCame12(pkt, decCode)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 12;
        pkt.decodedPulseLen = 320;
        strncpy(pkt.protoName, "Came12", sizeof(pkt.protoName));
        return;
    }

    // 3. Nice FLO (12-bit fixed)
    if (decodeNice12(pkt, decCode)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 12;
        pkt.decodedPulseLen = 700;
        strncpy(pkt.protoName, "NiceFLO", sizeof(pkt.protoName));
        return;
    }

    // 4. Linear Multi-Code (10-bit DIP)
    if (decodeLinear10(pkt, decCode)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 10;
        pkt.decodedPulseLen = 1000;
        strncpy(pkt.protoName, "Linear", sizeof(pkt.protoName));
        return;
    }

    // 5. Somfy RTS (56-bit Manchester)
    if (decodeSomfyRTS(pkt, decCode)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 56;
        pkt.decodedPulseLen = 604;
        strncpy(pkt.protoName, "Somfy", sizeof(pkt.protoName));
        return;
    }

    // 6. EV1527 (24-bit Learning Code) & PT2262 (Tri-State)
    if (decodeEV1527(pkt, decCode, decPulse)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 24;
        pkt.decodedPulseLen = decPulse;

        char triStr[16];
        if (radio_decode_pt2262_tristate(decCode, 24, triStr)) {
            strncpy(pkt.protoName, "PT2262", sizeof(pkt.protoName));
            strncpy(pkt.extraInfo, triStr, sizeof(pkt.extraInfo) - 1);
        } else {
            strncpy(pkt.protoName, "EV1527", sizeof(pkt.protoName));
            uint8_t btn = decCode & 0x0F;
            snprintf(pkt.extraInfo, sizeof(pkt.extraInfo), "ID:%05lX B:%d", (unsigned long)(decCode >> 4), btn);
        }
        return;
    }

    // 7. Princeton PT2240 (24-bit)
    if (decodePT2240(pkt, decCode, decPulse)) {
        pkt.decodedCode = decCode;
        pkt.decodedBitLen = 24;
        pkt.decodedPulseLen = decPulse;
        strncpy(pkt.protoName, "PT2240", sizeof(pkt.protoName));
        return;
    }

    // 8. Sensor Telemetry (Oregon Scientific v2/v3, TPMS Toyota, Ford, Schrader)
    SensorTelemetryData telem;
    if (telemetry_process_pulses(pkt.pulses, pkt.pulseCount, telem)) {
        pkt.decodedCode = telem.sensorId;
        pkt.decodedBitLen = (telem.protocol == SENSOR_PROTO_TPMS_TOYOTA) ? 72 :
                            (telem.protocol == SENSOR_PROTO_TPMS_FORD) ? 64 : 68;
        pkt.decodedPulseLen = (telem.protocol == SENSOR_PROTO_OREGON_V2 || telem.protocol == SENSOR_PROTO_OREGON_V3) ? 488 : 52;
        strncpy(pkt.protoName, (telem.protocol == SENSOR_PROTO_OREGON_V3) ? "OS_v3" :
                               (telem.protocol == SENSOR_PROTO_OREGON_V2) ? "OS_v2" :
                               (telem.protocol == SENSOR_PROTO_TPMS_TOYOTA) ? "TPMS_Toy" :
                               (telem.protocol == SENSOR_PROTO_TPMS_FORD) ? "TPMS_Ford" : "TPMS", sizeof(pkt.protoName) - 1);
        pkt.protoName[sizeof(pkt.protoName) - 1] = '\0';
        if (telem.pressureKpa_x10 > 0) {
            snprintf(pkt.extraInfo, sizeof(pkt.extraInfo), "%d.%dkPa %dC", telem.pressureKpa_x10 / 10, telem.pressureKpa_x10 % 10, telem.temperatureC_x10 / 10);
        } else {
            int16_t t = telem.temperatureC_x10;
            const char* sign = (t < 0 && t > -10) ? "-" : "";
            snprintf(pkt.extraInfo, sizeof(pkt.extraInfo), "%s%d.%dC %d%%", sign, t / 10, abs(t % 10), telem.humidityPct);
        }
        return;
    }

    // Fallback эвристика по синхропаузе
    uint32_t syncLen = 0;
    for (int i = 0; i < pkt.pulseCount; i++) {
        if (pkt.pulses[i] >= 7500) {
            syncLen = pkt.pulses[i];
            break;
        }
    }

    if (syncLen >= 20000 && syncLen <= 32000) {
        strncpy(pkt.protoName, "NiceFLO", sizeof(pkt.protoName));
    } else if (syncLen >= 9000 && syncLen <= 17000) {
        if (pkt.pulseCount >= 48) {
            strncpy(pkt.protoName, "Came24", sizeof(pkt.protoName));
        } else if (pkt.pulseCount >= 24) {
            strncpy(pkt.protoName, "Came12", sizeof(pkt.protoName));
        } else {
            strncpy(pkt.protoName, "EV1527", sizeof(pkt.protoName));
        }
    } else {
        strncpy(pkt.protoName, "RAW", sizeof(pkt.protoName));
    }
}

void radio_start_raw_sniffer(float freq, int paLevel, int squelchDbm) {
    radio_stop_raw_sniffer();
    radio_apply_config(freq, paLevel);
    ELECHOUSE_cc1101.SetRx();

    pinMode(CC_GDO0, INPUT);
    delay(50); // Время установления AGC
    isrIndex = 0;
    isrPacketReady = false;
    isrLastEdge = micros();
    isrListening = true;
    attachInterrupt(digitalPinToInterrupt(CC_GDO0), isrRawEdge, CHANGE);
}

void radio_stop_raw_sniffer() {
    isrListening = false;
    detachInterrupt(digitalPinToInterrupt(CC_GDO0));
    radio_set_idle();
}

bool radio_check_raw(RawPacket &outPacket, int squelchDbm) {
    if (!isrListening) return false;

    // Окончание посылки по таймауту тишины (> 12 мс после последнего фронта)
    if (!isrPacketReady && isrIndex >= 24 && (micros() - isrLastEdge > 12000)) {
        isrPacketReady = true;
    }

    if (isrPacketReady) {
        // 1. Проверяем порог мощности RSSI шумоподавителя (Squelch)
        int rssi = ELECHOUSE_cc1101.getRssi();
        if (rssi < squelchDbm) {
            // Уровень ниже порога — сбрасываем эфирный шум
            isrIndex = 0;
            isrPacketReady = false;
            isrListening = true;
            return false;
        }

        // 2. Проверяем наличие синхропаузы (пульты ворот/авто имеют паузу 1.8мс..15мс, FSK телеметрия использует более короткие паузы)
        bool isFskMode = (radio_get_current_modulation() != MOD_OOK_ASK);
        bool hasPreambleOrSync = isFskMode;
        if (!hasPreambleOrSync) {
            for (int i = 0; i < isrIndex; i++) {
                if (isrPulses[i] >= 1800 && isrPulses[i] <= 15000) {
                    hasPreambleOrSync = true;
                    break;
                }
            }
        }

        // Если импульсов много (>45) или есть синхропауза — это реальный пакет
        if (!hasPreambleOrSync && isrIndex < 45) {
            isrIndex = 0;
            isrPacketReady = false;
            isrListening = true;
            return false;
        }

        // Пакет валиден!
        isrListening = false;
        detachInterrupt(digitalPinToInterrupt(CC_GDO0));

        outPacket.pulseCount = isrIndex;
        for (int i = 0; i < isrIndex; i++) {
            outPacket.pulses[i] = isrPulses[i];
        }

        detectProtocol(outPacket);
        Serial.printf("[RAW] Captured %d pulses, proto=%s (RSSI: %d dBm)\n",
                      outPacket.pulseCount, outPacket.protoName, rssi);

        isrIndex = 0;
        isrPacketReady = false;
        return true;
    }
    return false;
}

bool radio_check_telemetry(SensorTelemetryData &outData) {
    if (!isrListening) return false;

    uint32_t gapTimeout = (radio_get_glitch_filter() <= 30) ? 6000 : 12000;
    if (!isrPacketReady && isrIndex >= 24 && (micros() - isrLastEdge > gapTimeout)) {
        isrPacketReady = true;
    }

    if (isrPacketReady) {
        uint16_t count = isrIndex;
        if (count < 24) {
            isrIndex = 0;
            isrPacketReady = false;
            isrListening = true;
            return false;
        }

        uint16_t pulsesCopy[MAX_RAW_PULSES];
        for (uint16_t i = 0; i < count; i++) {
            pulsesCopy[i] = isrPulses[i];
        }

        isrIndex = 0;
        isrPacketReady = false;
        isrListening = true;

        return telemetry_process_pulses(pulsesCopy, count, outData);
    }
    return false;
}

void radio_start_telemetry_sniffer(RfModulation mod, float freqMhz, int paLevel) {
    radio_stop_raw_sniffer();
    radio_set_preset(mod, freqMhz, paLevel);
    radio_start_raw_sniffer(freqMhz, paLevel, -95);
}

#include "soc/gpio_struct.h"

void radio_transmit_raw_pulses(const uint16_t* pulses, uint16_t count, float freq, int paLevel, uint8_t repeats) {
    Serial.printf("[TX-RAW] Transmitting %d pulses (%d repeats) on %.2f MHz (PA: %d dBm)\n",
                  count, repeats, freq, paLevel);
    radio_stop_raw_sniffer();
    radio_set_tx(freq, paLevel);
    pinMode(CC_GDO0, OUTPUT);

    for (int rep = 0; rep < repeats; rep++) {
        bool level = HIGH;
        noInterrupts();
        for (int i = 0; i < count; i++) {
            if (level) {
                GPIO.out_w1ts.val = (1UL << CC_GDO0);
            } else {
                GPIO.out_w1tc.val = (1UL << CC_GDO0);
            }
            delayMicroseconds(pulses[i]);
            level = !level;
        }
        GPIO.out_w1tc.val = (1UL << CC_GDO0);
        interrupts();
        delay(14);
    }
    radio_set_idle();
}

bool radio_decode_pt2262_tristate(uint32_t code, uint8_t bitLen, char* outTriStr) {
    if (bitLen != 24 || outTriStr == nullptr) return false;
    for (int i = 0; i < 12; i++) {
        uint8_t pair = (code >> ((11 - i) * 2)) & 0b11;
        if (pair == 0b00) {
            outTriStr[i] = '0';
        } else if (pair == 0b11) {
            outTriStr[i] = '1';
        } else if (pair == 0b01) {
            outTriStr[i] = 'F';
        } else {
            return false; // Not pure Tri-State
        }
    }
    outTriStr[12] = '\0';
    return true;
}

void radio_transmit_ev1527(uint32_t address20, uint8_t buttonIdx, float freq, int paLevel, uint8_t repeats) {
    uint8_t btnBits = 0x01;
    if (buttonIdx == 1) btnBits = 0x01; // D0
    else if (buttonIdx == 2) btnBits = 0x02; // D1
    else if (buttonIdx == 3) btnBits = 0x04; // D2
    else if (buttonIdx == 4) btnBits = 0x08; // D3
    else if (buttonIdx == 5) btnBits = 0x0F; // ALL
    uint32_t finalCode = ((address20 & 0xFFFFF) << 4) | (btnBits & 0x0F);
    radio_transmit_fixed(finalCode, 24, 320, 1, freq, paLevel, repeats);
}

void radio_transmit_came12(uint16_t code12, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[26];
    pulses[0] = 320;
    pulses[1] = 11520; // Sync
    int pIdx = 2;
    for (int b = 11; b >= 0; b--) {
        bool bit = (code12 >> b) & 1;
        pulses[pIdx++] = bit ? 640 : 320;
        pulses[pIdx++] = bit ? 320 : 640;
    }
    radio_transmit_raw_pulses(pulses, 26, freq, paLevel, repeats);
}

void radio_transmit_nice12(uint16_t code12, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[26];
    pulses[0] = 700;
    pulses[1] = 24000; // Sync
    int pIdx = 2;
    for (int b = 11; b >= 0; b--) {
        bool bit = (code12 >> b) & 1;
        pulses[pIdx++] = bit ? 1400 : 700;
        pulses[pIdx++] = bit ? 700 : 1400;
    }
    radio_transmit_raw_pulses(pulses, 26, freq, paLevel, repeats);
}

void radio_transmit_came24(uint32_t code24, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[50];
    pulses[0] = 320;
    pulses[1] = 11520; // Sync
    int pIdx = 2;
    for (int b = 23; b >= 0; b--) {
        bool bit = (code24 >> b) & 1;
        pulses[pIdx++] = bit ? 640 : 320;
        pulses[pIdx++] = bit ? 320 : 640;
    }
    radio_transmit_raw_pulses(pulses, 50, freq, paLevel, repeats);
}

void radio_transmit_linear10(uint16_t code10, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[22];
    pulses[0] = 1000;
    pulses[1] = 20000; // Sync
    int pIdx = 2;
    for (int b = 9; b >= 0; b--) {
        bool bit = (code10 >> b) & 1;
        pulses[pIdx++] = bit ? 1000 : 2000;
        pulses[pIdx++] = bit ? 2000 : 1000;
    }
    radio_transmit_raw_pulses(pulses, 22, freq, paLevel, repeats);
}

void radio_transmit_somfy_rts(uint32_t address24, uint16_t rollingCounter, uint8_t buttonCode, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[120];
    pulses[0] = 2450; // HW Sync High
    pulses[1] = 4850; // HW Sync Low
    pulses[2] = 4550; // SW Sync
    int pIdx = 3;
    uint32_t full = ((address24 & 0xFFFFFF) << 8) | (buttonCode & 0x0F);
    for (int b = 31; b >= 0; b--) {
        bool bit = (full >> b) & 1;
        pulses[pIdx++] = bit ? 1208 : 604;
        pulses[pIdx++] = bit ? 604 : 1208;
    }
    radio_transmit_raw_pulses(pulses, pIdx, freq, paLevel, repeats);
}

void radio_transmit_pt2240(uint32_t code24, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[50];
    pulses[0] = 450;
    pulses[1] = 14000; // Sync
    int pIdx = 2;
    for (int b = 23; b >= 0; b--) {
        bool bit = (code24 >> b) & 1;
        pulses[pIdx++] = bit ? 1350 : 450;
        pulses[pIdx++] = bit ? 450 : 1350;
    }
    radio_transmit_raw_pulses(pulses, 50, freq, paLevel, repeats);
}

void radio_transmit_doorhan(uint32_t serial, uint16_t counter, uint8_t button, float freq, int paLevel, uint8_t repeats) {
    uint16_t pulses[160];
    uint16_t count = keeloq_emulate_next(serial, button, counter, KLQ_KEY_DOORHAN, pulses);
    if (count > 0) {
        radio_transmit_raw_pulses(pulses, count, freq, paLevel, repeats);
    }
}
