#pragma once
#include <Arduino.h>
#include "types.h"

void serial_proto_init();
void serial_proto_poll(SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state);
void proto_process_line(char* line, SavedSignal* sigList, int &sigCount, int &freqIdx, int &paIdx, int &squelchIdx, AppState &state);
void serial_proto_broadcast_fixed_rx(uint32_t code, uint8_t bitLen, uint16_t pulseLen, uint8_t proto, float freq, int rssi);
void serial_proto_broadcast_raw_rx(const RawPacket &pkt, float freq, int rssi);
bool serial_proto_is_streaming();
void serial_proto_send_screen_frame(bool force = false);
void wake_screen_if_sleeping();
