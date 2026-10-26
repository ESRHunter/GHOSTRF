#pragma once
#include <Arduino.h>
#include "config.h"

// =========================================================================
// ПЕРЕЧИСЛЕНИЯ И СОСТОЯНИЯ
// =========================================================================

enum AppState {
    MODE_MAIN_MENU,
    MODE_TX_REMOTE,
    MODE_SIGNAL_SUBMENU,
    MODE_SIGNAL_WAVEFORM,
    MODE_SIGNAL_EDITOR,
    MODE_RX_SNIFFER,
    MODE_RX_DETAILS,
    MODE_RAW_SNIFFER,
    MODE_RAW_DETAILS,
    MODE_FREQ_ANALYZER,
    MODE_SPECTRUM,
    MODE_CARRIER_GEN,
    MODE_24G_MENU,
    MODE_WIFI_SCAN,
    MODE_WIFI_DETAILS,
    MODE_WIFI_CHANNELS,
    MODE_WIFI_SNIFFER,
    MODE_BLE_SCAN,
    MODE_BLE_SPAM,
    MODE_WIFI_BEACON_SPAM,
    MODE_BATTERY_INFO,
    MODE_SETTINGS,
    MODE_SYS_INFO
};

enum ButtonEvent {
    BTN_EVT_NONE = 0,
    BTN_EVT_UP_CLICK,
    BTN_EVT_DOWN_CLICK,
    BTN_EVT_OK_CLICK,
    BTN_EVT_OK_HOLD
};

enum SignalType {
    SIG_FIXED = 0,
    SIG_RAW = 1,
    SIG_KLQ_GEN = 2,
    SIG_SMART_EV1527 = 3,
    SIG_CAME12 = 4,
    SIG_CAME24 = 5,
    SIG_NICE12 = 6,
    SIG_LINEAR10 = 7,
    SIG_SOMFY_RTS = 8,
    SIG_DOORHAN = 9
};

enum CarrierType {
    CARRIER_CW = 0,
    CARRIER_TONE_1KHZ = 1,
    CARRIER_JAM_NOISE = 2
};

// =========================================================================
// СТРУКТУРЫ ДАННЫХ
// =========================================================================

struct SavedSignal {
    char name[16];
    uint8_t type; // SIG_FIXED, SIG_RAW, SIG_KLQ_GEN, SIG_SMART_EV1527
    float freq;
    uint8_t repeats; // 1, 3, 5, 10
    char protoName[8]; // Came, Nice, KLQ, EV1527, PT2262, RAW
    // Фиксированный код:
    uint32_t code;
    uint8_t bitLength;
    uint16_t pulseLength;
    uint8_t protocol;
    // RAW импульсы:
    uint16_t rawCount;
    uint16_t rawPulses[MAX_RAW_PULSES];
    // Инфо KeeLoq / EV1527 Button:
    bool isKeeloq;
    uint32_t keeloqSerial;
    uint8_t keeloqBtn; // 1..5 for EV1527 (1=D0, 2=D1, 3=D2, 4=D3, 5=ALL)
    uint32_t keeloqEnc;
    // Виртуальный генератор KeeLoq:
    uint16_t klqCounter;
    uint64_t klqKey;
};

struct RawPacket {
    uint16_t pulses[MAX_RAW_PULSES];
    uint16_t pulseCount;
    char protoName[8];
    bool isKeeloq;
    uint32_t keeloqEnc;
    uint32_t keeloqSerial;
    uint8_t keeloqBtn;
    // Автоматически декодированный фиксированный код (EV1527 / Came / Nice):
    uint32_t decodedCode;
    uint8_t decodedBitLen;
    uint16_t decodedPulseLen;
    char extraInfo[16];
};

struct SpectrumBand {
    const char* name;
    float startFreq;
    float stepFreq;
    const char* labelRange;
};

