#include "storage.h"
#include <Preferences.h>

static Preferences prefs;

void storage_init() {
    // Preferences открываются по требованию
}

void storage_load_all(int &freqIdx, int &paIdx, int &squelchIdx, int &spectrumBandIdx, SavedSignal* sigList, int &sigCount) {
    prefs.begin("ghostrf", false);
    if (!prefs.isKey("count")) {
        // Namespace "ghostrf" is empty. Check old "rf_hub" namespace
        prefs.end();
        prefs.begin("rf_hub", false);
        if (prefs.isKey("count")) {
            freqIdx = prefs.getInt("freq_idx", 0);
            if (freqIdx < 0 || freqIdx >= NUM_FREQ_PRESETS) freqIdx = 0;
            paIdx = prefs.getInt("pa_idx", 3);
            if (paIdx < 0 || paIdx >= NUM_PA_LEVELS) paIdx = 3;
            squelchIdx = prefs.getInt("squelch", 1);
            if (squelchIdx < 0 || squelchIdx >= NUM_SQUELCH_LEVELS) squelchIdx = 1;
            spectrumBandIdx = prefs.getInt("spec_band", 0);
            if (spectrumBandIdx < 0 || spectrumBandIdx >= NUM_SPECTRUM_BANDS) spectrumBandIdx = 0;
            sigCount = prefs.getInt("count", 0);
            if (sigCount < 0) sigCount = 0;
            if (sigCount > MAX_SAVED_SIGNALS) sigCount = MAX_SAVED_SIGNALS;
            for (int i = 0; i < sigCount; i++) {
                char key[16];
                snprintf(key, sizeof(key), "s_%d", i);
                prefs.getBytes(key, &sigList[i], sizeof(SavedSignal));
                if (sigList[i].repeats == 0 || sigList[i].repeats > 20) sigList[i].repeats = 5;
                if (sigList[i].protoName[0] == '\0') {
                    strncpy(sigList[i].protoName, sigList[i].isKeeloq ? "KeeLoq" : (sigList[i].type == SIG_FIXED ? "EV1527" : "RAW"), 7);
                }
            }
            prefs.end();
            storage_save_settings(freqIdx, paIdx, squelchIdx, spectrumBandIdx);
            storage_save_signals(sigList, sigCount);
            Serial.printf("[NVS] Migrated %d signals from rf_hub to ghostrf\n", sigCount);
            return;
        }
        prefs.end();
        prefs.begin("ghostrf", false);
    }

    freqIdx = prefs.getInt("freq_idx", 0);
    if (freqIdx < 0 || freqIdx >= NUM_FREQ_PRESETS) freqIdx = 0;

    paIdx = prefs.getInt("pa_idx", 3);
    if (paIdx < 0 || paIdx >= NUM_PA_LEVELS) paIdx = 3;

    squelchIdx = prefs.getInt("squelch", 1); // По умолчанию -78 dBm (Mid)
    if (squelchIdx < 0 || squelchIdx >= NUM_SQUELCH_LEVELS) squelchIdx = 1;

    spectrumBandIdx = prefs.getInt("spec_band", 0);
    if (spectrumBandIdx < 0 || spectrumBandIdx >= NUM_SPECTRUM_BANDS) spectrumBandIdx = 0;

    sigCount = prefs.getInt("count", 0);
    if (sigCount < 0) sigCount = 0;
    if (sigCount > MAX_SAVED_SIGNALS) sigCount = MAX_SAVED_SIGNALS;

    for (int i = 0; i < sigCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "s_%d", i);
        prefs.getBytes(key, &sigList[i], sizeof(SavedSignal));
        if (sigList[i].repeats == 0 || sigList[i].repeats > 20) {
            sigList[i].repeats = 5;
        }
        if (sigList[i].protoName[0] == '\0') {
            strncpy(sigList[i].protoName, sigList[i].isKeeloq ? "KeeLoq" : (sigList[i].type == SIG_FIXED ? "EV1527" : "RAW"), 7);
        }
    }

    prefs.end();

    // Если память пуста, добавим демо-команды умного дома
    if (sigCount == 0) {
        SavedSignal &s1 = sigList[0];
        snprintf(s1.name, 16, "Smart Relay 1");
        s1.type = SIG_SMART_EV1527;
        s1.freq = 433.92f;
        s1.code = 0x547A1; // 20-bit address
        s1.bitLength = 24;
        s1.pulseLength = 320;
        s1.protocol = 1;
        s1.repeats = 5;
        s1.keeloqBtn = 1; // Btn 1 active
        strncpy(s1.protoName, "EV1527", 7);

        SavedSignal &s2 = sigList[1];
        snprintf(s2.name, 16, "Socket 1 ON");
        s2.type = SIG_FIXED;
        s2.freq = 433.92f;
        s2.code = 0x547A11;
        s2.bitLength = 24;
        s2.pulseLength = 320;
        s2.protocol = 1;
        s2.repeats = 5;
        strncpy(s2.protoName, "PT2262", 7);

        SavedSignal &s3 = sigList[2];
        snprintf(s3.name, 16, "Socket 1 OFF");
        s3.type = SIG_FIXED;
        s3.freq = 433.92f;
        s3.code = 0x547A12;
        s3.bitLength = 24;
        s3.pulseLength = 320;
        s3.protocol = 1;
        s3.repeats = 5;
        strncpy(s3.protoName, "PT2262", 7);

        sigCount = 3;
        storage_save_signals(sigList, sigCount);
    }

    Serial.printf("[NVS] Loaded: %d signals, freq=%s, PA=%s, Squelch=%s\n",
                  sigCount, FREQ_NAMES[freqIdx], PA_NAMES[paIdx], SQUELCH_NAMES[squelchIdx]);
}

void storage_save_settings(int freqIdx, int paIdx, int squelchIdx, int spectrumBandIdx) {
    prefs.begin("ghostrf", false);
    prefs.putInt("freq_idx", freqIdx);
    prefs.putInt("pa_idx", paIdx);
    prefs.putInt("squelch", squelchIdx);
    prefs.putInt("spec_band", spectrumBandIdx);
    prefs.end();
    Serial.printf("[NVS] Settings saved: freq=%s, PA=%s, Squelch=%s\n",
                  FREQ_NAMES[freqIdx], PA_NAMES[paIdx], SQUELCH_NAMES[squelchIdx]);
}

void storage_save_signals(const SavedSignal* sigList, int sigCount) {
    prefs.begin("ghostrf", false);
    prefs.putInt("count", sigCount);
    for (int i = 0; i < sigCount; i++) {
        char key[16];
        snprintf(key, sizeof(key), "s_%d", i);
        prefs.putBytes(key, &sigList[i], sizeof(SavedSignal));
    }
    for (int i = sigCount; i < MAX_SAVED_SIGNALS; i++) {
        char key[16];
        snprintf(key, sizeof(key), "s_%d", i);
        prefs.remove(key);
    }
    prefs.end();
}

bool storage_delete_signal(int index, SavedSignal* sigList, int &sigCount) {
    if (index < 0 || index >= sigCount) return false;
    for (int i = index; i < sigCount - 1; i++) {
        sigList[i] = sigList[i + 1];
    }
    sigCount--;
    storage_save_signals(sigList, sigCount);
    Serial.printf("[NVS] Deleted signal at index %d, remaining: %d\n", index, sigCount);
    return true;
}

bool storage_add_fixed(const char* name, uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol, float freq, SavedSignal* sigList, int &sigCount) {
    int idx = (sigCount < MAX_SAVED_SIGNALS) ? sigCount : (MAX_SAVED_SIGNALS - 1);
    SavedSignal &sig = sigList[idx];

    strncpy(sig.name, name, 15);
    sig.name[15] = '\0';
    sig.type = SIG_FIXED;
    sig.freq = freq;
    sig.code = code;
    sig.bitLength = bitLength;
    sig.pulseLength = pulseLength;
    sig.protocol = protocol;
    sig.rawCount = 0;
    sig.isKeeloq = false;
    sig.repeats = 5;
    strncpy(sig.protoName, "EV1527", 7);

    if (sigCount < MAX_SAVED_SIGNALS) sigCount++;
    storage_save_signals(sigList, sigCount);
    return true;
}

bool storage_add_raw(const char* name, const RawPacket &rawPkt, float freq, SavedSignal* sigList, int &sigCount) {
    int idx = (sigCount < MAX_SAVED_SIGNALS) ? sigCount : (MAX_SAVED_SIGNALS - 1);
    SavedSignal &sig = sigList[idx];

    strncpy(sig.name, name, 15);
    sig.name[15] = '\0';
    sig.type = SIG_RAW;
    sig.freq = freq;
    sig.code = rawPkt.decodedCode;
    sig.bitLength = rawPkt.decodedBitLen;
    sig.pulseLength = rawPkt.decodedPulseLen;
    sig.rawCount = (rawPkt.pulseCount > MAX_RAW_PULSES) ? MAX_RAW_PULSES : rawPkt.pulseCount;

    for (int i = 0; i < sig.rawCount; i++) {
        sig.rawPulses[i] = rawPkt.pulses[i];
    }

    sig.isKeeloq = rawPkt.isKeeloq;
    sig.keeloqSerial = rawPkt.keeloqSerial;
    sig.keeloqBtn = rawPkt.keeloqBtn;
    sig.keeloqEnc = rawPkt.keeloqEnc;
    sig.repeats = 5;
    strncpy(sig.protoName, rawPkt.protoName, 7);

    // Умное распознавание типа для идеальной генерации
    if (strcmp(rawPkt.protoName, "Came12") == 0) sig.type = SIG_CAME12;
    else if (strcmp(rawPkt.protoName, "Came24") == 0) sig.type = SIG_CAME24;
    else if (strcmp(rawPkt.protoName, "NiceFLO") == 0) sig.type = SIG_NICE12;
    else if (strcmp(rawPkt.protoName, "Linear") == 0) sig.type = SIG_LINEAR10;
    else if (strcmp(rawPkt.protoName, "Somfy") == 0) sig.type = SIG_SOMFY_RTS;
    else if (strcmp(rawPkt.protoName, "DoorHan") == 0) sig.type = SIG_DOORHAN;
    else if (strcmp(rawPkt.protoName, "EV1527") == 0) sig.type = SIG_SMART_EV1527;
    else if (rawPkt.decodedCode != 0) sig.type = SIG_FIXED;

    if (sigCount < MAX_SAVED_SIGNALS) sigCount++;
    storage_save_signals(sigList, sigCount);
    return true;
}

void storage_clear_signals(int &sigCount) {
    prefs.begin("ghostrf", false);
    sigCount = 0;
    prefs.putInt("count", 0);
    prefs.end();
}

bool storage_add_smart_remote(const char* name, uint32_t addr20, float freq, SavedSignal* sigList, int &sigCount) {
    int idx = (sigCount < MAX_SAVED_SIGNALS) ? sigCount : (MAX_SAVED_SIGNALS - 1);
    SavedSignal &sig = sigList[idx];

    strncpy(sig.name, name, 15);
    sig.name[15] = '\0';
    sig.type = SIG_SMART_EV1527;
    sig.freq = freq;
    sig.code = addr20 & 0xFFFFF; // 20-bit address
    sig.bitLength = 24;
    sig.pulseLength = 320;
    sig.protocol = 1;
    sig.rawCount = 0;
    sig.isKeeloq = false;
    sig.keeloqBtn = 1; // Default Button 1
    sig.repeats = 5;
    strncpy(sig.protoName, "EV1527", 7);

    if (sigCount < MAX_SAVED_SIGNALS) sigCount++;
    storage_save_signals(sigList, sigCount);
    Serial.printf("[NVS] Created Smart Remote: '%s' Addr=0x%05X, count=%d\n", name, (unsigned int)sig.code, sigCount);
    return true;
}


