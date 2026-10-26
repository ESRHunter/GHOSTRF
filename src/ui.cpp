#include "ui.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "radio.h"
#include "ble_proto.h"
#include "wifi_tools.h"
#include "battery.h"

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
static uint8_t rxAnimFrame = 0;

static int peakHold[SPECTRUM_CHANNELS] = {0};
static unsigned long lastPeakDecay = 0;

const SpectrumBand SPECTRUM_BANDS[NUM_SPECTRUM_BANDS] = {
    { "433M", 433.05f, 0.0540f, "433.05   433.9   434.8" },
    { "315M", 314.00f, 0.0625f, "314.0    315.0   316.0" },
    { "868M", 867.50f, 0.0625f, "867.5    868.5   869.5" }
};

void ui_init() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.println("[ERR] OLED SSD1306 init failed!");
    } else {
        Serial.println("[OK] OLED SSD1306 initialized.");
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextWrap(false); // Запрет переноса строк на экранные элементы ниже
    display.setTextColor(SSD1306_WHITE);
    display.display();
}

const unsigned char ghostrf_bitmap_32x32[] PROGMEM = {
    0x00, 0x3f, 0xf0, 0x00, 0x00, 0xff, 0xfe, 0x00,
    0x01, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0x80,
    0x03, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xff, 0xe0,
    0x07, 0xe3, 0xf1, 0xf0, 0x07, 0xe3, 0xf1, 0xf0,
    0x07, 0xe3, 0xf1, 0xf0, 0x0f, 0xe3, 0xf1, 0xf0,
    0x1f, 0xe3, 0xf1, 0xf0, 0x1f, 0xe3, 0xf1, 0xf0,
    0x1f, 0xff, 0xff, 0xf0, 0x1f, 0xfe, 0x7f, 0xf0,
    0x1f, 0xfc, 0x3f, 0xf0, 0x1f, 0xfc, 0x3f, 0xf0,
    0x1f, 0xfc, 0x3f, 0xf8, 0x1f, 0xfe, 0x7f, 0xf8,
    0x3f, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xfc,
    0x7f, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc,
    0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc,
    0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xf0,
    0xff, 0xff, 0xff, 0xc0, 0xff, 0xff, 0xff, 0xc0,
    0x3f, 0xff, 0xff, 0x80, 0x0f, 0xff, 0xff, 0x00,
    0x06, 0x1f, 0x8e, 0x00, 0x00, 0x0f, 0x00, 0x00,
};

void ui_draw_splash() {
    display.clearDisplay();
    display.drawBitmap(48, 6, ghostrf_bitmap_32x32, 32, 32, SSD1306_WHITE);
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(22, 44);
    display.print("GHOSTRF");
    display.setTextSize(1);
    display.display();
}

void ui_set_display_power(bool on) {
    if (on) {
        display.ssd1306_command(SSD1306_DISPLAYON);
        delay(15);
        display.display();
    } else {
        display.ssd1306_command(SSD1306_DISPLAYOFF);
    }
}

const uint8_t* ui_get_display_buffer() {
    return display.getBuffer();
}

int ui_get_display_buffer_size() {
    return (SCREEN_WIDTH * ((SCREEN_HEIGHT + 7) / 8));
}

void ui_draw_header(const char* title, const char* badge) {
    display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);

    // 1. Иконка батареи справа (x: 114..126)
    display.drawRect(114, 1, 11, 8, SSD1306_BLACK);
    display.drawFastVLine(125, 3, 4, SSD1306_BLACK);
    uint8_t pct = battery_get_percentage();
    int fillW = map(constrain(pct, 0, 100), 0, 100, 0, 7);
    if (fillW > 0) {
        display.fillRect(116, 3, fillW, 4, SSD1306_BLACK);
    }
    if (battery_is_charging()) {
        display.drawPixel(119, 4, SSD1306_WHITE);
        display.drawPixel(120, 3, SSD1306_WHITE);
    }

    // 2. Индикатор Bluetooth (если подключен: x: 99..112)
    bool btOn = ble_proto_is_connected();
    if (btOn) {
        display.fillRect(99, 1, 13, 8, SSD1306_BLACK);
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(100, 1);
        display.print("BT");
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    }

    int rightEdge = btOn ? 97 : 112;

    int badgeW = (badge && badge[0]) ? (strlen(badge) * 6) : 0;
    int badgeX = rightEdge - badgeW;

    if (badge && badge[0]) {
        display.setCursor(badgeX, 1);
        display.print(badge);
    }

    // Заголовок гарантированно не залезает на бейдж
    int maxTitlePx = (badge && badge[0]) ? (badgeX - 4) : (rightEdge - 2);
    int maxTitleChars = max(1, maxTitlePx / 6);

    display.setCursor(2, 1);
    display.printf("%.*s", maxTitleChars, title);
}

void ui_draw_footer(const char* text) {
    display.drawFastHLine(0, 54, SCREEN_WIDTH, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 56);
    display.print(text);
}

void ui_draw_toast(const char* message) {
    display.fillRect(8, 20, 112, 22, SSD1306_WHITE);
    display.drawRect(9, 21, 110, 20, SSD1306_BLACK);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(14, 27);
    display.print(message);
    display.display();
}

void ui_draw_main_menu(int menuIndex) {
    display.clearDisplay();
    ui_draw_header("GHOSTRF", "C3");

    const char* items[] = {
        "1. Remote (TX)",
        "2. Sniffer (RX)",
        "3. RAW Sniffer",
        "4. Freq Analyzer",
        "5. RSSI Spectrum",
        "6. Carrier / Jam",
        "7. 2.4GHz Tools",
        "8. Battery Info",
        "9. Settings"
    };

    const int totalItems = 9;
    int start = (menuIndex / 4) * 4;
    int end = min(start + 4, totalItems);

    for (int i = start; i < end; i++) {
        int y = 12 + ((i - start) * 10);
        if (i == menuIndex) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
            display.setCursor(10, y);
            display.print(items[i]);
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.setCursor(10, y);
            display.print(items[i]);
        }
    }

    ui_draw_footer("UP/DN:Nav  OK:Enter");
    display.display();
}

void ui_draw_tx_menu(const SavedSignal* sigList, int sigCount, int txIndex) {
    display.clearDisplay();
    ui_draw_header("REMOTE (TX)", "MEM");

    int totalItems = sigCount + 1; // Existing signals + [+] NEW SIGNAL
    int start = (txIndex / 3) * 3;
    int end = min(start + 3, totalItems);

    for (int i = start; i < end; i++) {
        int y = 12 + ((i - start) * 14);

        if (i == txIndex) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }

        display.setCursor(10, y);
        if (i < sigCount) {
            display.printf("%.11s", sigList[i].name);

            // Бейдж типа сигнала справа
            display.setCursor(80, y);
            if (sigList[i].type == SIG_FIXED || sigList[i].type == SIG_SMART_EV1527) {
                char hexStr[10];
                snprintf(hexStr, sizeof(hexStr), "0x%05X", (unsigned int)(sigList[i].code & 0xFFFFF));
                display.print(hexStr);
            } else {
                if (sigList[i].isKeeloq) {
                    display.print("[KLQ]");
                } else {
                    char plsStr[10];
                    snprintf(plsStr, sizeof(plsStr), "[%dp]", sigList[i].rawCount);
                    display.print(plsStr);
                }
            }
        } else {
            display.print("[+] NEW SIGNAL");
        }
    }

    ui_draw_footer("OK:Select  Hold:Menu");
    display.display();
}

void ui_draw_signal_submenu(const SavedSignal &sig, int subMenuIdx) {
    display.clearDisplay();

    char badge[10];
    snprintf(badge, sizeof(badge), "[%s]", sig.protoName[0] != '\0' ? sig.protoName : (sig.type == SIG_FIXED ? "FIX" : "RAW"));
    ui_draw_header(sig.name, badge);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 13);
    if (sig.type == SIG_SMART_EV1527) {
        char buf[32];
        const char* btnNames[] = {"Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"};
        int bIdx = constrain(sig.keeloqBtn - 1, 0, 4);
        snprintf(buf, sizeof(buf), "%.2fM 0x%05X %s", sig.freq, (unsigned int)(sig.code & 0xFFFFF), btnNames[bIdx]);
        display.print(buf);
    } else if (sig.type == SIG_FIXED) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.2fM 0x%05X %dx", sig.freq, (unsigned int)(sig.code & 0xFFFFF), sig.repeats);
        display.print(buf);
    } else {
        if (sig.isKeeloq) {
            char buf[32];
            snprintf(buf, sizeof(buf), "SN:0x%07X %dx", (unsigned int)sig.keeloqSerial, sig.repeats);
            display.print(buf);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2fM %dp %dx", sig.freq, sig.rawCount, sig.repeats);
            display.print(buf);
        }
    }

    display.drawFastHLine(0, 23, SCREEN_WIDTH, SSD1306_WHITE);

    char txBuf[24];
    char btnBuf[24];
    char repBuf[24];
    const char* actions[7];
    int totalActions = 0;

    if (sig.type == SIG_SMART_EV1527) {
        const char* btnNames[] = {"Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"};
        int bIdx = constrain(sig.keeloqBtn - 1, 0, 4);
        snprintf(txBuf, sizeof(txBuf), "1. Transmit (%s)", btnNames[bIdx]);
        snprintf(btnBuf, sizeof(btnBuf), "2. Button: [%s]", btnNames[bIdx]);
        snprintf(repBuf, sizeof(repBuf), "3. Repeat: [%dx]", sig.repeats);
        actions[0] = txBuf;
        actions[1] = btnBuf;
        actions[2] = repBuf;
        actions[3] = "4. Edit Signal";
        actions[4] = "5. Waveform / Info";
        actions[5] = "6. Delete Signal";
        actions[6] = "7. Back to List";
        totalActions = 7;
    } else {
        snprintf(repBuf, sizeof(repBuf), "2. Repeat: [%dx]", sig.repeats);
        actions[0] = "1. Transmit (TX)";
        actions[1] = repBuf;
        actions[2] = "3. Edit Signal";
        actions[3] = "4. Waveform / Info";
        actions[4] = "5. Delete Signal";
        actions[5] = "6. Back to List";
        totalActions = 6;
    }

    int start = (subMenuIdx / 3) * 3;
    int end = min(start + 3, totalActions);

    for (int i = start; i < end; i++) {
        int y = 26 + ((i - start) * 10);
        if (i == subMenuIdx) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
            display.setCursor(10, y);
            display.print(actions[i]);
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.setCursor(10, y);
            display.print(actions[i]);
        }
    }

    ui_draw_footer("UP/DN:Nav  OK:Select");
    display.display();
}

void ui_draw_signal_editor(const SavedSignal &sig, int editorMenuIdx, bool isNewSignal) {
    display.clearDisplay();
    char badge[10];
    snprintf(badge, sizeof(badge), "[%s]", sig.protoName[0] != '\0' ? sig.protoName : "FIX");
    ui_draw_header(isNewSignal ? "NEW SIGNAL" : "EDIT SIGNAL", badge);

    char typeBuf[24];
    char nameBuf[24];
    char freqBuf[24];
    char btnBuf[24];
    char codeBuf[24];
    char repBuf[24];

    snprintf(typeBuf, sizeof(typeBuf), "Type: [%s]", sig.protoName[0] ? sig.protoName : "EV1527");
    snprintf(nameBuf, sizeof(nameBuf), "Name: [%s]", sig.name);
    snprintf(freqBuf, sizeof(freqBuf), "Freq: [%.2fM]", sig.freq);
    if (sig.type == SIG_SMART_EV1527) {
        const char* btnNames[] = {"Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"};
        int bIdx = constrain(sig.keeloqBtn - 1, 0, 4);
        snprintf(btnBuf, sizeof(btnBuf), "Btn:  [%s]", btnNames[bIdx]);
        snprintf(codeBuf, sizeof(codeBuf), "Addr: 0x%05X [RAND]", (unsigned int)(sig.code & 0xFFFFF));
    } else if (sig.type == SIG_RAW) {
        snprintf(btnBuf, sizeof(btnBuf), "Data: RAW (%dp)", sig.rawCount);
        snprintf(codeBuf, sizeof(codeBuf), "Freq: %.2f MHz", sig.freq);
    } else {
        snprintf(btnBuf, sizeof(btnBuf), "Bits: [%db %dus]", sig.bitLength, sig.pulseLength);
        snprintf(codeBuf, sizeof(codeBuf), "Code: 0x%05X [RAND]", (unsigned int)(sig.code & 0xFFFFF));
    }
    snprintf(repBuf, sizeof(repBuf), "Reps: [%dx]", sig.repeats);

    const char* rows[9] = {
        typeBuf,
        nameBuf,
        freqBuf,
        btnBuf,
        codeBuf,
        repBuf,
        "[ TEST TRANSMIT ]",
        "[ SAVE SIGNAL ]",
        "[ CANCEL ]"
    };

    int start = (editorMenuIdx / 3) * 3;
    int end = min(start + 3, 9);

    for (int i = start; i < end; i++) {
        int y = 12 + ((i - start) * 14);
        if (i == editorMenuIdx) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }
        display.setCursor(10, y);
        display.print(rows[i]);
    }

    if (editorMenuIdx == 7) {
        ui_draw_footer("OK: Save to Flash");
    } else if (editorMenuIdx == 6) {
        ui_draw_footer("OK: Transmit Test");
    } else if (editorMenuIdx == 8) {
        ui_draw_footer("OK: Discard changes");
    } else {
        ui_draw_footer("OK: Edit  UP/DN: Nav");
    }
    display.display();
}

void ui_draw_signal_waveform(const SavedSignal &sig) {
    display.clearDisplay();
    char badge[10];
    snprintf(badge, sizeof(badge), "[%s]", sig.protoName[0] != '\0' ? sig.protoName : (sig.type == SIG_FIXED ? "FIX" : "RAW"));
    ui_draw_header(sig.name, badge);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 12);
    if (sig.type == SIG_SMART_EV1527) {
        char buf[32];
        const char* btnNames[] = {"Btn 1", "Btn 2", "Btn 3", "Btn 4", "ALL"};
        int bIdx = constrain(sig.keeloqBtn - 1, 0, 4);
        snprintf(buf, sizeof(buf), "Addr:0x%05X  %s", (unsigned int)(sig.code & 0xFFFFF), btnNames[bIdx]);
        display.print(buf);
    } else if (sig.type == SIG_FIXED) {
        char triStr[13];
        if (radio_decode_pt2262_tristate(sig.code, sig.bitLength, triStr)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Tri: %s", triStr);
            display.print(buf);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "0x%06X P%d %dus", (unsigned int)sig.code, sig.protocol, sig.pulseLength);
            display.print(buf);
        }
    } else {
        if (sig.isKeeloq) {
            char buf[32];
            snprintf(buf, sizeof(buf), "SN:0x%07X B:%X", (unsigned int)sig.keeloqSerial, sig.keeloqBtn);
            display.print(buf);
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f MHz  %d pulses", sig.freq, sig.rawCount);
            display.print(buf);
        }
    }

    // Мини-осциллограф формы импульсов
    display.drawRect(2, 22, 124, 20, SSD1306_WHITE);
    int x = 4;
    bool lvl = true;
    if (sig.type == SIG_RAW) {
        for (int i = 0; i < (int)sig.rawCount && x < 122; i++) {
            int w = map(constrain(sig.rawPulses[i], 120, 2500), 120, 2500, 2, 7);
            int y = lvl ? 25 : 37;
            display.drawFastHLine(x, y, w, SSD1306_WHITE);
            display.drawFastVLine(x + w, 25, 13, SSD1306_WHITE);
            x += w;
            lvl = !lvl;
        }
    } else {
        uint32_t c = (sig.type == SIG_SMART_EV1527) ? ((sig.code << 4) | (1 << constrain(sig.keeloqBtn - 1, 0, 3))) : sig.code;
        int bits = sig.bitLength > 0 ? sig.bitLength : 24;
        for (int b = bits - 1; b >= 0 && x < 122; b--) {
            bool bit = (c >> b) & 1;
            int w1 = bit ? 6 : 2;
            int w2 = bit ? 2 : 6;
            display.drawFastHLine(x, 25, w1, SSD1306_WHITE);
            display.drawFastVLine(x + w1, 25, 13, SSD1306_WHITE);
            display.drawFastHLine(x + w1, 37, w2, SSD1306_WHITE);
            display.drawFastVLine(x + w1 + w2, 25, 13, SSD1306_WHITE);
            x += (w1 + w2);
        }
    }

    display.setCursor(2, 45);
    display.print("Repeats: ");
    display.print(sig.repeats);
    display.print("x  Power: +10dB");

    ui_draw_footer("Any key: Back");
    display.display();
}

void ui_show_tx_anim() {
    display.fillRect(0, 0, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(10, 1);
    display.print(">>> TRANSMITTING <<<");
    display.display();
}

void ui_draw_rx_screen(int freqIdx) {
    display.clearDisplay();
    ui_draw_header("SNIFFER (RX)", FREQ_NAMES[freqIdx]);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 13);
    display.print("Freq: ");
    display.print(FREQ_NAMES[freqIdx]);
    display.print(" MHz");

    display.setCursor(2, 24);
    display.print("Mod:  ASK/OOK");

    display.setCursor(2, 35);
    display.print("Type: PT/EV1527");

    display.setCursor(2, 46);
    display.print("Listening...");

    ui_draw_footer("Hold OK: Exit");
    display.display();
}

void ui_update_rx_anim() {
    const char* frames[] = {"-", "\\", "|", "/"};
    display.fillRect(74, 46, 24, 8, SSD1306_BLACK);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(74, 46);
    display.print("[ ");
    display.print(frames[rxAnimFrame % 4]);
    display.print(" ]");
    display.display();
    rxAnimFrame++;
}

void ui_draw_rx_details(uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol) {
    display.clearDisplay();

    char buf[32];
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    if (bitLength == 24) {
        char triStr[13];
        if (radio_decode_pt2262_tristate(code, 24, triStr)) {
            ui_draw_header("PT2262 TRISTATE", "SOCKET");
            snprintf(buf, sizeof(buf), "Tri: %s", triStr);
            display.setCursor(2, 12);
            display.print(buf);

            snprintf(buf, sizeof(buf), "HEX: 0x%06X (24b)", (unsigned int)code);
            display.setCursor(2, 23);
            display.print(buf);

            snprintf(buf, sizeof(buf), "Proto:%d  P:%dus", protocol, pulseLength);
            display.setCursor(2, 34);
            display.print(buf);
        } else {
            ui_draw_header("EV1527 CAPTURED", "SMART");
            uint32_t addr = (code >> 4) & 0xFFFFF;
            uint8_t btn = code & 0x0F;
            snprintf(buf, sizeof(buf), "Addr: 0x%05X  Btn: %d", (unsigned int)addr, btn);
            display.setCursor(2, 12);
            display.print(buf);

            snprintf(buf, sizeof(buf), "HEX: 0x%06X (24b)", (unsigned int)code);
            display.setCursor(2, 23);
            display.print(buf);

            snprintf(buf, sizeof(buf), "Proto:%d  P:%dus", protocol, pulseLength);
            display.setCursor(2, 34);
            display.print(buf);
        }
    } else if (bitLength == 12) {
        ui_draw_header("12-BIT CAPTURED", "GATE");
        snprintf(buf, sizeof(buf), "HEX: 0x%03X (%lu)", (unsigned int)code, (unsigned long)code);
        display.setCursor(2, 12);
        display.print(buf);

        snprintf(buf, sizeof(buf), "Bits: 12  Proto: %d", protocol);
        display.setCursor(2, 23);
        display.print(buf);

        snprintf(buf, sizeof(buf), "Pulse: %dus", pulseLength);
        display.setCursor(2, 34);
        display.print(buf);
    } else {
        ui_draw_header("FIXED CAPTURED!", "NEW");

        snprintf(buf, sizeof(buf), "HEX: 0x%06X", (unsigned int)code);
        display.setCursor(2, 12);
        display.print(buf);

        snprintf(buf, sizeof(buf), "DEC: %lu (%db)", (unsigned long)code, bitLength);
        display.setCursor(2, 23);
        display.print(buf);

        snprintf(buf, sizeof(buf), "Proto:%d  P:%dus", protocol, pulseLength);
        display.setCursor(2, 34);
        display.print(buf);
    }

    display.setCursor(2, 45);
    display.print("[OK]Save [UP]TX [DN]X");

    ui_draw_footer("OK to save to NVS");
    display.display();
}

void ui_draw_raw_screen(int freqIdx, int squelchIdx) {
    display.clearDisplay();
    ui_draw_header("RAW SNIFFER", FREQ_NAMES[freqIdx]);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 13);
    display.print("Freq: ");
    display.print(FREQ_NAMES[freqIdx]);
    display.print(" MHz");

    display.setCursor(2, 23);
    display.print("Squelch: ");
    display.print(SQUELCH_NAMES[squelchIdx]);

    display.setCursor(2, 33);
    display.print("Listening... [ - ]");

    // Рамка полосы уровня RSSI в реальном времени
    display.setCursor(2, 44);
    display.print("RSSI:");
    display.drawRect(34, 44, 90, 8, SSD1306_WHITE);

    ui_draw_footer("Hold OK: Exit");
    display.display();
}

void ui_update_raw_anim(int squelchDbm) {
    // Вращающийся спиннер
    const char* frames[] = {"-", "\\", "|", "/"};
    display.fillRect(86, 33, 24, 8, SSD1306_BLACK);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(86, 33);
    display.print("[ ");
    display.print(frames[rxAnimFrame % 4]);
    display.print(" ]");
    rxAnimFrame++;

    // Шкала RSSI в реальном времени
    int rssi = radio_get_rssi();
    int barW = map(constrain(rssi, -115, -25), -115, -25, 0, 86);
    int thrX = map(constrain(squelchDbm, -115, -25), -115, -25, 0, 86);

    display.fillRect(36, 46, 86, 4, SSD1306_BLACK);
    if (barW > 0) {
        display.fillRect(36, 46, barW, 4, SSD1306_WHITE);
    }
    // Маркер порога шумоподавителя
    display.drawFastVLine(36 + thrX, 43, 8, SSD1306_WHITE);

    display.display();
}

void ui_draw_raw_details(const RawPacket &pkt) {
    display.clearDisplay();

    if (pkt.isKeeloq) {
        ui_draw_header("KEELOQ CAPTURED!", "HCS");

        char buf[32];
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

        snprintf(buf, sizeof(buf), "SN: 0x%07X", (unsigned int)pkt.keeloqSerial);
        display.setCursor(2, 13);
        display.print(buf);

        snprintf(buf, sizeof(buf), "Btn: 0x%X  Enc: 0x%04X", pkt.keeloqBtn, (unsigned int)(pkt.keeloqEnc & 0xFFFF));
        display.setCursor(2, 24);
        display.print(buf);

        snprintf(buf, sizeof(buf), "Pulses: %d (66-bit)", pkt.pulseCount);
        display.setCursor(2, 35);
        display.print(buf);

        display.setCursor(2, 45);
        display.print("[OK]Save [UP]TX [DN]X");

        ui_draw_footer("Save KeeLoq to NVS");
    } else if (pkt.decodedCode != 0) {
        char title[24];
        char badge[10];
        snprintf(title, sizeof(title), "%s DECODED!", pkt.protoName);
        snprintf(badge, sizeof(badge), "[%s]", pkt.protoName);
        ui_draw_header(title, badge);

        char buf[32];
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

        if (strcmp(pkt.protoName, "EV1527") == 0) {
            uint32_t addr = (pkt.decodedCode >> 4) & 0xFFFFF;
            uint8_t btn = pkt.decodedCode & 0x0F;
            snprintf(buf, sizeof(buf), "Addr: 0x%05X Btn: %d", (unsigned int)addr, btn);
            display.setCursor(2, 12);
            display.print(buf);

            snprintf(buf, sizeof(buf), "HEX: 0x%06X (24b)", (unsigned int)pkt.decodedCode);
            display.setCursor(2, 23);
            display.print(buf);
        } else {
            snprintf(buf, sizeof(buf), "Code: 0x%X (%db)", (unsigned int)pkt.decodedCode, pkt.decodedBitLen);
            display.setCursor(2, 12);
            display.print(buf);

            snprintf(buf, sizeof(buf), "Pulse: %dus (%dp)", pkt.decodedPulseLen, pkt.pulseCount);
            display.setCursor(2, 23);
            display.print(buf);
        }

        if (pkt.extraInfo[0] != '\0') {
            display.setCursor(2, 34);
            display.print(pkt.extraInfo);
        } else {
            snprintf(buf, sizeof(buf), "Pulses: %d  Pw: %dus", pkt.pulseCount, pkt.decodedPulseLen);
            display.setCursor(2, 34);
            display.print(buf);
        }

        display.setCursor(2, 45);
        display.print("[OK]Save [UP]TX [DN]X");

        ui_draw_footer("OK:Save Clean FIX");
    } else {
        char title[24];
        char badge[10];
        snprintf(title, sizeof(title), "%s CAPTURED!", pkt.protoName);
        snprintf(badge, sizeof(badge), "[%s]", pkt.protoName);
        ui_draw_header(title, badge);

        char buf[32];
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

        snprintf(buf, sizeof(buf), "Proto: %s (%dp)", pkt.protoName, pkt.pulseCount);
        display.setCursor(2, 12);
        display.print(buf);

        // Мини-осциллограф формы импульсов
        display.drawRect(2, 22, 124, 18, SSD1306_WHITE);
        int x = 4;
        bool lvl = true;
        for (int i = 0; i < (int)pkt.pulseCount && x < 122; i++) {
            int w = map(constrain(pkt.pulses[i], 120, 2500), 120, 2500, 2, 8);
            int y = lvl ? 25 : 35;
            display.drawFastHLine(x, y, w, SSD1306_WHITE);
            display.drawFastVLine(x + w, 25, 11, SSD1306_WHITE);
            x += w;
            lvl = !lvl;
        }

        display.setCursor(2, 44);
        display.print("[OK]Save [UP]TX [DN]X");

        ui_draw_footer("Save Signal to NVS");
    }
    display.display();
}

void ui_draw_freq_analyzer_screen() {
    display.clearDisplay();
    ui_draw_header("FREQ ANALYZER", "SCAN");

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 13);
    display.print("Scanning Sub-GHz...");

    display.setCursor(2, 44);
    display.print("RSSI:");
    display.drawRect(34, 44, 90, 8, SSD1306_WHITE);

    ui_draw_footer("Hold OK: Exit");
    display.display();
}

void ui_update_freq_analyzer(float peakFreq, int peakRssi, bool locked) {
    display.fillRect(0, 11, SCREEN_WIDTH, 42, SSD1306_BLACK);

    if (locked) {
        display.fillRect(0, 11, SCREEN_WIDTH, 11, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.setCursor(8, 13);
        display.print(">>> SIGNAL DETECTED! <<<");
    } else {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(2, 13);
        display.print("Press keyfob button...");
    }

    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    char freqBuf[16];
    snprintf(freqBuf, sizeof(freqBuf), "%.2f", peakFreq);
    display.setCursor(8, 25);
    display.print(freqBuf);

    display.setTextSize(1);
    display.setCursor(84, 29);
    display.print("MHz");

    display.setCursor(2, 44);
    display.print("RSSI:");
    display.drawRect(34, 44, 90, 8, SSD1306_WHITE);

    int barW = map(constrain(peakRssi, -115, -25), -115, -25, 0, 86);
    if (barW > 0) {
        display.fillRect(36, 46, barW, 4, SSD1306_WHITE);
    }

    if (locked) {
        ui_draw_footer("OK:Tune & Sniff  Hold:X");
    } else {
        char rssiStr[24];
        snprintf(rssiStr, sizeof(rssiStr), "Peak: %ddBm", peakRssi);
        ui_draw_footer(rssiStr);
    }
    display.display();
}

void ui_draw_spectrum_screen(int bandIdx) {
    display.clearDisplay();
    ui_draw_header("RSSI SPECTRUM", SPECTRUM_BANDS[bandIdx].name);

    display.drawFastHLine(0, 52, SCREEN_WIDTH, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 55);
    display.print(SPECTRUM_BANDS[bandIdx].labelRange);
    display.display();
}

void ui_reset_spectrum_peaks() {
    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
        peakHold[ch] = 0;
    }
}

void ui_update_spectrum(int bandIdx) {
    const SpectrumBand &band = SPECTRUM_BANDS[bandIdx];
    int maxRssi = -130;

    display.fillRect(0, 11, SCREEN_WIDTH, 41, SSD1306_BLACK);

    if (millis() - lastPeakDecay > 1200) {
        lastPeakDecay = millis();
        for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
            if (peakHold[ch] > 0) peakHold[ch]--;
        }
    }

    for (int ch = 0; ch < SPECTRUM_CHANNELS; ch++) {
        float f = band.startFreq + (ch * band.stepFreq);
        int rssi = radio_scan_rssi(f);

        if (rssi > maxRssi) {
            maxRssi = rssi;
        }

        int barHeight = map(constrain(rssi, -110, -15), -110, -15, 0, 40);
        if (barHeight > peakHold[ch]) {
            peakHold[ch] = barHeight;
        }

        int x = ch * 4;
        int y = 51 - barHeight;

        if (barHeight > 0) {
            display.fillRect(x, y, 3, barHeight, SSD1306_WHITE);
        }

        if (peakHold[ch] > 1) {
            display.drawFastHLine(x, 51 - peakHold[ch], 3, SSD1306_WHITE);
        }
    }

    display.fillRect(68, 0, 60, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    char peakBuf[16];
    snprintf(peakBuf, sizeof(peakBuf), "%ddBm", maxRssi);
    display.setCursor(76, 1);
    display.print(peakBuf);

    display.display();
}

void ui_draw_carrier_gen(int freqIdx, int paIdx, CarrierType mode, bool isTransmitting) {
    display.clearDisplay();
    ui_draw_header("CARRIER / JAM", "RF");

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 13);
    display.print("Freq:  ");
    display.print(FREQ_NAMES[freqIdx]);
    display.print(" MHz");

    display.setCursor(2, 23);
    display.print("Power: ");
    display.print(PA_NAMES[paIdx]);

    display.setCursor(2, 33);
    display.print("Mode:  ");
    if (mode == CARRIER_CW) {
        display.print("CW Carrier");
    } else if (mode == CARRIER_TONE_1KHZ) {
        display.print("1 kHz Tone");
    } else {
        display.print("Jammer Noise");
    }

    display.setCursor(2, 44);
    display.print("State: ");
    if (isTransmitting) {
        display.fillRect(44, 43, 80, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.print("[ TX ACTIVE ]");
    } else {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.print("DISABLED");
    }

    ui_draw_footer("OK:Toggle  UP/DN:Mode");
    display.display();
}

void ui_draw_settings_screen(bool bleEnabled, bool bleConnected) {
    display.clearDisplay();
    ui_draw_header("SETTINGS", bleEnabled ? "BLE:ON" : "BLE:OFF");

    // Строка 1: Выделенная плашка BLE
    display.fillRect(0, 13, SCREEN_WIDTH, 11, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(4, 15);
    display.print("> BLE: ");
    display.print(bleEnabled ? "[ENABLED]" : "[DISABLED]");

    // Строка 2: Имя устройства
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(4, 27);
    display.print("Dev: GHOSTRF");

    // Строка 3: Текущее состояние
    display.setCursor(4, 39);
    if (!bleEnabled) {
        display.print("State: Inactive");
    } else if (bleConnected) {
        display.print("State: [BT CONNECTED]");
    } else {
        display.print("State: [BT WAITING...]");
    }

    // Подвал
    ui_draw_footer("OK: Toggle ON/OFF");
    display.display();
}

void ui_draw_sys_info() {
    display.clearDisplay();
    ui_draw_header("ESP32-C3 INFO");

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 12);
    char cpuBuf[32];
    snprintf(cpuBuf, sizeof(cpuBuf), "CPU: 160M  Temp:%dC", (int)temperatureRead());
    display.print(cpuBuf);

    display.setCursor(2, 23);
    char heapBuf[32];
    snprintf(heapBuf, sizeof(heapBuf), "RAM Free: %d KB", (int)(ESP.getFreeHeap() / 1024));
    display.print(heapBuf);

    display.setCursor(2, 34);
    unsigned long upSec = millis() / 1000;
    char upBuf[32];
    snprintf(upBuf, sizeof(upBuf), "Uptime: %lum %lus", upSec / 60, upSec % 60);
    display.print(upBuf);

    display.setCursor(2, 45);
    display.print("CC1101: ");
    display.print(radio_is_connected() ? "OK (Online)" : "ERR Disconn");

    ui_draw_footer("Any key: Back");
    display.display();
}

void ui_draw_24g_menu(int menuIndex) {
    display.clearDisplay();
    ui_draw_header("2.4G TOOLS", "WIFI/BT");

    const char* items[] = {
        "1. Wi-Fi Scanner",
        "2. 2.4G Channels",
        "3. Deauth Sniffer",
        "4. BLE Radar / Scan",
        "5. BLE Spammer",
        "6. Beacon Spam"
    };

    const int totalItems = 6;
    int start = (menuIndex / 4) * 4;
    int end = min(start + 4, totalItems);

    for (int i = start; i < end; i++) {
        int y = 12 + ((i - start) * 10);
        if (i == menuIndex) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
            display.setCursor(10, y);
            display.print(items[i]);
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
            display.setCursor(10, y);
            display.print(items[i]);
        }
    }

    ui_draw_footer("UP/DN:Nav  OK:Enter");
    display.display();
}

void ui_draw_wifi_scan_screen(int selectedIndex, bool isScanning) {
    display.clearDisplay();
    int count = wifi_tools_get_ap_count();
    char badge[12];
    snprintf(badge, sizeof(badge), "%d AP", count);
    ui_draw_header("WIFI SCAN", badge);

    if (isScanning) {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(8, 22);
        display.print("[ SCANNING 2.4G... ]");
        display.setCursor(14, 34);
        display.print("CH 1 .. 13 Sweep");

        static uint8_t animBar = 0;
        animBar = (animBar + 1) % 40;
        display.drawRect(14, 44, 100, 6, SSD1306_WHITE);
        display.fillRect(16 + (animBar * 2), 45, 16, 4, SSD1306_WHITE);
        ui_draw_footer("Scanning channels...");
        display.display();
        return;
    }

    if (count == 0) {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(10, 24);
        display.print("No APs found!");
        display.setCursor(10, 36);
        display.print("Press UP to Rescan");
        ui_draw_footer("UP:Rescan  Hold:Exit");
        display.display();
        return;
    }

    int start = (selectedIndex / 3) * 3;
    int end = min(start + 3, count);

    for (int i = start; i < end; i++) {
        const WifiApInfo* ap = wifi_tools_get_ap(i);
        if (!ap) continue;

        int y = 12 + ((i - start) * 14);
        if (i == selectedIndex) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }

        display.setCursor(10, y);
        char shortSsid[11];
        snprintf(shortSsid, sizeof(shortSsid), "%.10s", ap->ssid);
        display.print(shortSsid);

        display.setCursor(74, y);
        char chRssi[16];
        snprintf(chRssi, sizeof(chRssi), "c%d %ddB", ap->channel, ap->rssi);
        display.print(chRssi);
    }

    ui_draw_footer("OK:Details  UP:Rescan");
    display.display();
}

void ui_draw_wifi_details_screen(int selectedIndex) {
    display.clearDisplay();
    const WifiApInfo* ap = wifi_tools_get_ap(selectedIndex);
    if (!ap) {
        ui_draw_header("AP DETAILS", "INFO");
        display.setCursor(10, 24);
        display.print("No Data");
        ui_draw_footer("Hold:Back");
        display.display();
        return;
    }

    ui_draw_header("AP DETAILS", wifi_tools_get_auth_str(ap->auth));
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    display.setCursor(2, 12);
    char buf[32];
    snprintf(buf, sizeof(buf), "SSID: %-.14s", ap->ssid);
    display.print(buf);

    display.setCursor(2, 23);
    snprintf(buf, sizeof(buf), "MAC:%02X:%02X:%02X:%02X:%02X:%02X",
             ap->bssid[0], ap->bssid[1], ap->bssid[2], ap->bssid[3], ap->bssid[4], ap->bssid[5]);
    display.print(buf);

    display.setCursor(2, 34);
    int freq = 2407 + ap->channel * 5;
    snprintf(buf, sizeof(buf), "CH:%d (%dM) %ddB", ap->channel, freq, ap->rssi);
    display.print(buf);

    display.setCursor(2, 45);
    display.print("Sig:");
    int barW = map(constrain(ap->rssi, -100, -30), -100, -30, 0, 80);
    display.drawRect(30, 45, 84, 7, SSD1306_WHITE);
    if (barW > 0) display.fillRect(32, 47, barW, 3, SSD1306_WHITE);

    ui_draw_footer("Hold:Back to list");
    display.display();
}

void ui_draw_wifi_channels_screen() {
    display.clearDisplay();
    ui_draw_header("CHANNELS", "1..13");

    uint8_t counts[14];
    int8_t peakRssi[14];
    wifi_tools_get_channel_stats(counts, peakRssi);

    uint8_t maxCount = 1;
    int peakCh = 1;
    int8_t maxPeakRssi = -120;
    int totalAps = 0;

    for (int ch = 1; ch <= 13; ch++) {
        totalAps += counts[ch];
        if (counts[ch] > maxCount) maxCount = counts[ch];
        if (peakRssi[ch] > maxPeakRssi) {
            maxPeakRssi = peakRssi[ch];
            peakCh = ch;
        }
    }

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 12);
    char topBuf[32];
    snprintf(topBuf, sizeof(topBuf), "Top:CH%d %ddB APs:%d", peakCh, maxPeakRssi, totalAps);
    display.print(topBuf);

    // Отрисовка гистограммы 13 каналов
    for (int ch = 1; ch <= 13; ch++) {
        int x = 6 + (ch - 1) * 9;
        int barH = map(counts[ch], 0, maxCount, 0, 24);
        int y = 47 - barH;

        if (barH > 0) {
            display.fillRect(x, y, 7, barH, SSD1306_WHITE);
        } else {
            display.drawFastHLine(x, 47, 7, SSD1306_WHITE);
        }
        if (ch == peakCh && maxPeakRssi > -120) {
            display.drawFastHLine(x, y - 2, 7, SSD1306_WHITE);
        }
    }

    display.drawFastHLine(0, 48, SCREEN_WIDTH, SSD1306_WHITE);
    display.setCursor(6, 49);
    display.print("1  3  5  7  9 11 13");

    ui_draw_footer("Hold:Back  OK:Rescan");
    display.display();
}

void ui_draw_wifi_sniffer_screen() {
    display.clearDisplay();
    char badge[12];
    snprintf(badge, sizeof(badge), "CH %d", wifi_tools_get_sniffer_channel());
    ui_draw_header("DEAUTH MON", badge);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    display.setCursor(2, 12);
    char buf[32];
    snprintf(buf, sizeof(buf), "Pkts:%lu Mgmt:%lu", wifi_tools_get_total_pkts(), wifi_tools_get_mgmt_pkts());
    display.print(buf);

    display.setCursor(2, 23);
    snprintf(buf, sizeof(buf), "Probe:%lu Data:%lu", wifi_tools_get_probe_pkts(), wifi_tools_get_data_pkts());
    display.print(buf);

    uint32_t deauths = wifi_tools_get_deauth_pkts();
    if (deauths > 0) {
        display.fillRect(0, 33, SCREEN_WIDTH, 10, SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
        display.setCursor(2, 34);
        snprintf(buf, sizeof(buf), "! DEAUTH:%lu !", (unsigned long)deauths);
        display.print(buf);

        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(2, 45);
        snprintf(buf, sizeof(buf), "Vic:%s", wifi_tools_get_last_deauth_mac());
        display.print(buf);
    } else {
        display.setCursor(2, 35);
        display.print("Listening 2.4G frames...");

        static uint8_t hopAnim = 0;
        hopAnim++;
        display.setCursor(2, 45);
        const char* hops[] = {"[ - - - ]", "[ * - - ]", "[ - * - ]", "[ - - * ]"};
        display.print("Status: ");
        display.print(hops[hopAnim % 4]);
    }

    ui_draw_footer("Hold:Exit  OK:Reset");
    display.display();
}

void ui_draw_ble_scan_screen(int selectedIndex, bool isScanning) {
    display.clearDisplay();
    int count = wifi_tools_get_ble_count();
    char badge[12];
    snprintf(badge, sizeof(badge), "%d DEV", count);
    ui_draw_header("BLE RADAR", badge);

    if (isScanning && count == 0) {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(14, 24);
        display.print("[ SCANNING BLE... ]");
        display.setCursor(10, 36);
        display.print("Ch 37, 38, 39 Listen");
        ui_draw_footer("Listening beacons...");
        display.display();
        return;
    }

    if (count == 0) {
        display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        display.setCursor(10, 24);
        display.print("No BLE devices!");
        display.setCursor(10, 36);
        display.print("Listening in bg...");
        ui_draw_footer("OK:Rescan  Hold:Exit");
        display.display();
        return;
    }

    int start = (selectedIndex / 3) * 3;
    int end = min(start + 3, count);

    for (int i = start; i < end; i++) {
        const BleDevInfo* dev = wifi_tools_get_ble_dev(i);
        if (!dev) continue;

        int y = 12 + ((i - start) * 14);
        if (i == selectedIndex) {
            display.fillRect(0, y - 1, SCREEN_WIDTH, 13, SSD1306_WHITE);
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
            display.setCursor(2, y);
            display.print(">");
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }

        display.setCursor(10, y);
        char shortName[10];
        snprintf(shortName, sizeof(shortName), "%.9s", dev->name);
        display.print(shortName);

        display.setCursor(70, y);
        char tagBuf[14];
        snprintf(tagBuf, sizeof(tagBuf), "%3ddB %s", dev->rssi, dev->tag);
        display.print(tagBuf);
    }

    ui_draw_footer("OK:Clear  Hold:Exit");
    display.display();
}

void ui_draw_ble_spam_screen(uint8_t mode, uint32_t count) {
    display.clearDisplay();
    ui_draw_header("BLE SPAM", "2.4G TX");

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 12);
    display.print("Mode: ");
    if (mode == 0) display.print("AirPods Modal");
    else if (mode == 1) display.print("FastPair Popup");
    else display.print("GHOSTRF Beacon");

    display.setCursor(2, 24);
    char buf[32];
    snprintf(buf, sizeof(buf), "Packets: %lu", (unsigned long)count);
    display.print(buf);

    display.setCursor(2, 36);
    display.print("Adv: Ch 37, 38, 39");

    display.fillRect(10, 46, 108, 7, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(14, 46);
    display.print(">>> ((( TX ON ))) <<<");

    ui_draw_footer("UP/DN:Mode  OK:Stop");
    display.display();
}

void ui_draw_wifi_beacon_spam_screen(uint32_t count) {
    display.clearDisplay();
    ui_draw_header("BEACONS", "ACTIVE");

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
    display.setCursor(2, 12);
    display.print("12 SSIDs Multi-Cast:");

    display.setCursor(2, 23);
    display.print("SSIDs: GHOSTRF 01..12");

    display.setCursor(2, 34);
    char buf[32];
    snprintf(buf, sizeof(buf), "Total Sent: %lu", (unsigned long)count);
    display.print(buf);

    display.fillRect(10, 45, 108, 8, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(14, 45);
    display.print("[ BROADCAST ACTIVE ]");

    ui_draw_footer("OK: Stop & Exit");
    display.display();
}

// =========================================================================
// МОНИТОРИНГ АККУМУЛЯТОРА UI
// =========================================================================

void ui_draw_battery_screen(uint16_t mv, uint8_t pct, bool isCharging) {
    display.clearDisplay();
    char badge[10];
    if (mv < 2000) {
        snprintf(badge, sizeof(badge), "USB");
    } else if (mv > 4380) {
        snprintf(badge, sizeof(badge), "OV!");
    } else {
        snprintf(badge, sizeof(badge), "%d%%", pct);
    }
    ui_draw_header("BATTERY", badge);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    // Большой индикатор батареи с аккуратным центрированием
    display.drawRect(12, 14, 98, 11, SSD1306_WHITE);
    display.fillRect(110, 16, 3, 7, SSD1306_WHITE);

    int barW = map(constrain(pct, 0, 100), 0, 100, 0, 94);
    if (barW > 0) {
        display.fillRect(14, 16, barW, 7, SSD1306_WHITE);
    }

    char buf[32];
    if (mv < 2000) {
        display.setCursor(4, 29);
        display.print("Power: USB-C (5V)");
        display.setCursor(4, 41);
        display.print("Li-Po: Disconnected");
    } else if (mv > 4380) {
        display.setCursor(4, 29);
        snprintf(buf, sizeof(buf), "Volt: %.2fV (OVER!)", mv / 1000.0f);
        display.print(buf);

        display.setCursor(4, 41);
        display.print("ERR: Check Divider!");
    } else {
        display.setCursor(4, 29);
        snprintf(buf, sizeof(buf), "Volt: %.2fV (%dmV)", mv / 1000.0f, mv);
        display.print(buf);

        display.setCursor(4, 41);
        if (isCharging) {
            display.print("Status: CHARGING");
        } else if (pct > 20) {
            display.print("Status: DISCHARGE OK");
        } else {
            display.print("Status: LOW BATTERY!");
        }
    }

    ui_draw_footer("OK: Back to Menu");
    display.display();
}


