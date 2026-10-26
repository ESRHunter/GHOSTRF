#include "serial_proto.h"
#include "ble_proto.h"
#include "config.h"
#include "storage.h"
#include "radio.h"
#include "buttons.h"
#include "ui.h"
#include "battery.h"
#include "wifi_tools.h"

static char rxLineBuf[1024];
static int rxLineLen = 0;
static bool screenStreaming = false;
static unsigned long lastScreenStream = 0;
static const char hexLookup[] = "0123456789ABCDEF";

static void proto_println(const char* str) {
    Serial.println(str);
    if (ble_proto_is_connected()) {
        ble_proto_send(str);
        ble_proto_send("\n");
    }
}

static void proto_printf(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    proto_println(buf);
}

bool serial_proto_is_streaming() {
    return screenStreaming;
}

static uint8_t lastScreenBuf[1024];
static bool hasLastScreenBuf = false;
static unsigned long lastForceScreenSend = 0;

void serial_proto_send_screen_frame(bool force) {
    const uint8_t* buf = ui_get_display_buffer();
    int sz = ui_get_display_buffer_size();
    if (!buf || sz <= 0) return;

    bool changed = (!hasLastScreenBuf) || (memcmp(buf, lastScreenBuf, sz) != 0);
    if (!changed && !force && (millis() - lastForceScreenSend < 1500)) {
        return;
    }

    memcpy(lastScreenBuf, buf, sz);
    hasLastScreenBuf = true;
    lastForceScreenSend = millis();

    Serial.print("{\"cmd\":\"SCREEN\",\"hex\":\"");
    for (int i = 0; i < sz; i++) {
        uint8_t b = buf[i];
        Serial.write(hexLookup[(b >> 4) & 0x0F]);
        Serial.write(hexLookup[b & 0x0F]);
    }
    Serial.println("\"}");

    if (ble_proto_is_connected()) {
        ble_proto_send("{\"cmd\":\"SCREEN\",\"hex\":\"");
        char hexChunk[241];
        int chunkPos = 0;
        for (int i = 0; i < sz; i++) {
            uint8_t b = buf[i];
            hexChunk[chunkPos++] = hexLookup[(b >> 4) & 0x0F];
            hexChunk[chunkPos++] = hexLookup[b & 0x0F];
            if (chunkPos >= 240) {
                hexChunk[chunkPos] = '\0';
                ble_proto_send(hexChunk);
                chunkPos = 0;
            }
        }
        if (chunkPos > 0) {
            hexChunk[chunkPos] = '\0';
            ble_proto_send(hexChunk);
        }
        ble_proto_send("\"}\n");
    }
}

static inline void send_screen_frame(bool force = true) {
    serial_proto_send_screen_frame(force);
}

void serial_proto_init() {
    rxLineLen = 0;
    screenStreaming = false;
    proto_println("{\"event\":\"DEVICE_READY\",\"device\":\"GHOSTRF\",\"ver\":\"1.2.0\"}");
}

static void send_pong(int sigCount, int freqIdx, int paIdx, int squelchIdx) {
    battery_update();
    proto_printf("{\"cmd\":\"PONG\",\"device\":\"GHOSTRF\",\"ver\":\"1.2.0\",\"chip\":\"ESP32-C3\",\"cc1101\":%s,\"free_heap\":%lu,\"signals_count\":%d,\"max_signals\":%d,\"freq\":%.3f,\"pa\":%d,\"squelch\":%d,\"bat_pct\":%d,\"bat_mv\":%d,\"charging\":%s}",
                 radio_is_connected() ? "true" : "false",
                 (unsigned long)ESP.getFreeHeap(),
                 sigCount,
                 MAX_SAVED_SIGNALS,
                 FREQ_PRESETS[freqIdx],
                 PA_LEVELS[paIdx],
                 SQUELCH_LEVELS[squelchIdx],
                 (int)battery_get_percentage(),
                 (int)battery_get_millivolts(),
                 battery_is_charging() ? "true" : "false");
}

static void send_signals_list(const SavedSignal* sigList, int sigCount) {
    proto_printf("{\"cmd\":\"SIGNALS_BEGIN\",\"count\":%d}", sigCount);

    for (int i = 0; i < sigCount; i++) {
        const SavedSignal &sig = sigList[i];
        String s = "{\"cmd\":\"SIGNAL\",\"index\":";
        s += i;
        s += ",\"name\":\"";
        s += sig.name;
        s += "\",\"type\":";
        s += sig.type;
        s += ",\"freq\":";
        s += String(sig.freq, 3);
        s += ",\"repeats\":";
        s += sig.repeats;
        s += ",\"proto\":\"";
        s += sig.protoName;
        s += "\",\"code\":";
        s += String(sig.code);
        s += ",\"bitLen\":";
        s += sig.bitLength;
        s += ",\"pulseLen\":";
        s += sig.pulseLength;
        s += ",\"protocol\":";
        s += sig.protocol;
        s += ",\"isKeeloq\":";
        s += sig.isKeeloq ? "true" : "false";
        s += ",\"keeloqSerial\":";
        s += String(sig.keeloqSerial);
        s += ",\"keeloqBtn\":";
        s += sig.keeloqBtn;
        s += ",\"keeloqEnc\":";
        s += String(sig.keeloqEnc);
        s += ",\"rawCount\":";
        s += sig.rawCount;
        if (sig.rawCount > 0) {
            s += ",\"pulses\":[";
            for (int p = 0; p < sig.rawCount; p++) {
                if (p > 0) s += ",";
                s += sig.rawPulses[p];
            }
            s += "]";
        } else {
            s += ",\"pulses\":[]";
        }
        s += "}";
        proto_println(s.c_str());
        delay(5);
    }

    proto_println("{\"cmd\":\"SIGNALS_END\"}");
}

void proto_process_line(char* line, SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state) {
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0') return;

    wake_screen_if_sleeping();

    if (strncmp(line, "PING", 4) == 0) {
        send_pong(sigCount, freqIdx, paIdx, squelchIdx);
    }
    else if (strncmp(line, "GET_SIGNALS", 11) == 0) {
        send_signals_list(sigList, sigCount);
    }
    else if (strncmp(line, "TX_SAVED", 8) == 0) {
        int idx = atoi(line + 8);
        if (idx >= 0 && idx < sigCount) {
            SavedSignal &sig = sigList[idx];
            if (sig.type == SIG_RAW || sig.rawCount > 0) {
                radio_transmit_raw_pulses(sig.rawPulses, sig.rawCount, sig.freq, PA_LEVELS[paIdx], sig.repeats);
            } else if (sig.type == SIG_SMART_EV1527) {
                radio_transmit_ev1527(sig.code, sig.keeloqBtn, sig.freq, PA_LEVELS[paIdx], sig.repeats);
            } else {
                radio_transmit_fixed(sig.code, sig.bitLength, sig.pulseLength, sig.protocol, sig.freq, PA_LEVELS[paIdx], sig.repeats);
            }
            proto_printf("{\"cmd\":\"TX_DONE\",\"status\":\"OK\",\"index\":%d}", idx);
        } else {
            proto_println("{\"cmd\":\"TX_DONE\",\"status\":\"ERR\",\"msg\":\"Invalid index\"}");
        }
    }
    else if (strncmp(line, "TX_FIXED", 8) == 0) {
        float f = 433.92;
        unsigned long code = 0;
        int bitLen = 24;
        int pulseLen = 320;
        int proto = 1;
        int pa = PA_LEVELS[paIdx];
        int reps = 7;
        int parsed = sscanf(line + 8, "%f %lu %d %d %d %d %d", &f, &code, &bitLen, &pulseLen, &proto, &pa, &reps);
        if (parsed >= 2) {
            radio_transmit_fixed(code, (uint8_t)bitLen, (uint16_t)pulseLen, (uint8_t)proto, f, pa, (uint8_t)reps);
            proto_println("{\"cmd\":\"TX_DONE\",\"status\":\"OK\"}");
        } else {
            proto_println("{\"cmd\":\"TX_DONE\",\"status\":\"ERR\",\"msg\":\"Param error\"}");
        }
    }
    else if (strncmp(line, "TX_RAW", 6) == 0) {
        float f = 433.92;
        int pa = PA_LEVELS[paIdx];
        int reps = 4;
        int count = 0;
        char* ptr = line + 6;
        f = strtof(ptr, &ptr);
        pa = strtol(ptr, &ptr, 10);
        reps = strtol(ptr, &ptr, 10);
        count = strtol(ptr, &ptr, 10);

        if (count > 0 && count <= MAX_RAW_PULSES) {
            uint16_t pulses[MAX_RAW_PULSES];
            int loaded = 0;
            while (*ptr && loaded < count) {
                while (*ptr == ' ' || *ptr == ',' || *ptr == '\t') ptr++;
                if (!*ptr) break;
                pulses[loaded++] = (uint16_t)strtoul(ptr, &ptr, 10);
            }
            radio_transmit_raw_pulses(pulses, loaded, f, pa, reps);
            proto_printf("{\"cmd\":\"TX_DONE\",\"status\":\"OK\",\"count\":%d}", loaded);
        } else {
            proto_println("{\"cmd\":\"TX_DONE\",\"status\":\"ERR\",\"msg\":\"Invalid count\"}");
        }
    }
    else if (strncmp(line, "SAVE_FIXED", 10) == 0) {
        char name[16] = {0};
        float f = 433.92;
        unsigned long code = 0;
        int bitLen = 24;
        int pulseLen = 320;
        int proto = 1;
        int reps = 5;
        char protoName[8] = "EV1527";
        int parsed = sscanf(line + 10, "%15s %f %lu %d %d %d %d %7s", name, &f, &code, &bitLen, &pulseLen, &proto, &reps, protoName);
        if (parsed >= 3) {
            int idx = (sigCount < MAX_SAVED_SIGNALS) ? sigCount : (MAX_SAVED_SIGNALS - 1);
            SavedSignal &s = sigList[idx];
            strncpy(s.name, name, 15);
            s.name[15] = '\0';
            s.type = SIG_FIXED;
            s.freq = f;
            s.code = code;
            s.bitLength = bitLen;
            s.pulseLength = pulseLen;
            s.protocol = proto;
            s.repeats = reps;
            strncpy(s.protoName, protoName, 7);
            s.protoName[7] = '\0';
            s.rawCount = 0;
            s.isKeeloq = false;
            s.keeloqSerial = 0;
            s.keeloqBtn = 0;
            s.keeloqEnc = 0;
            if (sigCount < MAX_SAVED_SIGNALS) sigCount++;
            storage_save_signals(sigList, sigCount);
            proto_printf("{\"cmd\":\"SAVE_DONE\",\"status\":\"OK\",\"count\":%d}", sigCount);
        } else {
            proto_println("{\"cmd\":\"SAVE_DONE\",\"status\":\"ERR\"}");
        }
    }
    else if (strncmp(line, "SAVE_RAW", 8) == 0) {
        char name[16] = {0};
        float f = 433.92;
        int reps = 5;
        char protoName[8] = "RAW";
        int isK = 0;
        unsigned long serial = 0;
        int btn = 0;
        unsigned long enc = 0;
        int count = 0;
        char* ptr = line + 8;
        if (sscanf(ptr, "%15s %f %d %7s %d %lu %d %lu %d", name, &f, &reps, protoName, &isK, &serial, &btn, &enc, &count) >= 9) {
            for (int a = 0; a < 9; a++) {
                while (*ptr == ' ' || *ptr == '\t') ptr++;
                while (*ptr && *ptr != ' ' && *ptr != '\t') ptr++;
            }
            int idx = (sigCount < MAX_SAVED_SIGNALS) ? sigCount : (MAX_SAVED_SIGNALS - 1);
            SavedSignal &s = sigList[idx];
            strncpy(s.name, name, 15);
            s.name[15] = '\0';
            s.type = SIG_RAW;
            s.freq = f;
            s.code = 0;
            s.repeats = reps;
            strncpy(s.protoName, protoName, 7);
            s.protoName[7] = '\0';
            s.isKeeloq = (isK != 0);
            s.keeloqSerial = serial;
            s.keeloqBtn = btn;
            s.keeloqEnc = enc;
            s.rawCount = (count > MAX_RAW_PULSES) ? MAX_RAW_PULSES : count;
            for (int p = 0; p < s.rawCount; p++) {
                while (*ptr == ' ' || *ptr == ',' || *ptr == '\t') ptr++;
                if (!*ptr) break;
                s.rawPulses[p] = (uint16_t)strtoul(ptr, &ptr, 10);
            }
            if (sigCount < MAX_SAVED_SIGNALS) sigCount++;
            storage_save_signals(sigList, sigCount);
            proto_printf("{\"cmd\":\"SAVE_DONE\",\"status\":\"OK\",\"count\":%d}", sigCount);
        } else {
            proto_println("{\"cmd\":\"SAVE_DONE\",\"status\":\"ERR\",\"msg\":\"Format error\"}");
        }
    }
    else if (strncmp(line, "DELETE_SIGNAL", 13) == 0) {
        int idx = atoi(line + 13);
        if (storage_delete_signal(idx, sigList, sigCount)) {
            proto_printf("{\"cmd\":\"DELETE_DONE\",\"status\":\"OK\",\"count\":%d}", sigCount);
        } else {
            proto_println("{\"cmd\":\"DELETE_DONE\",\"status\":\"ERR\"}");
        }
    }
    else if (strncmp(line, "CLEAR_SIGNALS", 13) == 0) {
        storage_clear_signals(sigCount);
        proto_println("{\"cmd\":\"CLEAR_DONE\",\"status\":\"OK\"}");
    }
    else if (strncmp(line, "GET_CONFIG", 10) == 0) {
        proto_printf("{\"cmd\":\"CONFIG\",\"freq_idx\":%d,\"freq_val\":%.3f,\"pa_idx\":%d,\"pa_val\":%d,\"squelch_idx\":%d,\"squelch_val\":%d}",
                     freqIdx, FREQ_PRESETS[freqIdx], paIdx, PA_LEVELS[paIdx], squelchIdx, SQUELCH_LEVELS[squelchIdx]);
    }
    else if (strncmp(line, "SET_CONFIG", 10) == 0) {
        int f = freqIdx, p = paIdx, sq = squelchIdx;
        int parsed = sscanf(line + 10, "%d %d %d", &f, &p, &sq);
        if (parsed >= 1 && f >= 0 && f < NUM_FREQ_PRESETS) freqIdx = f;
        if (parsed >= 2 && p >= 0 && p < NUM_PA_LEVELS) paIdx = p;
        if (parsed >= 3 && sq >= 0 && sq < NUM_SQUELCH_LEVELS) squelchIdx = sq;
        radio_apply_config(FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx]);
        storage_save_settings(freqIdx, paIdx, squelchIdx, 0);
        proto_printf("{\"cmd\":\"CONFIG_SAVED\",\"status\":\"OK\",\"freq\":%.3f,\"pa\":%d,\"squelch\":%d}",
                     FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx], SQUELCH_LEVELS[squelchIdx]);
    }
    else if (strncmp(line, "REMOTE_BTN", 10) == 0) {
        char btn[16] = {0};
        sscanf(line + 10, "%15s", btn);
        if (strcmp(btn, "UP") == 0) {
            buttons_inject_event(BTN_EVT_UP_CLICK);
        } else if (strcmp(btn, "DOWN") == 0) {
            buttons_inject_event(BTN_EVT_DOWN_CLICK);
        } else if (strcmp(btn, "OK") == 0) {
            buttons_inject_event(BTN_EVT_OK_CLICK);
        } else if (strcmp(btn, "OK_HOLD") == 0) {
            buttons_inject_event(BTN_EVT_OK_HOLD);
        }
        proto_printf("{\"cmd\":\"REMOTE_BTN_ACK\",\"btn\":\"%s\"}", btn);
    }
    else if (strncmp(line, "START_SNIFFER", 13) == 0) {
        char mode[16] = "FIXED";
        sscanf(line + 13, "%15s", mode);
        if (strcmp(mode, "RAW") == 0) {
            radio_start_raw_sniffer(FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx], SQUELCH_LEVELS[squelchIdx]);
            state = MODE_RAW_SNIFFER;
            proto_println("{\"cmd\":\"SNIFFER_STARTED\",\"mode\":\"RAW\"}");
        } else {
            radio_set_rx(FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx]);
            state = MODE_RX_SNIFFER;
            proto_println("{\"cmd\":\"SNIFFER_STARTED\",\"mode\":\"FIXED\"}");
        }
    }
    else if (strncmp(line, "STOP_SNIFFER", 12) == 0) {
        radio_set_idle();
        radio_stop_raw_sniffer();
        state = MODE_MAIN_MENU;
        proto_println("{\"cmd\":\"SNIFFER_STOPPED\"}");
    }
    else if (strncmp(line, "SET_CARRIER", 11) == 0) {
        int mode = 0;
        float f = FREQ_PRESETS[freqIdx];
        int pa = PA_LEVELS[paIdx];
        int on = 0;
        sscanf(line + 11, "%d %f %d %d", &mode, &f, &pa, &on);
        if (on) {
            radio_set_carrier((CarrierType)mode, f, pa);
            proto_println("{\"cmd\":\"CARRIER_DONE\",\"status\":\"ON\"}");
        } else {
            radio_set_idle();
            proto_println("{\"cmd\":\"CARRIER_DONE\",\"status\":\"OFF\"}");
        }
    }
    else if (strncmp(line, "GET_SCREEN", 10) == 0) {
        send_screen_frame();
    }
    else if (strncmp(line, "STREAM_SCREEN", 13) == 0) {
        int on = atoi(line + 13);
        screenStreaming = (on != 0);
        if (screenStreaming) {
            send_screen_frame();
        }
        Serial.printf("{\"cmd\":\"STREAM_SCREEN_ACK\",\"active\":%s}\n", screenStreaming ? "true" : "false");
    }
    else if (strncmp(line, "GOTO_MODE", 9) == 0) {
        char mode[16] = {0};
        sscanf(line + 9, "%15s", mode);
        if (strcmp(mode, "MAIN") == 0 || strcmp(mode, "0") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_MAIN_MENU;
            ui_draw_main_menu(0);
        } else if (strcmp(mode, "TX") == 0 || strcmp(mode, "1") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_TX_REMOTE;
            ui_draw_tx_menu(sigList, sigCount, 0);
        } else if (strcmp(mode, "RX") == 0 || strcmp(mode, "2") == 0 || strcmp(mode, "FIXED") == 0) {
            radio_set_rx(FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx]);
            state = MODE_RX_SNIFFER;
            ui_draw_rx_screen(freqIdx);
        } else if (strcmp(mode, "RAW") == 0 || strcmp(mode, "3") == 0) {
            radio_start_raw_sniffer(FREQ_PRESETS[freqIdx], PA_LEVELS[paIdx], SQUELCH_LEVELS[squelchIdx]);
            state = MODE_RAW_SNIFFER;
            ui_draw_raw_screen(freqIdx, squelchIdx);
        } else if (strcmp(mode, "FREQ") == 0 || strcmp(mode, "4") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_FREQ_ANALYZER;
            ui_draw_freq_analyzer_screen();
        } else if (strcmp(mode, "SPECTRUM") == 0 || strcmp(mode, "5") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_SPECTRUM;
            ui_reset_spectrum_peaks();
            ui_draw_spectrum_screen(0);
        } else if (strcmp(mode, "CARRIER") == 0 || strcmp(mode, "6") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_CARRIER_GEN;
            ui_draw_carrier_gen(freqIdx, paIdx, CARRIER_CW, false);
        } else if (strcmp(mode, "24G") == 0 || strcmp(mode, "7") == 0 || strcmp(mode, "WIFI") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_24G_MENU;
            ui_draw_24g_menu(0);
        } else if (strcmp(mode, "BATTERY") == 0 || strcmp(mode, "8") == 0 || strcmp(mode, "BAT") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            battery_update();
            state = MODE_BATTERY_INFO;
            ui_draw_battery_screen(battery_get_millivolts(), battery_get_percentage(), battery_is_charging());
        } else if (strcmp(mode, "SETTINGS") == 0 || strcmp(mode, "9") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_SETTINGS;
            ui_draw_settings_screen(ble_proto_is_enabled(), ble_proto_is_connected());
        } else if (strcmp(mode, "SYS_INFO") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            state = MODE_SYS_INFO;
            ui_draw_sys_info();
        } else if (strcmp(mode, "NEW_SIG") == 0 || strcmp(mode, "EDITOR") == 0) {
            radio_set_idle();
            radio_stop_raw_sniffer();
            open_new_signal_editor();
            state = MODE_SIGNAL_EDITOR;
        }
        send_screen_frame();
        char resp[96];
        snprintf(resp, sizeof(resp), "{\"cmd\":\"GOTO_MODE_ACK\",\"mode\":\"%s\",\"state\":%d}", mode, (int)state);
        proto_println(resp);
    }
    else if (strncmp(line, "BLE_SET", 7) == 0) {
        int on = atoi(line + 7);
        ble_proto_set_enabled(on != 0);
        if (state == MODE_SETTINGS) {
            ui_draw_settings_screen(ble_proto_is_enabled(), ble_proto_is_connected());
            send_screen_frame();
        }
        proto_printf("{\"cmd\":\"BLE_SET_ACK\",\"enabled\":%s}", ble_proto_is_enabled() ? "true" : "false");
    }
    else if (strncmp(line, "GET_BATTERY", 11) == 0) {
        battery_update();
        proto_printf("{\"cmd\":\"BATTERY\",\"raw_adc_mv\":%d,\"calc_mv\":%d,\"pct\":%d,\"charging\":%s,\"overvolt\":%s}",
                     (int)battery_get_raw_adc_mv(),
                     (int)battery_get_millivolts(),
                     (int)battery_get_percentage(),
                     battery_is_charging() ? "true" : "false",
                     battery_is_overvoltage() ? "true" : "false");
    }
    else if (strncmp(line, "REBOOT", 6) == 0) {
        proto_println("{\"cmd\":\"REBOOTING\"}");
        delay(100);
        ESP.restart();
    }
    else {
        Serial.print("{\"cmd\":\"UNKNOWN\",\"echo\":\"");
        for (char* c = line; *c; c++) {
            if (*c == '"' || *c == '\\') Serial.print('\\');
            Serial.print(*c);
        }
        Serial.println("\"}");
    }
}

void serial_proto_poll(SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state) {
    if (screenStreaming && (millis() - lastScreenStream >= 100)) {
        lastScreenStream = millis();
        send_screen_frame(false);
    }

    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            rxLineBuf[rxLineLen] = '\0';
            if (rxLineLen > 0) {
                proto_process_line(rxLineBuf, sigList, sigCount, freqIdx, paIdx, squelchIdx, state);
                rxLineLen = 0;
            }
        } else {
            if (rxLineLen < (int)sizeof(rxLineBuf) - 2) {
                rxLineBuf[rxLineLen++] = c;
            }
        }
    }
}

void serial_proto_broadcast_fixed_rx(uint32_t code, uint8_t bitLen, uint16_t pulseLen, uint8_t proto, float freq, int rssi) {
    char buf[160];
    snprintf(buf, sizeof(buf), "{\"event\":\"CAPTURED_FIXED\",\"code\":%lu,\"codeHex\":\"0x%lX\",\"bitLen\":%d,\"pulseLen\":%d,\"proto\":%d,\"freq\":%.3f,\"rssi\":%d}",
             (unsigned long)code, (unsigned long)code, bitLen, pulseLen, proto, freq, rssi);
    proto_println(buf);
}

void serial_proto_broadcast_raw_rx(const RawPacket &pkt, float freq, int rssi) {
    Serial.print("{\"event\":\"CAPTURED_RAW\",\"proto\":\"");
    Serial.print(pkt.protoName);
    Serial.print("\",\"isKeeloq\":");
    Serial.print(pkt.isKeeloq ? "true" : "false");
    Serial.print(",\"serial\":");
    Serial.print(pkt.keeloqSerial);
    Serial.print(",\"btn\":");
    Serial.print(pkt.keeloqBtn);
    Serial.print(",\"enc\":");
    Serial.print(pkt.keeloqEnc);
    Serial.print(",\"count\":");
    Serial.print(pkt.pulseCount);
    Serial.print(",\"freq\":");
    Serial.print(freq, 3);
    Serial.print(",\"rssi\":");
    Serial.print(rssi);
    Serial.print(",\"pulses\":[");
    for (int p = 0; p < pkt.pulseCount; p++) {
        if (p > 0) Serial.print(",");
        Serial.print(pkt.pulses[p]);
    }
    Serial.println("]}");

    if (ble_proto_is_connected()) {
        char hdr[180];
        snprintf(hdr, sizeof(hdr), "{\"event\":\"CAPTURED_RAW\",\"proto\":\"%s\",\"isKeeloq\":%s,\"serial\":%lu,\"btn\":%d,\"enc\":%lu,\"count\":%d,\"freq\":%.3f,\"rssi\":%d,\"pulses\":[",
                 pkt.protoName, pkt.isKeeloq ? "true" : "false", (unsigned long)pkt.keeloqSerial, pkt.keeloqBtn, (unsigned long)pkt.keeloqEnc, pkt.pulseCount, freq, rssi);
        ble_proto_send(hdr);
        char pBuf[32];
        for (int p = 0; p < pkt.pulseCount; p++) {
            snprintf(pBuf, sizeof(pBuf), "%s%d", (p > 0 ? "," : ""), pkt.pulses[p]);
            ble_proto_send(pBuf);
        }
        ble_proto_send("]}\n");
    }
}
