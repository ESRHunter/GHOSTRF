#pragma once
#include <Arduino.h>
#include "types.h"
#include "config.h"

extern const SpectrumBand SPECTRUM_BANDS[NUM_SPECTRUM_BANDS];

void ui_init();
void ui_draw_splash();
void ui_draw_header(const char* title, const char* badge = nullptr);
void ui_draw_footer(const char* text);

void ui_draw_main_menu(int menuIndex);
void ui_draw_tx_menu(const SavedSignal* sigList, int sigCount, int txIndex);
void ui_draw_signal_submenu(const SavedSignal &sig, int subMenuIdx);
void ui_draw_signal_waveform(const SavedSignal &sig);
void ui_draw_signal_editor(const SavedSignal &sig, int editorMenuIdx, bool isNewSignal);
void open_new_signal_editor();
void ui_show_tx_anim();

void ui_draw_rx_screen(int freqIdx);
void ui_update_rx_anim();
void ui_draw_rx_details(uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol);
void ui_draw_toast(const char* message);

// RAW & KeeLoq UI
void ui_draw_raw_screen(int freqIdx, int squelchIdx);
void ui_update_raw_anim(int squelchDbm);
void ui_draw_raw_details(const RawPacket &pkt);

// Автодетектор / Частотомер
void ui_draw_freq_analyzer_screen();
void ui_update_freq_analyzer(float peakFreq, int peakRssi, bool locked);

void ui_draw_spectrum_screen(int bandIdx);
void ui_update_spectrum(int bandIdx);
void ui_reset_spectrum_peaks();

void ui_draw_carrier_gen(int freqIdx, int paIdx, CarrierType mode, bool isTransmitting);

// 2.4 GHz Toolkit UI
void ui_draw_24g_menu(int menuIndex);
void ui_draw_wifi_scan_screen(int selectedIndex, bool isScanning);
void ui_draw_wifi_details_screen(int selectedIndex);
void ui_draw_wifi_channels_screen();
void ui_draw_wifi_sniffer_screen();
void ui_draw_ble_scan_screen(int selectedIndex, bool isScanning);
void ui_draw_ble_spam_screen(uint8_t mode, uint32_t count);
void ui_draw_wifi_beacon_spam_screen(uint32_t count);

void ui_draw_settings_screen(bool bleEnabled, bool bleConnected);
void ui_draw_sys_info();

// Мониторинг аккумулятора UI
void ui_draw_battery_screen(uint16_t mv, uint8_t pct, bool isCharging);

void ui_set_display_power(bool on);
const uint8_t* ui_get_display_buffer();
int ui_get_display_buffer_size();
