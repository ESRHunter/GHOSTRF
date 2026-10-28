#pragma once

#if defined(ARDUINO) || defined(ESP32)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#endif

// =========================================================================
// SENSOR TELEMETRY PROTOCOLS & DATA STRUCTURES
// =========================================================================

enum SensorProtocol {
    SENSOR_PROTO_UNKNOWN = 0,
    SENSOR_PROTO_OREGON_V2,
    SENSOR_PROTO_OREGON_V3,
    SENSOR_PROTO_FINE_OFFSET,
    SENSOR_PROTO_TPMS_TOYOTA,
    SENSOR_PROTO_TPMS_FORD,
    SENSOR_PROTO_TPMS_SCHRADER
};

struct SensorTelemetryData {
    SensorProtocol protocol;
    char           sensorName[20];
    uint32_t       sensorId;
    int16_t        temperatureC_x10; // e.g. 234 = +23.4 C, -52 = -5.2 C
    uint8_t        humidityPct;      // 0..100 %
    uint16_t       pressureKpa_x10;  // e.g. 2206 = 220.6 kPa (TPMS)
    bool           batteryLow;
    uint8_t        channel;
    bool           validCrc;
};

// =========================================================================
// CRC & CHECKSUM ALGORITHMS
// =========================================================================

// Toyota PMV-C210 CRC-8: Polynomial 0x07, Init 0x80, MSB-first
uint8_t toyota_crc8(const uint8_t* data, size_t len);

// Oregon Scientific v3.0 CRC-8: Polynomial 0x07, Init 0x00, MSB-first
uint8_t oregon_crc8(const uint8_t* data, size_t len);

// Ford / Continental TPMS Additive Checksum: sum(b[0..6]) & 0xFF
uint8_t ford_checksum(const uint8_t* data, size_t len);

// Schrader TPMS CRC-8: Polynomial 0x07, Init 0xE0
uint8_t schrader_crc8(const uint8_t* data, size_t len);

// =========================================================================
// HIGH-LEVEL PARSERS & MANCHESTER DEMODULATION
// =========================================================================

// Non-blocking bitstream decoders (feed raw demodulated bytes/bits)
bool decode_oregon_scientific(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out);
bool decode_tpms_toyota(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out);
bool decode_tpms_ford(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out);
bool decode_tpms_schrader(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out);

// Unified ingestion: Demodulates raw transition intervals (microseconds) from CC_GDO0
bool telemetry_process_pulses(const uint16_t* pulses, uint16_t count, SensorTelemetryData &outData);
