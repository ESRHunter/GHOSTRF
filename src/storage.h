#pragma once
#include <Arduino.h>
#include "types.h"
#include "config.h"

void storage_init();
void storage_load_all(int &freqIdx, int &paIdx, int &squelchIdx, int &spectrumBandIdx, SavedSignal* sigList, int &sigCount);
void storage_save_settings(int freqIdx, int paIdx, int squelchIdx, int spectrumBandIdx);
bool storage_add_fixed(const char* name, uint32_t code, uint8_t bitLength, uint16_t pulseLength, uint8_t protocol, float freq, SavedSignal* sigList, int &sigCount);
bool storage_add_raw(const char* name, const RawPacket &rawPkt, float freq, SavedSignal* sigList, int &sigCount);
bool storage_add_virtual_keeloq(const char* name, uint32_t serial, uint64_t key, float freq, SavedSignal* sigList, int &sigCount);
bool storage_add_smart_remote(const char* name, uint32_t addr20, float freq, SavedSignal* sigList, int &sigCount);
bool storage_delete_signal(int index, SavedSignal* sigList, int &sigCount);
void storage_save_signals(const SavedSignal* sigList, int sigCount);
void storage_clear_signals(int &sigCount);

