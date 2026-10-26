#include "ble_proto.h"
#include "serial_proto.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;
static bool bleEnabled = true;

// Очередь входящих BLE команд (FIFO кольцевой буфер)
#define BLE_QUEUE_SIZE 16
#define BLE_LINE_BUF_SIZE 256
static char bleQueue[BLE_QUEUE_SIZE][BLE_LINE_BUF_SIZE];
static volatile int bleQueueHead = 0;
static volatile int bleQueueTail = 0;

static char bleIncomingBuf[BLE_LINE_BUF_SIZE];
static int bleIncomingLen = 0;

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
    }
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) override {
        deviceConnected = true;
        if (param) {
            pServer->updateConnParams(param->connect.remote_bda, 6, 12, 0, 400);
        }
    }
    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        if (bleEnabled) {
            BLEDevice::startAdvertising();
        }
    }
};

class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();
        for (size_t i = 0; i < rxValue.length(); i++) {
            char c = rxValue[i];
            if (c == '\r') continue;
            if (c == '\n') {
                if (bleIncomingLen > 0) {
                    bleIncomingBuf[bleIncomingLen] = '\0';
                    int nextHead = (bleQueueHead + 1) % BLE_QUEUE_SIZE;
                    if (nextHead != bleQueueTail) {
                        strncpy(bleQueue[bleQueueHead], bleIncomingBuf, BLE_LINE_BUF_SIZE - 1);
                        bleQueue[bleQueueHead][BLE_LINE_BUF_SIZE - 1] = '\0';
                        bleQueueHead = nextHead;
                    }
                    bleIncomingLen = 0;
                }
            } else {
                if (bleIncomingLen < BLE_LINE_BUF_SIZE - 2) {
                    bleIncomingBuf[bleIncomingLen++] = c;
                }
            }
        }
    }
};

void ble_proto_init() {
    BLEDevice::init("GHOSTRF");
    BLEDevice::setMTU(512);
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // 7.5ms
    pAdvertising->setMaxPreferred(0x12); // 22.5ms
    pAdvertising->setMinInterval(32);    // 20ms (32 * 0.625ms)
    pAdvertising->setMaxInterval(48);    // 30ms (48 * 0.625ms)
    BLEDevice::startAdvertising();
}

bool ble_proto_is_enabled() {
    return bleEnabled;
}

void ble_proto_set_enabled(bool enabled) {
    if (bleEnabled == enabled) return;
    bleEnabled = enabled;
    if (bleEnabled) {
        if (!pServer) {
            ble_proto_init();
        } else {
            BLEDevice::startAdvertising();
        }
    } else {
        if (pServer) {
            BLEDevice::stopAdvertising();
            deviceConnected = false;
        }
    }
}

bool ble_proto_is_connected() {
    return bleEnabled && deviceConnected;
}

void ble_proto_send(const char* str) {
    if (!bleEnabled || !deviceConnected || !pTxCharacteristic) return;
    int len = strlen(str);
    int offset = 0;
    while (offset < len) {
        int chunkSize = len - offset;
        if (chunkSize > 240) chunkSize = 240;
        pTxCharacteristic->setValue((uint8_t*)(str + offset), chunkSize);
        pTxCharacteristic->notify();
        offset += chunkSize;
        delay(2);
    }
}

void ble_proto_poll(SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state) {
    if (!bleEnabled) return;

    // Handle disconnect & restart advertising
    if (!deviceConnected && oldDeviceConnected) {
        BLEDevice::startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    // Обработка очереди входящих команд из BLE
    while (bleQueueTail != bleQueueHead) {
        char* line = bleQueue[bleQueueTail];
        bleQueueTail = (bleQueueTail + 1) % BLE_QUEUE_SIZE;
        proto_process_line(line, sigList, sigCount, freqIdx, paIdx, squelchIdx, state);
    }
}
