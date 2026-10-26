#pragma once
#include <Arduino.h>
#include "types.h"

void radio_init();
bool radio_is_connected();
void radio_apply_config(float freq, int paLevel);
void radio_set_rx(float freq, int paLevel);
void radio_set_tx(float freq, int paLevel);
void radio_set_idle();
void radio_transmit_fixed(uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol, float freq, int paLevel, uint8_t repeats = 7);
void radio_set_carrier(CarrierType mode, float freq, int paLevel);
void radio_update_carrier_loop(CarrierType mode);
bool radio_check_rx(uint32_t &code, uint8_t &bitLen, uint16_t &pulseLen, uint8_t &proto);
int  radio_scan_rssi(float freq);
int  radio_get_rssi();

// Автодетектор / Частотомер
float radio_scan_fast_peak(int &outRssi);

// RAW & KeeLoq / Protocol Sniffer
void radio_start_raw_sniffer(float freq, int paLevel, int squelchDbm);
void radio_stop_raw_sniffer();
bool radio_check_raw(RawPacket &outPacket, int squelchDbm);
void radio_transmit_raw_pulses(const uint16_t* pulses, uint16_t count, float freq, int paLevel, uint8_t repeats = 4);

// Smart Home & Open Protocols (EV1527, PT2262, Came, Nice)
bool radio_decode_pt2262_tristate(uint32_t code, uint8_t bitLen, char* outTriStr);
void radio_transmit_ev1527(uint32_t address20, uint8_t buttonIdx, float freq, int paLevel, uint8_t repeats = 5);
void radio_transmit_came12(uint16_t code12, float freq, int paLevel, uint8_t repeats = 6);
void radio_transmit_came24(uint32_t code24, float freq, int paLevel, uint8_t repeats = 6);
void radio_transmit_nice12(uint16_t code12, float freq, int paLevel, uint8_t repeats = 6);
void radio_transmit_linear10(uint16_t code10, float freq, int paLevel, uint8_t repeats = 6);
void radio_transmit_somfy_rts(uint32_t address24, uint16_t rollingCounter, uint8_t buttonCode, float freq, int paLevel, uint8_t repeats = 4);
void radio_transmit_pt2240(uint32_t code24, float freq, int paLevel, uint8_t repeats = 6);
void radio_transmit_doorhan(uint32_t serial, uint16_t counter, uint8_t button, float freq, int paLevel, uint8_t repeats = 4);
