#include "wifi_tools.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEAdvertising.h>

#define MAX_WIFI_APS 24
#define MAX_BLE_DEVS 18

static WifiApInfo scannedAps[MAX_WIFI_APS];
static int totalScannedAps = 0;
static bool scanRunning = false;

// Sniffer counters
static volatile uint32_t totalPackets = 0;
static volatile uint32_t mgmtPackets = 0;
static volatile uint32_t dataPackets = 0;
static volatile uint32_t deauthPackets = 0;
static volatile uint32_t probePackets = 0;
static uint8_t currentSnifferChannel = 1;
static unsigned long lastHopTime = 0;
static char lastDeauthVictim[18] = "None";
static char lastDeauthAP[18] = "None";
static bool snifferActive = false;

// BLE Scan & Spammer
static BleDevInfo scannedBles[MAX_BLE_DEVS];
static int totalScannedBles = 0;
static bool bleScanActive = false;
static BLEScan* pBLEScan = nullptr;

static bool bleSpamActive = false;
static uint8_t bleSpamMode = 0;
static uint32_t bleSpamCounter = 0;
static unsigned long lastBleSpamTime = 0;

// Wi-Fi Beacon Spam
static bool beaconSpamActive = false;
static uint32_t beaconSpamCounter = 0;
static unsigned long lastBeaconTime = 0;
static uint16_t beaconSeq[16] = {0};

static const char* BEACON_SSIDS[] = {
    "GHOSTRF_PENTEST",
    "GHOSTRF_SUBGHZ",
    "GHOSTRF_RADAR",
    "GHOSTRF_ESP32C3",
    "FREE_WIFI_GHOSTRF",
    "GHOSTRF_PORTABLE",
    "NEVER_GONNA_GIVE_YOU_UP",
    "NEVER_GONNA_LET_YOU_DOWN",
    "FBI_SURVEILLANCE_01",
    "VIRUS_INFECTED_AP",
    "FLIPPER_ESP32_RF",
    "SKYWIFI_GHOSTRF"
};
static const int NUM_BEACON_SSIDS = 12;

void wifi_tools_init() {
    // Начальное состояние — Wi-Fi и BLE выключены для экономии энергии
    WiFi.mode(WIFI_OFF);
}

// =========================================================================
// 1. WI-FI SCANNER
// =========================================================================

void wifi_tools_scan_start() {
    wifi_tools_stop_all();
    totalScannedAps = 0;
    scanRunning = true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(40);
    // Асинхронный скан каналов 1..13
    WiFi.scanNetworks(true, true);
}

bool wifi_tools_scan_is_done() {
    if (!scanRunning) return true;
    int16_t status = WiFi.scanComplete();
    if (status >= 0) {
        totalScannedAps = min((int)status, MAX_WIFI_APS);
        for (int i = 0; i < totalScannedAps; i++) {
            strncpy(scannedAps[i].ssid, WiFi.SSID(i).c_str(), sizeof(scannedAps[i].ssid) - 1);
            scannedAps[i].ssid[sizeof(scannedAps[i].ssid) - 1] = '\0';
            if (scannedAps[i].ssid[0] == '\0') {
                strncpy(scannedAps[i].ssid, "<Hidden SSID>", sizeof(scannedAps[i].ssid) - 1);
            }
            scannedAps[i].rssi = WiFi.RSSI(i);
            scannedAps[i].channel = WiFi.channel(i);
            scannedAps[i].auth = (uint8_t)WiFi.encryptionType(i);
            const uint8_t* bssid = WiFi.BSSID(i);
            if (bssid) {
                memcpy(scannedAps[i].bssid, bssid, 6);
            } else {
                memset(scannedAps[i].bssid, 0, 6);
            }
        }
        WiFi.scanDelete();
        scanRunning = false;
        return true;
    }
    return false;
}

int wifi_tools_get_ap_count() {
    return totalScannedAps;
}

const WifiApInfo* wifi_tools_get_ap(int index) {
    if (index >= 0 && index < totalScannedAps) {
        return &scannedAps[index];
    }
    return nullptr;
}

const char* wifi_tools_get_auth_str(uint8_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA";
        case WIFI_AUTH_WPA2_PSK: return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "EAP";
        case WIFI_AUTH_WPA3_PSK: return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "W2/W3";
        default: return "SEC";
    }
}

// =========================================================================
// 2. 2.4 GHz CHANNELS ANALYZER
// =========================================================================

void wifi_tools_get_channel_stats(uint8_t apCounts[14], int8_t peakRssi[14]) {
    for (int ch = 0; ch < 14; ch++) {
        apCounts[ch] = 0;
        peakRssi[ch] = -120;
    }
    for (int i = 0; i < totalScannedAps; i++) {
        uint8_t ch = scannedAps[i].channel;
        if (ch >= 1 && ch <= 13) {
            apCounts[ch]++;
            if (scannedAps[i].rssi > peakRssi[ch]) {
                peakRssi[ch] = scannedAps[i].rssi;
            }
        }
    }
}

// =========================================================================
// 3. PROMISCUOUS SNIFFER & DEAUTH MONITOR
// =========================================================================

static void IRAM_ATTR wifi_promiscuous_rx_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (!snifferActive || !buf) return;

    totalPackets++;

    if (type == WIFI_PKT_MGMT) {
        mgmtPackets++;
        const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
        const uint8_t* payload = pkt->payload;
        uint16_t len = pkt->rx_ctrl.sig_len;

        if (len >= 24) {
            uint8_t frameControl = payload[0];
            uint8_t subType = (frameControl >> 4) & 0x0F;

            // 0x0C = Deauthentication, 0x0A = Disassociation
            if (subType == 0x0C || subType == 0x0A) {
                deauthPackets++;
                snprintf(lastDeauthVictim, sizeof(lastDeauthVictim), "%02X:%02X:%02X:%02X:%02X:%02X",
                         payload[4], payload[5], payload[6], payload[7], payload[8], payload[9]);
                snprintf(lastDeauthAP, sizeof(lastDeauthAP), "%02X:%02X:%02X:%02X:%02X:%02X",
                         payload[16], payload[17], payload[18], payload[19], payload[20], payload[21]);
            } else if (subType == 0x04) {
                probePackets++;
            }
        }
    } else if (type == WIFI_PKT_DATA) {
        dataPackets++;
    }
}

void wifi_tools_sniffer_start() {
    wifi_tools_stop_all();
    wifi_tools_reset_sniffer_counters();

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(40);

    snifferActive = true;
    currentSnifferChannel = 1;
    lastHopTime = millis();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_rx_cb);
    esp_wifi_set_channel(currentSnifferChannel, WIFI_SECOND_CHAN_NONE);
}

void wifi_tools_sniffer_stop() {
    snifferActive = false;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    WiFi.mode(WIFI_OFF);
}

void wifi_tools_sniffer_update() {
    if (!snifferActive) return;
    unsigned long now = millis();
    if (now - lastHopTime >= 250) { // Переключение канала каждые 250 мс
        lastHopTime = now;
        currentSnifferChannel = (currentSnifferChannel % 13) + 1;
        esp_wifi_set_channel(currentSnifferChannel, WIFI_SECOND_CHAN_NONE);
    }
}

uint32_t wifi_tools_get_total_pkts() { return totalPackets; }
uint32_t wifi_tools_get_mgmt_pkts() { return mgmtPackets; }
uint32_t wifi_tools_get_data_pkts() { return dataPackets; }
uint32_t wifi_tools_get_deauth_pkts() { return deauthPackets; }
uint32_t wifi_tools_get_probe_pkts() { return probePackets; }
uint8_t  wifi_tools_get_sniffer_channel() { return currentSnifferChannel; }
const char* wifi_tools_get_last_deauth_mac() { return lastDeauthVictim; }
const char* wifi_tools_get_last_deauth_ap() { return lastDeauthAP; }

void wifi_tools_reset_sniffer_counters() {
    totalPackets = 0;
    mgmtPackets = 0;
    dataPackets = 0;
    deauthPackets = 0;
    probePackets = 0;
    strncpy(lastDeauthVictim, "None", sizeof(lastDeauthVictim));
    strncpy(lastDeauthAP, "None", sizeof(lastDeauthAP));
}

// =========================================================================
// 4. BLE SCANNER / RADAR (Асинхронный, без блокировок)
// =========================================================================

class GhostBLEAdvertisedCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        std::string addrStr = advertisedDevice.getAddress().toString();

        // Если устройство уже есть в списке — просто обновляем RSSI и имя (если пришел Scan Response)
        for (int i = 0; i < totalScannedBles; i++) {
            if (strcmp(scannedBles[i].mac, addrStr.c_str()) == 0) {
                scannedBles[i].rssi = advertisedDevice.getRSSI();
                if (scannedBles[i].name[0] == '<' && advertisedDevice.haveName()) {
                    std::string n = advertisedDevice.getName();
                    if (n.length() > 0) {
                        strncpy(scannedBles[i].name, n.c_str(), sizeof(scannedBles[i].name) - 1);
                        scannedBles[i].name[sizeof(scannedBles[i].name) - 1] = '\0';
                    }
                }
                return;
            }
        }

        if (totalScannedBles >= MAX_BLE_DEVS) return;

        BleDevInfo &dev = scannedBles[totalScannedBles++];
        strncpy(dev.mac, addrStr.c_str(), sizeof(dev.mac) - 1);
        dev.mac[sizeof(dev.mac) - 1] = '\0';
        dev.rssi = advertisedDevice.getRSSI();

        std::string name = advertisedDevice.getName();
        if (name.length() > 0) {
            strncpy(dev.name, name.c_str(), sizeof(dev.name) - 1);
        } else {
            strncpy(dev.name, "<Unnamed>", sizeof(dev.name) - 1);
        }
        dev.name[sizeof(dev.name) - 1] = '\0';

        // Детекция типа устройства (Компактные теги)
        strncpy(dev.tag, "BLE", sizeof(dev.tag) - 1);
        if (advertisedDevice.haveManufacturerData()) {
            std::string mData = advertisedDevice.getManufacturerData();
            if (mData.length() >= 2) {
                uint16_t mId = ((uint8_t)mData[1] << 8) | (uint8_t)mData[0];
                if (mId == 0x004C) {
                    strncpy(dev.tag, "APL", sizeof(dev.tag) - 1);
                }
            }
        }
        if (advertisedDevice.haveServiceUUID()) {
            std::string sUuid = advertisedDevice.getServiceUUID().toString();
            if (sUuid.find("fe2c") != std::string::npos) {
                strncpy(dev.tag, "G-FP", sizeof(dev.tag) - 1);
            } else if (sUuid.find("3080") != std::string::npos || name.find("Flipper") != std::string::npos) {
                strncpy(dev.tag, "FLIP", sizeof(dev.tag) - 1);
            }
        }
    }
};

static GhostBLEAdvertisedCallbacks staticBleCallbacks;

static void onBleScanCompleteCb(BLEScanResults results) {
    // Вызывается при завершении интервала сканирования
}

void wifi_tools_ble_scan_start() {
    wifi_tools_stop_all();
    totalScannedBles = 0;
    bleScanActive = true;

    pBLEScan = BLEDevice::getScan();
    if (pBLEScan) {
        pBLEScan->setAdvertisedDeviceCallbacks(&staticBleCallbacks, true);
        pBLEScan->setActiveScan(true);
        pBLEScan->setInterval(80);
        pBLEScan->setWindow(40);
        // Непрерывное фоновое сканирование (duration=0), НЕ блокирует процессор!
        pBLEScan->start(0, &onBleScanCompleteCb, false);
    }
}

void wifi_tools_ble_scan_stop() {
    bleScanActive = false;
    if (pBLEScan) {
        pBLEScan->stop();
        pBLEScan->clearResults();
    }
}

int wifi_tools_get_ble_count() {
    return totalScannedBles;
}

const BleDevInfo* wifi_tools_get_ble_dev(int index) {
    if (index >= 0 && index < totalScannedBles) {
        return &scannedBles[index];
    }
    return nullptr;
}

// =========================================================================
// 5. BLE BEACON SPAMMER (Apple Action Modal / Fast Pair / GHOSTRF)
// =========================================================================

void wifi_tools_ble_spam_start(uint8_t mode) {
    wifi_tools_stop_all();
    bleSpamMode = mode;
    bleSpamCounter = 0;
    bleSpamActive = true;
    lastBleSpamTime = millis();
}

void wifi_tools_ble_spam_stop() {
    bleSpamActive = false;
    BLEDevice::getAdvertising()->stop();
}

void wifi_tools_ble_spam_loop() {
    if (!bleSpamActive) return;
    unsigned long now = millis();
    if (now - lastBleSpamTime >= 80) { // Период 80 мс
        lastBleSpamTime = now;

        BLEAdvertising *pAdv = BLEDevice::getAdvertising();
        pAdv->stop();

        BLEAdvertisementData advData;

        if (bleSpamMode == 0) {
            // Apple Nearby Action Modal (AirPods Pro / Transfer Number)
            const uint8_t appleData[] = {
                0x1E,       // Длина
                0xFF,       // Manufacturer Specific Data
                0x4C, 0x00, // Apple Inc ID
                0x0F, 0x05, 0xC1, // Type: Nearby Action
                0x01, 0x60, 0x4C, 0x95, 0x00, 0x00, 0x10,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
            };
            advData.addData(std::string((char*)appleData, sizeof(appleData)));
        } else if (bleSpamMode == 1) {
            // Google Fast Pair Popup (Pixel Buds)
            const uint8_t fastPairData[] = {
                0x03, 0x03, 0x2C, 0xFE, // Service UUID 0xFE2C
                0x06, 0x16, 0x2C, 0xFE, // Service Data
                0x2C, 0xA2, 0x30        // Model ID
            };
            advData.addData(std::string((char*)fastPairData, sizeof(fastPairData)));
        } else {
            // GHOSTRF Beacon
            advData.setName("GHOSTRF_2.4G");
            const uint8_t ghData[] = { 0x07, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
            advData.addData(std::string((char*)ghData, sizeof(ghData)));
        }

        pAdv->setAdvertisementData(advData);
        pAdv->start();
        bleSpamCounter++;
    }
}

uint32_t wifi_tools_get_ble_spam_count() {
    return bleSpamCounter;
}

// =========================================================================
// 6. WI-FI BEACON SPAM (Multi-SSID Broadcast, 100% стандартный IEEE 802.11)
// =========================================================================

void wifi_tools_beacon_spam_start() {
    wifi_tools_stop_all();
    beaconSpamCounter = 0;
    beaconSpamActive = true;
    lastBeaconTime = millis();
    memset(beaconSeq, 0, sizeof(beaconSeq));

    // Для отправки raw 802.11 beacons через WIFI_IF_AP режим AP обязателен!
    WiFi.mode(WIFI_AP);
    delay(40);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
}

void wifi_tools_beacon_spam_stop() {
    beaconSpamActive = false;
    esp_wifi_set_promiscuous(false);
    WiFi.mode(WIFI_OFF);
}

void wifi_tools_beacon_spam_loop() {
    if (!beaconSpamActive) return;
    unsigned long now = millis();
    // Отправляем пакеты каждые 90-100 мс (стандартный beacon interval)
    if (now - lastBeaconTime >= 95) {
        lastBeaconTime = now;

        uint64_t ts = esp_timer_get_time(); // Метка времени в микросекундах

        // Передаем beacon для ВСЕХ виртуальных точек доступа пачкой
        for (int i = 0; i < NUM_BEACON_SSIDS; i++) {
            uint8_t packet[128];
            memset(packet, 0, sizeof(packet));

            // 1. Frame Control: Type=Management (0), Subtype=Beacon (8) -> 0x80
            packet[0] = 0x80;
            packet[1] = 0x00;

            // 2. Duration: 0x0000
            packet[2] = 0x00;
            packet[3] = 0x00;

            // 3. Destination: FF:FF:FF:FF:FF:FF (Broadcast)
            memset(&packet[4], 0xFF, 6);

            // 4. Source MAC & 5. BSSID: Уникальный, стабильный UNICAST MAC per SSID!
            // 0x02 в первом байте: Bit 0 = 0 (Unicast), Bit 1 = 1 (Locally Administered)
            // Это КРИТИЧЕСКИ важно: multicast BSSID отбрасываются всеми смартфонами!
            uint8_t mac[6] = { 0x02, 0x13, 0x37, 0xDE, 0xAD, (uint8_t)(i + 1) };
            memcpy(&packet[10], mac, 6); // Transmitter MAC
            memcpy(&packet[16], mac, 6); // BSSID

            // 6. Sequence Control (12-bit sequence number per BSSID)
            beaconSeq[i] = (beaconSeq[i] + 1) & 0x0FFF;
            packet[22] = (uint8_t)((beaconSeq[i] << 4) & 0xF0);
            packet[23] = (uint8_t)((beaconSeq[i] >> 4) & 0xFF);

            // 7. Timestamp (8 bytes)
            memcpy(&packet[24], &ts, 8);

            // 8. Beacon Interval: 100 TU (102.4 ms)
            packet[32] = 0x64;
            packet[33] = 0x00;

            // 9. Capability Info: ESS + Short Preamble
            packet[34] = 0x31;
            packet[35] = 0x04;

            // --- Tagged Parameters ---
            // Tag 0: SSID
            const char* ssid = BEACON_SSIDS[i];
            uint8_t ssidLen = strlen(ssid);
            packet[36] = 0x00;
            packet[37] = ssidLen;
            memcpy(&packet[38], ssid, ssidLen);

            int idx = 38 + ssidLen;

            // Tag 1: Supported Rates (1, 2, 5.5, 11, 6, 9, 12, 18 Mbps)
            packet[idx++] = 0x01;
            packet[idx++] = 0x08;
            packet[idx++] = 0x82; // 1 Mbps (B)
            packet[idx++] = 0x84; // 2 Mbps (B)
            packet[idx++] = 0x8b; // 5.5 Mbps (B)
            packet[idx++] = 0x96; // 11 Mbps (B)
            packet[idx++] = 0x0c; // 6 Mbps
            packet[idx++] = 0x12; // 9 Mbps
            packet[idx++] = 0x18; // 12 Mbps
            packet[idx++] = 0x24; // 18 Mbps

            // Tag 3: DS Parameter Set (Current Channel: Ch 1)
            packet[idx++] = 0x03;
            packet[idx++] = 0x01;
            packet[idx++] = 0x01; // Channel 1

            // Tag 5: TIM (Traffic Indication Map) — КРИТИЧЕСКИ нужен для Android 10+ и iOS
            packet[idx++] = 0x05;
            packet[idx++] = 0x04;
            packet[idx++] = 0x00; // DTIM Count
            packet[idx++] = 0x01; // DTIM Period
            packet[idx++] = 0x00; // Bitmap Control
            packet[idx++] = 0x00; // Partial Virtual Bitmap

            // Передача сырого пакета через интерфейс точки доступа WIFI_IF_AP
            esp_wifi_80211_tx(WIFI_IF_AP, packet, idx, false);
            beaconSpamCounter++;
        }
    }
}

uint32_t wifi_tools_get_beacon_spam_count() {
    return beaconSpamCounter;
}

// =========================================================================
// SHUTDOWN ALL 2.4 GHz RADIOS
// =========================================================================

void wifi_tools_stop_all() {
    if (snifferActive) {
        wifi_tools_sniffer_stop();
    }
    if (bleScanActive) {
        wifi_tools_ble_scan_stop();
    }
    if (bleSpamActive) {
        wifi_tools_ble_spam_stop();
    }
    if (beaconSpamActive) {
        wifi_tools_beacon_spam_stop();
    }
    scanRunning = false;
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    esp_wifi_set_promiscuous(false);
}
