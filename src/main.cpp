#include <Arduino.h>
#include "config.h"
#include "types.h"
#include "buttons.h"
#include "storage.h"
#include "radio.h"
#include "ui.h"
#include "serial_proto.h"
#include "ble_proto.h"
#include "wifi_tools.h"
#include "battery.h"
#include "sub_storage.h"

// =========================================================================
// СОСТОЯНИЕ ПРИЛОЖЕНИЯ
// =========================================================================
static AppState currentState = MODE_MAIN_MENU;
static int menuIndex = 0;
static int txIndex = 0;
static int sigSubMenuIdx = 0;
static int settingsSubIndex = 0;

// Состояние 2.4 GHz Tools
static int menu24gIndex = 0;
static int wifiApIndex = 0;
static int bleDevIndex = 0;
static uint8_t bleSpamMode = 0;
static unsigned long last24gUpdate = 0;
static unsigned long lastBatteryUpdate = 0;

static int currentFreqIdx = 0;
static int currentPaIdx = 3;
static int currentSquelchIdx = 1; // По умолчанию -78 dBm
static int currentSpectrumBandIdx = 0;

static SavedSignal savedList[MAX_SAVED_SIGNALS];
static int totalSaved = 0;

// Редактор сигналов
static SavedSignal editSignal;
static int editSignalIndex = -1; // -1 = новый сигнал, >=0 = индекс существующего
static int editorMenuIdx = 0;

// Буферы пойманных сигналов
static uint32_t capturedCode = 0;
static uint8_t  capturedBitLen = 0;
static uint16_t capturedPulseLen = 0;
static uint8_t  capturedProto = 0;
static RawPacket capturedRawPkt;

// Генератор несущей / глушитель
static CarrierType currentCarrierMode = CARRIER_CW;
static bool isTransmittingCarrier = false;

// Частотомер / Автопоиск частоты
static unsigned long lastFreqScanUpdate = 0;
static float analyzerPeakFreq = 433.92;
static int analyzerPeakRssi = -120;
static bool analyzerLocked = false;

// Экранная заставка и активность
static unsigned long lastUserActivity = 0;
static bool isScreenSleeping = false;

static unsigned long lastRxAnimUpdate = 0;
static unsigned long lastSpectrumUpdate = 0;

void setup() {
    Serial.begin(115200);

    ui_init();
    ui_draw_splash();
    unsigned long splashStart = millis();

    storage_init();
    storage_load_all(currentFreqIdx, currentPaIdx, currentSquelchIdx, currentSpectrumBandIdx, savedList, totalSaved);

    // Initialize and mount LittleFS on the 896 KB flash partition
    if (sub_storage_init()) {
        size_t totalBytes = sub_storage_total_bytes();
        size_t usedBytes = sub_storage_used_bytes();
        size_t freeBytes = sub_storage_free_bytes();
        Serial.printf("[FS] LittleFS mounted on 896 KB partition: Total=%u B, Used=%u B, Free=%u B\n",
                      (unsigned int)totalBytes, (unsigned int)usedBytes, (unsigned int)freeBytes);
        sub_storage_check_and_migrate_nvs();
    } else {
        Serial.println("[FS] ERROR: Failed to mount LittleFS on flash partition!");
    }

    battery_init();
    radio_init();
    buttons_init();
    serial_proto_init();
    ble_proto_init();
    wifi_tools_init();

    Serial.printf("[SYS] Free Heap: %u B, Min Free Heap: %u B, Max Alloc Heap: %u B\n",
                  (unsigned int)ESP.getFreeHeap(),
                  (unsigned int)ESP.getMinFreeHeap(),
                  (unsigned int)ESP.getMaxAllocHeap());

    // Заставка ровно на 0.5с (500 мс)
    while (millis() - splashStart < 500) {
        delay(10);
    }

    lastUserActivity = millis();
    ui_draw_main_menu(menuIndex);
}

static void redraw_active_screen() {
    switch (currentState) {
        case MODE_MAIN_MENU:
            ui_draw_main_menu(menuIndex);
            break;
        case MODE_TX_REMOTE:
            ui_draw_tx_menu(savedList, totalSaved, txIndex);
            break;
        case MODE_SIGNAL_SUBMENU:
            if (txIndex >= 0 && txIndex < totalSaved)
                ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
            break;
        case MODE_SIGNAL_WAVEFORM:
            if (txIndex >= 0 && txIndex < totalSaved)
                ui_draw_signal_waveform(savedList[txIndex]);
            break;
        case MODE_SIGNAL_EDITOR:
            ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
            break;
        case MODE_RX_SNIFFER:
            ui_draw_rx_screen(currentFreqIdx);
            break;
        case MODE_RAW_SNIFFER:
            ui_draw_raw_screen(currentFreqIdx, currentSquelchIdx);
            break;
        case MODE_FREQ_ANALYZER:
            ui_draw_freq_analyzer_screen();
            break;
        case MODE_SPECTRUM:
            ui_draw_spectrum_screen(currentSpectrumBandIdx);
            break;
        case MODE_CARRIER_GEN:
            ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, isTransmittingCarrier);
            break;
        case MODE_24G_MENU:
            ui_draw_24g_menu(menu24gIndex);
            break;
        case MODE_WIFI_SCAN:
            ui_draw_wifi_scan_screen(wifiApIndex, !wifi_tools_scan_is_done());
            break;
        case MODE_WIFI_DETAILS:
            ui_draw_wifi_details_screen(wifiApIndex);
            break;
        case MODE_WIFI_CHANNELS:
            ui_draw_wifi_channels_screen();
            break;
        case MODE_WIFI_SNIFFER:
            ui_draw_wifi_sniffer_screen();
            break;
        case MODE_BLE_SCAN:
            ui_draw_ble_scan_screen(bleDevIndex, false);
            break;
        case MODE_BLE_SPAM:
            ui_draw_ble_spam_screen(bleSpamMode, wifi_tools_get_ble_spam_count());
            break;
        case MODE_WIFI_BEACON_SPAM:
            ui_draw_wifi_beacon_spam_screen(wifi_tools_get_beacon_spam_count());
            break;
        case MODE_BATTERY_INFO:
            ui_draw_battery_screen(battery_get_millivolts(), battery_get_percentage(), battery_is_charging());
            break;
        case MODE_SETTINGS:
            ui_draw_settings_screen(ble_proto_is_enabled(), ble_proto_is_connected());
            break;
        case MODE_SYS_INFO:
            ui_draw_sys_info();
            break;
        default:
            ui_draw_main_menu(menuIndex);
            break;
    }
    serial_proto_send_screen_frame();
}

void wake_screen_if_sleeping() {
    lastUserActivity = millis();
    if (isScreenSleeping) {
        ui_set_display_power(true);
        isScreenSleeping = false;
        redraw_active_screen();
    }
}

void open_new_signal_editor() {
    editSignalIndex = -1;
    editorMenuIdx = 0;
    memset(&editSignal, 0, sizeof(SavedSignal));
    snprintf(editSignal.name, sizeof(editSignal.name), "Sig_%02d", totalSaved + 1);
    editSignal.type = SIG_SMART_EV1527;
    editSignal.freq = 433.92f;
    editSignal.repeats = 5;
    strncpy(editSignal.protoName, "EV1527", 7);
    editSignal.code = (uint32_t)(esp_random() & 0xFFFFF);
    editSignal.bitLength = 24;
    editSignal.pulseLength = 320;
    editSignal.protocol = 1;
    editSignal.keeloqBtn = 1;
    currentState = MODE_SIGNAL_EDITOR;
    ui_draw_signal_editor(editSignal, editorMenuIdx, true);
    serial_proto_send_screen_frame(true);
}

void loop() {
    serial_proto_poll(savedList, totalSaved, currentFreqIdx, currentPaIdx, currentSquelchIdx, currentState);
    ble_proto_poll(savedList, totalSaved, currentFreqIdx, currentPaIdx, currentSquelchIdx, currentState);

    // Если активен стриминг экрана или подключен BLE-клиент, экран НЕ должен засыпать
    if (serial_proto_is_streaming() || ble_proto_is_connected()) {
        lastUserActivity = millis();
        if (isScreenSleeping) {
            wake_screen_if_sleeping();
        }
    }

    ButtonEvent evt = buttons_poll();

    // -------------------------------------------------------------
    // ЭНЕРГОСБЕРЕЖЕНИЕ И ЗАСТАВКА (АВТОМАТИЧЕСКИЙ СОН ДИСПЛЕЯ)
    // -------------------------------------------------------------
    if (evt != BTN_EVT_NONE) {
        if (isScreenSleeping) {
            wake_screen_if_sleeping();
            return; // Первый клик только пробуждает экран
        }
        lastUserActivity = millis();
    }

    if (!isScreenSleeping && (millis() - lastUserActivity > SCREEN_TIMEOUT_MS)) {
        if (!serial_proto_is_streaming() && !ble_proto_is_connected()) {
            ui_set_display_power(false);
            isScreenSleeping = true;
        } else {
            lastUserActivity = millis();
        }
    }

    // -------------------------------------------------------------
    // ГЛОБАЛЬНЫЙ ВЫХОД: Длинное удержание OK (> 750 мс)
    // -------------------------------------------------------------
    if (evt == BTN_EVT_OK_HOLD) {
        wifi_tools_stop_all();
        if (isTransmittingCarrier) {
            isTransmittingCarrier = false;
            radio_set_idle();
        }
        if (currentState == MODE_RX_SNIFFER) {
            radio_set_idle();
        }
        if (currentState == MODE_RAW_SNIFFER) {
            radio_stop_raw_sniffer();
        }
        if (currentState == MODE_FREQ_ANALYZER || currentState == MODE_SPECTRUM) {
            radio_set_idle();
            radio_apply_config(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
        }

        currentState = MODE_MAIN_MENU;
        ui_draw_main_menu(menuIndex);
        return;
    }

    // -------------------------------------------------------------
    // ОБРАБОТКА НАЖАТИЙ ПО ЭКРАНАМ
    // -------------------------------------------------------------
    switch (currentState) {
        case MODE_MAIN_MENU:
            if (evt == BTN_EVT_UP_CLICK) {
                menuIndex = (menuIndex - 1 + 9) % 9;
                ui_draw_main_menu(menuIndex);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                menuIndex = (menuIndex + 1) % 9;
                ui_draw_main_menu(menuIndex);
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (menuIndex == 0) {
                    currentState = MODE_TX_REMOTE;
                    ui_draw_tx_menu(savedList, totalSaved, txIndex);
                } else if (menuIndex == 1) {
                    currentState = MODE_RX_SNIFFER;
                    ui_draw_rx_screen(currentFreqIdx);
                    radio_set_rx(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
                } else if (menuIndex == 2) {
                    currentState = MODE_RAW_SNIFFER;
                    ui_draw_raw_screen(currentFreqIdx, currentSquelchIdx);
                    radio_start_raw_sniffer(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], SQUELCH_LEVELS[currentSquelchIdx]);
                } else if (menuIndex == 3) {
                    currentState = MODE_FREQ_ANALYZER;
                    analyzerPeakFreq = 433.92;
                    analyzerPeakRssi = -120;
                    analyzerLocked = false;
                    ui_draw_freq_analyzer_screen();
                } else if (menuIndex == 4) {
                    radio_set_idle();
                    currentState = MODE_SPECTRUM;
                    ui_reset_spectrum_peaks();
                    ui_draw_spectrum_screen(currentSpectrumBandIdx);
                } else if (menuIndex == 5) {
                    currentState = MODE_CARRIER_GEN;
                    isTransmittingCarrier = false;
                    ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, false);
                } else if (menuIndex == 6) {
                    radio_set_idle();
                    currentState = MODE_24G_MENU;
                    menu24gIndex = 0;
                    ui_draw_24g_menu(menu24gIndex);
                } else if (menuIndex == 7) {
                    currentState = MODE_BATTERY_INFO;
                    battery_update();
                    ui_draw_battery_screen(battery_get_millivolts(), battery_get_percentage(), battery_is_charging());
                } else if (menuIndex == 8) {
                    currentState = MODE_SETTINGS;
                    ui_draw_settings_screen(ble_proto_is_enabled(), ble_proto_is_connected());
                }
            }
            break;

        case MODE_TX_REMOTE: {
            int maxTx = totalSaved + 1; // existing signals + [+] NEW SIGNAL
            if (evt == BTN_EVT_UP_CLICK) {
                txIndex = (txIndex - 1 + maxTx) % maxTx;
                ui_draw_tx_menu(savedList, totalSaved, txIndex);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                txIndex = (txIndex + 1) % maxTx;
                ui_draw_tx_menu(savedList, totalSaved, txIndex);
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (txIndex < totalSaved) {
                    currentState = MODE_SIGNAL_SUBMENU;
                    sigSubMenuIdx = 0;
                    ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                } else {
                    // [+] ADD NEW SIGNAL
                    editSignalIndex = -1;
                    editorMenuIdx = 0;
                    memset(&editSignal, 0, sizeof(SavedSignal));
                    snprintf(editSignal.name, sizeof(editSignal.name), "Sig_%02d", totalSaved + 1);
                    editSignal.type = SIG_SMART_EV1527;
                    editSignal.freq = 433.92f;
                    editSignal.repeats = 5;
                    strncpy(editSignal.protoName, "EV1527", 7);
                    editSignal.code = (uint32_t)(esp_random() & 0xFFFFF);
                    editSignal.bitLength = 24;
                    editSignal.pulseLength = 320;
                    editSignal.protocol = 1;
                    editSignal.keeloqBtn = 1;
                    currentState = MODE_SIGNAL_EDITOR;
                    ui_draw_signal_editor(editSignal, editorMenuIdx, true);
                }
            }
            break;
        }

        case MODE_SIGNAL_SUBMENU: {
            SavedSignal &sig = savedList[txIndex];
            int totalSubActions = (sig.type == SIG_SMART_EV1527) ? 7 : 6;

            if (evt == BTN_EVT_UP_CLICK) {
                sigSubMenuIdx = (sigSubMenuIdx - 1 + totalSubActions) % totalSubActions;
                ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                sigSubMenuIdx = (sigSubMenuIdx + 1) % totalSubActions;
                ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (sig.type == SIG_SMART_EV1527) {
                    if (sigSubMenuIdx == 0) {
                        // 1. Transmit selected button
                        ui_show_tx_anim();
                        radio_transmit_ev1527(sig.code, sig.keeloqBtn, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        delay(300);
                        ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                    } else if (sigSubMenuIdx == 1) {
                        // 2. Button cycle (Btn 1 -> Btn 2 -> Btn 3 -> Btn 4 -> ALL -> Btn 1)
                        sig.keeloqBtn = (sig.keeloqBtn % 5) + 1;
                        storage_save_signals(savedList, totalSaved);
                        ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                    } else if (sigSubMenuIdx == 2) {
                        // 3. Repeats Toggle
                        if (sig.repeats <= 1) sig.repeats = 3;
                        else if (sig.repeats <= 3) sig.repeats = 5;
                        else if (sig.repeats <= 5) sig.repeats = 10;
                        else sig.repeats = 1;
                        storage_save_signals(savedList, totalSaved);
                        ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                    } else if (sigSubMenuIdx == 3) {
                        // 4. Edit Signal
                        editSignal = savedList[txIndex];
                        editSignalIndex = txIndex;
                        editorMenuIdx = 0;
                        currentState = MODE_SIGNAL_EDITOR;
                        ui_draw_signal_editor(editSignal, editorMenuIdx, false);
                    } else if (sigSubMenuIdx == 4) {
                        // 5. Waveform / Info
                        currentState = MODE_SIGNAL_WAVEFORM;
                        ui_draw_signal_waveform(savedList[txIndex]);
                    } else if (sigSubMenuIdx == 5) {
                        // 6. Delete Signal
                        storage_delete_signal(txIndex, savedList, totalSaved);
                        ui_draw_toast("SIGNAL DELETED!");
                        delay(650);
                        if (txIndex >= totalSaved && totalSaved > 0) {
                            txIndex = totalSaved - 1;
                        }
                        currentState = MODE_TX_REMOTE;
                        ui_draw_tx_menu(savedList, totalSaved, txIndex);
                    } else if (sigSubMenuIdx == 6) {
                        // 7. Back to List
                        currentState = MODE_TX_REMOTE;
                        ui_draw_tx_menu(savedList, totalSaved, txIndex);
                    }
                } else {
                    if (sigSubMenuIdx == 0) {
                        // 1. Transmit (TX)
                        ui_show_tx_anim();
                        if (sig.type == SIG_FIXED) {
                            radio_transmit_fixed(sig.code, sig.bitLength, sig.pulseLength, sig.protocol, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_CAME12) {
                            radio_transmit_came12(sig.code, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_CAME24) {
                            radio_transmit_came24(sig.code, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_NICE12) {
                            radio_transmit_nice12(sig.code, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_LINEAR10) {
                            radio_transmit_linear10(sig.code, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_SOMFY_RTS) {
                            radio_transmit_somfy_rts(sig.code, sig.klqCounter, sig.keeloqBtn, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else if (sig.type == SIG_DOORHAN) {
                            radio_transmit_doorhan(sig.keeloqSerial, sig.klqCounter, sig.keeloqBtn, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        } else {
                            radio_transmit_raw_pulses(sig.rawPulses, sig.rawCount, sig.freq, PA_LEVELS[currentPaIdx], sig.repeats);
                        }
                        delay(300);
                        ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                    } else if (sigSubMenuIdx == 1) {
                        // 2. Repeats Toggle (1x -> 3x -> 5x -> 10x)
                        if (sig.repeats <= 1) sig.repeats = 3;
                        else if (sig.repeats <= 3) sig.repeats = 5;
                        else if (sig.repeats <= 5) sig.repeats = 10;
                        else sig.repeats = 1;
                        storage_save_signals(savedList, totalSaved);
                        ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
                    } else if (sigSubMenuIdx == 2) {
                        // 3. Edit Signal
                        editSignal = savedList[txIndex];
                        editSignalIndex = txIndex;
                        editorMenuIdx = 0;
                        currentState = MODE_SIGNAL_EDITOR;
                        ui_draw_signal_editor(editSignal, editorMenuIdx, false);
                    } else if (sigSubMenuIdx == 3) {
                        // 4. Waveform / Info
                        currentState = MODE_SIGNAL_WAVEFORM;
                        ui_draw_signal_waveform(savedList[txIndex]);
                    } else if (sigSubMenuIdx == 4) {
                        // 5. Delete Signal
                        storage_delete_signal(txIndex, savedList, totalSaved);
                        ui_draw_toast("SIGNAL DELETED!");
                        delay(650);
                        if (txIndex >= totalSaved && totalSaved > 0) {
                            txIndex = totalSaved - 1;
                        }
                        currentState = MODE_TX_REMOTE;
                        ui_draw_tx_menu(savedList, totalSaved, txIndex);
                    } else if (sigSubMenuIdx == 5) {
                        // 6. Back to List
                        currentState = MODE_TX_REMOTE;
                        ui_draw_tx_menu(savedList, totalSaved, txIndex);
                    }
                }
            }
            break;
        }

        case MODE_SIGNAL_EDITOR: {
            if (evt == BTN_EVT_UP_CLICK) {
                editorMenuIdx = (editorMenuIdx - 1 + 9) % 9;
                ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                editorMenuIdx = (editorMenuIdx + 1) % 9;
                ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (editorMenuIdx == 0) {
                    // Type: EV1527 -> PT2262 -> Came -> Nice -> EV1527
                    if (strcmp(editSignal.protoName, "EV1527") == 0) {
                        strncpy(editSignal.protoName, "PT2262", 7);
                        editSignal.type = SIG_FIXED;
                        editSignal.bitLength = 24;
                        editSignal.pulseLength = 350;
                        editSignal.protocol = 1;
                    } else if (strcmp(editSignal.protoName, "PT2262") == 0) {
                        strncpy(editSignal.protoName, "Came", 7);
                        editSignal.type = SIG_FIXED;
                        editSignal.bitLength = 12;
                        editSignal.pulseLength = 320;
                        editSignal.protocol = 2;
                        editSignal.code &= 0xFFF;
                    } else if (strcmp(editSignal.protoName, "Came") == 0) {
                        strncpy(editSignal.protoName, "Nice", 7);
                        editSignal.type = SIG_FIXED;
                        editSignal.bitLength = 12;
                        editSignal.pulseLength = 700;
                        editSignal.protocol = 3;
                        editSignal.code &= 0xFFF;
                    } else {
                        strncpy(editSignal.protoName, "EV1527", 7);
                        editSignal.type = SIG_SMART_EV1527;
                        editSignal.bitLength = 24;
                        editSignal.pulseLength = 320;
                        editSignal.protocol = 1;
                        editSignal.keeloqBtn = 1;
                    }
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 1) {
                    // Name: Cycle common preset names
                    const char* presetNames[] = {"Gate", "Barrier", "CarKey", "Light", "Alarm", "Remote", "Socket", "Door", "Sig_01", "Sig_02"};
                    int curP = 0;
                    for (int p = 0; p < 10; p++) {
                        if (strncmp(editSignal.name, presetNames[p], 4) == 0) {
                            curP = (p + 1) % 10;
                            break;
                        }
                    }
                    strncpy(editSignal.name, presetNames[curP], 15);
                    editSignal.name[15] = '\0';
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 2) {
                    // Freq: Cycle FREQ_PRESETS
                    int fIdx = 0;
                    for (int f = 0; f < NUM_FREQ_PRESETS; f++) {
                        if (fabs(editSignal.freq - FREQ_PRESETS[f]) < 0.01) {
                            fIdx = (f + 1) % NUM_FREQ_PRESETS;
                            break;
                        }
                    }
                    editSignal.freq = FREQ_PRESETS[fIdx];
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 3) {
                    // Btn / Bits
                    if (editSignal.type == SIG_SMART_EV1527) {
                        editSignal.keeloqBtn = (editSignal.keeloqBtn % 5) + 1;
                    } else if (editSignal.type == SIG_FIXED) {
                        if (editSignal.pulseLength <= 320) editSignal.pulseLength = 350;
                        else if (editSignal.pulseLength <= 350) editSignal.pulseLength = 450;
                        else editSignal.pulseLength = 320;
                    }
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 4) {
                    // Code: Generate random address
                    uint32_t mask = (editSignal.bitLength == 12) ? 0xFFF : 0xFFFFF;
                    editSignal.code = (uint32_t)(esp_random() & mask);
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 5) {
                    // Repeats: 1 -> 3 -> 5 -> 7 -> 10 -> 1
                    if (editSignal.repeats <= 1) editSignal.repeats = 3;
                    else if (editSignal.repeats <= 3) editSignal.repeats = 5;
                    else if (editSignal.repeats <= 5) editSignal.repeats = 7;
                    else if (editSignal.repeats <= 7) editSignal.repeats = 10;
                    else editSignal.repeats = 1;
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 6) {
                    // Test Transmit
                    ui_show_tx_anim();
                    if (editSignal.type == SIG_RAW && editSignal.rawCount > 0) {
                        radio_transmit_raw_pulses(editSignal.rawPulses, editSignal.rawCount, editSignal.freq, PA_LEVELS[currentPaIdx], editSignal.repeats);
                    } else if (editSignal.type == SIG_SMART_EV1527) {
                        radio_transmit_ev1527(editSignal.code, editSignal.keeloqBtn, editSignal.freq, PA_LEVELS[currentPaIdx], editSignal.repeats);
                    } else {
                        radio_transmit_fixed(editSignal.code, editSignal.bitLength, editSignal.pulseLength, editSignal.protocol, editSignal.freq, PA_LEVELS[currentPaIdx], editSignal.repeats);
                    }
                    delay(300);
                    ui_draw_signal_editor(editSignal, editorMenuIdx, (editSignalIndex == -1));
                } else if (editorMenuIdx == 7) {
                    // Save Signal
                    if (editSignalIndex == -1) {
                        if (totalSaved < MAX_SAVED_SIGNALS) {
                            savedList[totalSaved++] = editSignal;
                            txIndex = totalSaved - 1;
                        } else {
                            savedList[MAX_SAVED_SIGNALS - 1] = editSignal;
                            txIndex = MAX_SAVED_SIGNALS - 1;
                        }
                    } else {
                        savedList[editSignalIndex] = editSignal;
                        txIndex = editSignalIndex;
                    }
                    storage_save_signals(savedList, totalSaved);
                    ui_draw_toast("SIGNAL SAVED!");
                    delay(600);
                    currentState = MODE_TX_REMOTE;
                    ui_draw_tx_menu(savedList, totalSaved, txIndex);
                } else if (editorMenuIdx == 8) {
                    // Cancel
                    currentState = MODE_TX_REMOTE;
                    ui_draw_tx_menu(savedList, totalSaved, (editSignalIndex >= 0 ? editSignalIndex : 0));
                }
            }
            break;
        }

        case MODE_SIGNAL_WAVEFORM:
            if (evt != BTN_EVT_NONE) {
                currentState = MODE_SIGNAL_SUBMENU;
                ui_draw_signal_submenu(savedList[txIndex], sigSubMenuIdx);
            }
            break;

        case MODE_RX_SNIFFER:
            // Выход по долгому удержанию OK
            break;

        case MODE_RX_DETAILS:
            if (evt == BTN_EVT_UP_CLICK) {
                ui_show_tx_anim();
                radio_transmit_fixed(capturedCode, capturedBitLen, capturedPulseLen, capturedProto, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], 5);
                delay(300);
                ui_draw_rx_details(capturedCode, capturedBitLen, capturedPulseLen, capturedProto);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                currentState = MODE_RX_SNIFFER;
                ui_draw_rx_screen(currentFreqIdx);
                radio_set_rx(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
            } else if (evt == BTN_EVT_OK_CLICK) {
                char name[16];
                char triStr[13];
                if (capturedBitLen == 24) {
                    if (radio_decode_pt2262_tristate(capturedCode, 24, triStr)) {
                        snprintf(name, sizeof(name), "Socket #%d", totalSaved + 1);
                    } else {
                        snprintf(name, sizeof(name), "Smart #%d", totalSaved + 1);
                    }
                } else if (capturedBitLen == 12) {
                    snprintf(name, sizeof(name), "Gate #%d", totalSaved + 1);
                } else {
                    snprintf(name, sizeof(name), "Key #%d", totalSaved + 1);
                }
                storage_add_fixed(name, capturedCode, capturedBitLen, capturedPulseLen, capturedProto, FREQ_PRESETS[currentFreqIdx], savedList, totalSaved);
                ui_draw_toast("SAVED TO NVS!");
                delay(650);
                currentState = MODE_TX_REMOTE;
                txIndex = totalSaved - 1;
                ui_draw_tx_menu(savedList, totalSaved, txIndex);
            }
            break;

        case MODE_RAW_SNIFFER:
            // Выход по долгому удержанию OK
            break;

        case MODE_RAW_DETAILS:
            if (evt == BTN_EVT_UP_CLICK) {
                ui_show_tx_anim();
                if (capturedRawPkt.decodedCode != 0) {
                    radio_transmit_fixed(capturedRawPkt.decodedCode, capturedRawPkt.decodedBitLen, capturedRawPkt.decodedPulseLen, 1, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], 5);
                } else {
                    radio_transmit_raw_pulses(capturedRawPkt.pulses, capturedRawPkt.pulseCount, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], 5);
                }
                delay(300);
                ui_draw_raw_details(capturedRawPkt);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                currentState = MODE_RAW_SNIFFER;
                ui_draw_raw_screen(currentFreqIdx, currentSquelchIdx);
                radio_start_raw_sniffer(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], SQUELCH_LEVELS[currentSquelchIdx]);
            } else if (evt == BTN_EVT_OK_CLICK) {
                char name[16];
                if (capturedRawPkt.decodedCode != 0) {
                    snprintf(name, sizeof(name), "%s #%d", capturedRawPkt.protoName, totalSaved + 1);
                    storage_add_fixed(name, capturedRawPkt.decodedCode, capturedRawPkt.decodedBitLen, capturedRawPkt.decodedPulseLen, 1, FREQ_PRESETS[currentFreqIdx], savedList, totalSaved);
                    ui_draw_toast("SAVED DECODED!");
                } else if (capturedRawPkt.isKeeloq) {
                    snprintf(name, sizeof(name), "KLQ #%d", totalSaved + 1);
                    storage_add_raw(name, capturedRawPkt, FREQ_PRESETS[currentFreqIdx], savedList, totalSaved);
                    ui_draw_toast("SAVED TO NVS!");
                } else if (strcmp(capturedRawPkt.protoName, "RAW") != 0) {
                    snprintf(name, sizeof(name), "%s #%d", capturedRawPkt.protoName, totalSaved + 1);
                    storage_add_raw(name, capturedRawPkt, FREQ_PRESETS[currentFreqIdx], savedList, totalSaved);
                    ui_draw_toast("SAVED TO NVS!");
                } else {
                    snprintf(name, sizeof(name), "RAW #%d", totalSaved + 1);
                    storage_add_raw(name, capturedRawPkt, FREQ_PRESETS[currentFreqIdx], savedList, totalSaved);
                    ui_draw_toast("SAVED TO NVS!");
                }
                delay(650);
                currentState = MODE_TX_REMOTE;
                txIndex = totalSaved - 1;
                ui_draw_tx_menu(savedList, totalSaved, txIndex);
            }
            break;

        case MODE_FREQ_ANALYZER:
            if (evt == BTN_EVT_UP_CLICK || evt == BTN_EVT_DOWN_CLICK) {
                analyzerPeakRssi = -120;
                analyzerLocked = false;
                ui_update_freq_analyzer(analyzerPeakFreq, analyzerPeakRssi, analyzerLocked);
            } else if (evt == BTN_EVT_OK_CLICK) {
                // Настраиваемся на найденную частоту и переходим в RAW Sniffer!
                for (int i = 0; i < NUM_FREQ_PRESETS; i++) {
                    if (fabs(FREQ_PRESETS[i] - analyzerPeakFreq) < 0.15) {
                        currentFreqIdx = i;
                        break;
                    }
                }
                storage_save_settings(currentFreqIdx, currentPaIdx, currentSquelchIdx, currentSpectrumBandIdx);
                radio_apply_config(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);

                ui_draw_toast("TUNED TO FREQ!");
                delay(650);
                currentState = MODE_RAW_SNIFFER;
                ui_draw_raw_screen(currentFreqIdx, currentSquelchIdx);
                radio_start_raw_sniffer(FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx], SQUELCH_LEVELS[currentSquelchIdx]);
            }
            break;

        case MODE_SPECTRUM:
            if (evt == BTN_EVT_UP_CLICK) {
                currentSpectrumBandIdx = (currentSpectrumBandIdx - 1 + NUM_SPECTRUM_BANDS) % NUM_SPECTRUM_BANDS;
                ui_reset_spectrum_peaks();
                ui_draw_spectrum_screen(currentSpectrumBandIdx);
                storage_save_settings(currentFreqIdx, currentPaIdx, currentSquelchIdx, currentSpectrumBandIdx);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                currentSpectrumBandIdx = (currentSpectrumBandIdx + 1) % NUM_SPECTRUM_BANDS;
                ui_reset_spectrum_peaks();
                ui_draw_spectrum_screen(currentSpectrumBandIdx);
                storage_save_settings(currentFreqIdx, currentPaIdx, currentSquelchIdx, currentSpectrumBandIdx);
            } else if (evt == BTN_EVT_OK_CLICK) {
                ui_reset_spectrum_peaks();
            }
            break;

        case MODE_CARRIER_GEN:
            if (evt == BTN_EVT_UP_CLICK) {
                currentCarrierMode = (CarrierType)((currentCarrierMode - 1 + 3) % 3);
                if (isTransmittingCarrier) {
                    radio_set_carrier(currentCarrierMode, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
                }
                ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, isTransmittingCarrier);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                currentCarrierMode = (CarrierType)((currentCarrierMode + 1) % 3);
                if (isTransmittingCarrier) {
                    radio_set_carrier(currentCarrierMode, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
                }
                ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, isTransmittingCarrier);
            } else if (evt == BTN_EVT_OK_CLICK) {
                isTransmittingCarrier = !isTransmittingCarrier;
                if (isTransmittingCarrier) {
                    radio_set_carrier(currentCarrierMode, FREQ_PRESETS[currentFreqIdx], PA_LEVELS[currentPaIdx]);
                } else {
                    digitalWrite(CC_GDO0, LOW);
                    radio_set_idle();
                }
                ui_draw_carrier_gen(currentFreqIdx, currentPaIdx, currentCarrierMode, isTransmittingCarrier);
            }
            break;

        case MODE_SETTINGS:
            if (evt == BTN_EVT_OK_CLICK) {
                bool newState = !ble_proto_is_enabled();
                ble_proto_set_enabled(newState);
                ui_draw_settings_screen(newState, ble_proto_is_connected());
            }
            break;

        case MODE_SYS_INFO:
            if (evt == BTN_EVT_UP_CLICK || evt == BTN_EVT_DOWN_CLICK || evt == BTN_EVT_OK_CLICK) {
                currentState = MODE_SETTINGS;
                ui_draw_settings_screen(ble_proto_is_enabled(), ble_proto_is_connected());
            }
            break;

        case MODE_24G_MENU:
            if (evt == BTN_EVT_UP_CLICK) {
                menu24gIndex = (menu24gIndex - 1 + 6) % 6;
                ui_draw_24g_menu(menu24gIndex);
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                menu24gIndex = (menu24gIndex + 1) % 6;
                ui_draw_24g_menu(menu24gIndex);
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (menu24gIndex == 0) {
                    currentState = MODE_WIFI_SCAN;
                    wifiApIndex = 0;
                    wifi_tools_scan_start();
                    ui_draw_wifi_scan_screen(wifiApIndex, true);
                } else if (menu24gIndex == 1) {
                    currentState = MODE_WIFI_CHANNELS;
                    if (wifi_tools_get_ap_count() == 0) {
                        wifi_tools_scan_start();
                    }
                    ui_draw_wifi_channels_screen();
                } else if (menu24gIndex == 2) {
                    currentState = MODE_WIFI_SNIFFER;
                    wifi_tools_sniffer_start();
                    ui_draw_wifi_sniffer_screen();
                } else if (menu24gIndex == 3) {
                    currentState = MODE_BLE_SCAN;
                    bleDevIndex = 0;
                    wifi_tools_ble_scan_start();
                    ui_draw_ble_scan_screen(bleDevIndex, true);
                } else if (menu24gIndex == 4) {
                    currentState = MODE_BLE_SPAM;
                    bleSpamMode = 0;
                    wifi_tools_ble_spam_start(bleSpamMode);
                    ui_draw_ble_spam_screen(bleSpamMode, 0);
                } else if (menu24gIndex == 5) {
                    currentState = MODE_WIFI_BEACON_SPAM;
                    wifi_tools_beacon_spam_start();
                    ui_draw_wifi_beacon_spam_screen(0);
                }
            }
            break;

        case MODE_WIFI_SCAN: {
            int apCount = wifi_tools_get_ap_count();
            if (evt == BTN_EVT_UP_CLICK) {
                if (apCount > 0) {
                    wifiApIndex = (wifiApIndex - 1 + apCount) % apCount;
                    ui_draw_wifi_scan_screen(wifiApIndex, false);
                } else {
                    wifi_tools_scan_start();
                    ui_draw_wifi_scan_screen(0, true);
                }
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                if (apCount > 0) {
                    wifiApIndex = (wifiApIndex + 1) % apCount;
                    ui_draw_wifi_scan_screen(wifiApIndex, false);
                }
            } else if (evt == BTN_EVT_OK_CLICK) {
                if (apCount > 0) {
                    currentState = MODE_WIFI_DETAILS;
                    ui_draw_wifi_details_screen(wifiApIndex);
                } else {
                    wifi_tools_scan_start();
                    ui_draw_wifi_scan_screen(0, true);
                }
            }
            break;
        }

        case MODE_WIFI_DETAILS:
            if (evt == BTN_EVT_OK_CLICK || evt == BTN_EVT_UP_CLICK || evt == BTN_EVT_DOWN_CLICK) {
                currentState = MODE_WIFI_SCAN;
                ui_draw_wifi_scan_screen(wifiApIndex, false);
            }
            break;

        case MODE_WIFI_CHANNELS:
            if (evt == BTN_EVT_OK_CLICK || evt == BTN_EVT_UP_CLICK) {
                wifi_tools_scan_start();
                ui_draw_wifi_channels_screen();
            }
            break;

        case MODE_WIFI_SNIFFER:
            if (evt == BTN_EVT_OK_CLICK) {
                wifi_tools_reset_sniffer_counters();
                ui_draw_wifi_sniffer_screen();
            }
            break;

        case MODE_BLE_SCAN: {
            int devCount = wifi_tools_get_ble_count();
            if (evt == BTN_EVT_UP_CLICK) {
                if (devCount > 0) {
                    bleDevIndex = (bleDevIndex - 1 + devCount) % devCount;
                    ui_draw_ble_scan_screen(bleDevIndex, false);
                }
            } else if (evt == BTN_EVT_DOWN_CLICK) {
                if (devCount > 0) {
                    bleDevIndex = (bleDevIndex + 1) % devCount;
                    ui_draw_ble_scan_screen(bleDevIndex, false);
                }
            } else if (evt == BTN_EVT_OK_CLICK) {
                wifi_tools_ble_scan_start();
                ui_draw_ble_scan_screen(bleDevIndex, true);
            }
            break;
        }

        case MODE_BLE_SPAM:
            if (evt == BTN_EVT_UP_CLICK || evt == BTN_EVT_DOWN_CLICK) {
                bleSpamMode = (bleSpamMode + 1) % 3;
                wifi_tools_ble_spam_start(bleSpamMode);
                ui_draw_ble_spam_screen(bleSpamMode, wifi_tools_get_ble_spam_count());
            } else if (evt == BTN_EVT_OK_CLICK) {
                wifi_tools_stop_all();
                currentState = MODE_24G_MENU;
                ui_draw_24g_menu(menu24gIndex);
            }
            break;

        case MODE_WIFI_BEACON_SPAM:
            if (evt == BTN_EVT_OK_CLICK) {
                wifi_tools_stop_all();
                currentState = MODE_24G_MENU;
                ui_draw_24g_menu(menu24gIndex);
            }
            break;



        case MODE_BATTERY_INFO:
            if (evt == BTN_EVT_OK_CLICK || evt == BTN_EVT_UP_CLICK || evt == BTN_EVT_DOWN_CLICK) {
                currentState = MODE_MAIN_MENU;
                ui_draw_main_menu(menuIndex);
            }
            break;
    }

    // -------------------------------------------------------------
    // ПЕРИОДИЧЕСКИЕ ФОНОВЫЕ ПРОЦЕССЫ
    // -------------------------------------------------------------
    battery_update();
    if (currentState == MODE_RX_SNIFFER) {
        if (millis() - lastRxAnimUpdate > 250) {
            lastRxAnimUpdate = millis();
            ui_update_rx_anim();
        }

        if (radio_check_rx(capturedCode, capturedBitLen, capturedPulseLen, capturedProto)) {
            currentState = MODE_RX_DETAILS;
            ui_draw_rx_details(capturedCode, capturedBitLen, capturedPulseLen, capturedProto);
            serial_proto_broadcast_fixed_rx(capturedCode, capturedBitLen, capturedPulseLen, capturedProto, FREQ_PRESETS[currentFreqIdx], radio_get_rssi());
        }
    }

    if (currentState == MODE_RAW_SNIFFER) {
        if (millis() - lastRxAnimUpdate > 100) {
            lastRxAnimUpdate = millis();
            ui_update_raw_anim(SQUELCH_LEVELS[currentSquelchIdx]);
        }

        if (radio_check_raw(capturedRawPkt, SQUELCH_LEVELS[currentSquelchIdx])) {
            currentState = MODE_RAW_DETAILS;
            ui_draw_raw_details(capturedRawPkt);
            serial_proto_broadcast_raw_rx(capturedRawPkt, FREQ_PRESETS[currentFreqIdx], radio_get_rssi());
        }
    }

    if (currentState == MODE_FREQ_ANALYZER) {
        if (millis() - lastFreqScanUpdate > 90) {
            lastFreqScanUpdate = millis();
            int r = -130;
            float f = radio_scan_fast_peak(r);

            if (r > analyzerPeakRssi || (millis() % 2500 < 100)) {
                analyzerPeakRssi = r;
                analyzerPeakFreq = f;
            }
            analyzerLocked = (analyzerPeakRssi >= -65);
            ui_update_freq_analyzer(analyzerPeakFreq, analyzerPeakRssi, analyzerLocked);
        }
    }

    if (currentState == MODE_SPECTRUM) {
        if (millis() - lastSpectrumUpdate > 25) {
            lastSpectrumUpdate = millis();
            ui_update_spectrum(currentSpectrumBandIdx);
        }
    }

    if (currentState == MODE_CARRIER_GEN && isTransmittingCarrier) {
        radio_update_carrier_loop(currentCarrierMode);
    }

    if (currentState == MODE_WIFI_SCAN) {
        static bool scanWasPending = true;
        if (wifi_tools_scan_is_done()) {
            if (scanWasPending) {
                scanWasPending = false;
                ui_draw_wifi_scan_screen(wifiApIndex, false);
            }
        } else {
            scanWasPending = true;
            if (millis() - last24gUpdate > 200) {
                last24gUpdate = millis();
                ui_draw_wifi_scan_screen(wifiApIndex, true);
            }
        }
    }

    if (currentState == MODE_WIFI_SNIFFER) {
        wifi_tools_sniffer_update();
        if (millis() - last24gUpdate > 200) {
            last24gUpdate = millis();
            ui_draw_wifi_sniffer_screen();
        }
    }

    if (currentState == MODE_BLE_SCAN) {
        static int lastSeenBleCount = -1;
        int curCount = wifi_tools_get_ble_count();
        if (curCount != lastSeenBleCount || (millis() - last24gUpdate > 400)) {
            lastSeenBleCount = curCount;
            last24gUpdate = millis();
            ui_draw_ble_scan_screen(bleDevIndex, false);
        }
    }

    if (currentState == MODE_BLE_SPAM) {
        wifi_tools_ble_spam_loop();
        if (millis() - last24gUpdate > 250) {
            last24gUpdate = millis();
            ui_draw_ble_spam_screen(bleSpamMode, wifi_tools_get_ble_spam_count());
        }
    }

    if (currentState == MODE_WIFI_BEACON_SPAM) {
        wifi_tools_beacon_spam_loop();
        if (millis() - last24gUpdate > 250) {
            last24gUpdate = millis();
            ui_draw_wifi_beacon_spam_screen(wifi_tools_get_beacon_spam_count());
        }
    }

    if (currentState == MODE_BATTERY_INFO) {
        if (millis() - lastBatteryUpdate > 500) {
            lastBatteryUpdate = millis();
            ui_draw_battery_screen(battery_get_millivolts(), battery_get_percentage(), battery_is_charging());
        }
    }

    yield();
}
