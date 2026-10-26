#pragma once
#include <Arduino.h>

// =========================================================================
// GHOSTRF 2.4 GHz TOOLKIT (Wi-Fi & BLE Engine for ESP32-C3 Onboard Antenna)
// =========================================================================

struct WifiApInfo {
    char ssid[24];
    int8_t rssi;
    uint8_t channel;
    uint8_t auth; // 0=Open, 1=WEP, 2=WPA, 3=WPA2, 4=WPA3
    uint8_t bssid[6];
};

struct BleDevInfo {
    char name[16];
    char mac[18];
    int8_t rssi;
    char tag[8]; // "Apple", "FastPr", "Flppr", "BLE"
};

// Wi-Fi AP Scanner
void wifi_tools_init();
void wifi_tools_scan_start();
bool wifi_tools_scan_is_done();
int  wifi_tools_get_ap_count();
const WifiApInfo* wifi_tools_get_ap(int index);
const char* wifi_tools_get_auth_str(uint8_t auth);

// 2.4 GHz Channels Analyzer
void wifi_tools_get_channel_stats(uint8_t apCounts[14], int8_t peakRssi[14]);

// Promiscuous Sniffer & Deauth Alert Monitor
void wifi_tools_sniffer_start();
void wifi_tools_sniffer_stop();
void wifi_tools_sniffer_update();
uint32_t wifi_tools_get_total_pkts();
uint32_t wifi_tools_get_mgmt_pkts();
uint32_t wifi_tools_get_data_pkts();
uint32_t wifi_tools_get_deauth_pkts();
uint32_t wifi_tools_get_probe_pkts();
uint8_t  wifi_tools_get_sniffer_channel();
const char* wifi_tools_get_last_deauth_mac();
const char* wifi_tools_get_last_deauth_ap();
void wifi_tools_reset_sniffer_counters();

// BLE Scanner / Radar
void wifi_tools_ble_scan_start();
void wifi_tools_ble_scan_stop();
int  wifi_tools_get_ble_count();
const BleDevInfo* wifi_tools_get_ble_dev(int index);

// BLE Beacon Spammer (Apple Action Modal / Fast Pair / GHOSTRF Beacon)
void wifi_tools_ble_spam_start(uint8_t mode);
void wifi_tools_ble_spam_stop();
void wifi_tools_ble_spam_loop();
uint32_t wifi_tools_get_ble_spam_count();

// Wi-Fi Beacon Spam (Multi-SSID Broadcast)
void wifi_tools_beacon_spam_start();
void wifi_tools_beacon_spam_stop();
void wifi_tools_beacon_spam_loop();
uint32_t wifi_tools_get_beacon_spam_count();

// Полная остановка 2.4ГГц служб при выходе в меню
void wifi_tools_stop_all();
