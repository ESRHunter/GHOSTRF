#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "sub_format.h"
#include "types.h"

#define MAX_SIGNALS_INDEX 64

// =========================================================================
// STORAGE LIFECYCLE & PARTITION MANAGEMENT
// =========================================================================

bool   sub_storage_init();
bool   sub_storage_format();
size_t sub_storage_total_bytes();
size_t sub_storage_used_bytes();
size_t sub_storage_free_bytes();
bool   sub_storage_is_mounted();

// =========================================================================
// DIRECTORY & METADATA
// =========================================================================

int  sub_storage_list_files(SignalMeta* outList, int maxCount, const char* dirPath = "/sub");
bool sub_storage_get_meta(const char* filePath, SignalMeta &outMeta);

// =========================================================================
// SAVE OPERATIONS
// =========================================================================

bool sub_storage_save_fixed(const char* filename, const char* proto, float freq, uint64_t code, uint8_t bitLen, uint16_t pulseLen, uint8_t repeats);
bool sub_storage_save_keeloq(const char* filename, float freq, uint64_t fullData64, const char* manufacture, uint8_t repeats);
bool sub_storage_save_raw(const char* filename, float freq, const uint16_t* pulses, uint16_t count, uint8_t repeats = 4, const char* presetStr = nullptr);

// =========================================================================
// DELETE & READ OPERATIONS
// =========================================================================

bool sub_storage_delete(const char* filename);
bool sub_storage_read_raw(const char* filename, uint16_t* outPulses, uint16_t maxCount, uint16_t &actualCount);
bool sub_storage_read_key(const char* filename, SubGhzHeader &hdr, SubGhzKeyData &key);
bool sub_storage_stream_raw(const char* filename, SubFormatParser::PulseCallback cb, void* ctx);

// =========================================================================
// DIRECT RADIO TRANSMISSION FROM LITTLEFS
// =========================================================================

bool sub_storage_transmit(const char* filename, int customRepeats = -1, int paLevel = 10);

// =========================================================================
// NVS SYNC & MIGRATION
// =========================================================================

void sub_storage_check_and_migrate_nvs();
