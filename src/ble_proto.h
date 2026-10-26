#pragma once
#include <Arduino.h>
#include "types.h"

void ble_proto_init();
void ble_proto_poll(SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state);
bool ble_proto_is_connected();
bool ble_proto_is_enabled();
void ble_proto_set_enabled(bool enabled);
void ble_proto_send(const char* str);
void ble_proto_send_screen(const uint8_t* buf, int sz);
