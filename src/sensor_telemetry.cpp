#include "sensor_telemetry.h"

// =========================================================================
// CRC & CHECKSUM IMPLEMENTATIONS
// =========================================================================

uint8_t toyota_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x80;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }
    return crc;
}

uint8_t oregon_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }
    return crc;
}

uint8_t ford_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

uint8_t schrader_crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0xE0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) {
                crc = ((crc << 1) ^ 0x07) & 0xFF;
            } else {
                crc = (crc << 1) & 0xFF;
            }
        }
    }
    return crc;
}

// =========================================================================
// BITSTREAM DECODERS
// =========================================================================

bool decode_oregon_scientific(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out) {
    if (bitstream == nullptr || bitLen < 60) return false;

    size_t numNibbles = bitLen / 4;
    size_t startNibble = 0;

    // Check if the stream starts with sync nibble 0xA
    if (((bitstream[0] >> 4) & 0x0F) == 0x0A) {
        startNibble = 1;
    }

    uint8_t n[24];
    size_t count = 0;
    for (size_t i = startNibble; i < numNibbles && count < 24; i++) {
        if ((i % 2) == 0) {
            n[count++] = (bitstream[i / 2] >> 4) & 0x0F;
        } else {
            n[count++] = bitstream[i / 2] & 0x0F;
        }
    }

    if (count < 17) return false;

    // Model ID (Nibbles 0..3)
    uint32_t sensorId = ((uint32_t)n[0] << 12) | ((uint32_t)n[1] << 8) | ((uint32_t)n[2] << 4) | n[3];

    // Channel (Nibble 4): 1=Ch1, 2=Ch2, 4=Ch3
    uint8_t ch = 1;
    if (n[4] == 1) ch = 1;
    else if (n[4] == 2) ch = 2;
    else if (n[4] == 4) ch = 3;
    else ch = n[4] & 0x07;

    // Battery status (Nibble 7, bit 2: 1 = Low Battery)
    bool batteryLow = (n[7] & 0x04) != 0;

    // Temperature (Nibbles 8..10 BCD, Nibble 11 sign)
    int16_t tempTenths = n[8];
    int16_t tempUnits  = n[9];
    int16_t tempTens   = n[10];
    int16_t tempVal = (int16_t)(tempTens * 100 + tempUnits * 10 + tempTenths);
    if ((n[11] & 0x08) != 0) {
        tempVal = -tempVal;
    }

    // Humidity (Nibbles 12..13 BCD)
    uint8_t humUnits = n[12];
    uint8_t humTens  = n[13];
    uint8_t hum = humTens * 10 + humUnits;
    if (hum > 100) hum = 100;

    // Checksum validation: Sum of nibbles 0..14 with swapped nibbles (n[15] | (n[16]<<4))
    uint8_t sum = 0;
    for (int i = 0; i <= 14; i++) {
        sum += n[i];
    }
    uint8_t expectedChecksum = n[15] | (n[16] << 4);
    bool validChecksum = (sum == expectedChecksum);

    out.protocol = (count >= 19) ? SENSOR_PROTO_OREGON_V3 : SENSOR_PROTO_OREGON_V2;
    strncpy(out.sensorName, (out.protocol == SENSOR_PROTO_OREGON_V3) ? "Oregon-v3" : "Oregon-v2", sizeof(out.sensorName) - 1);
    out.sensorName[sizeof(out.sensorName) - 1] = '\0';
    out.sensorId = sensorId;
    out.temperatureC_x10 = tempVal;
    out.humidityPct = hum;
    out.pressureKpa_x10 = 0;
    out.batteryLow = batteryLow;
    out.channel = ch;
    out.validCrc = validChecksum;

    return validChecksum;
}

bool decode_tpms_toyota(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out) {
    if (bitstream == nullptr || bitLen < 72) return false;

    const uint8_t* b = bitstream;

    // Redundant inverted pressure check (b4/b5 vs b7)
    uint8_t rawPressure = ((b[4] & 0x7F) << 1) | (b[5] >> 7);
    uint8_t invPressure = b[7] ^ 0xFF;

    if (rawPressure != invPressure) {
        return false; // Edge Case 6: Reject immediately before CRC check
    }

    float psi = ((float)rawPressure * 0.25f) - 7.0f;
    if (psi < 0.0f) psi = 0.0f;
    float kpa = psi * 6.89476f;
    uint16_t pressureKpa_x10 = (uint16_t)roundf(kpa * 10.0f);

    uint8_t rawTemp = ((b[5] & 0x7F) << 1) | (b[6] >> 7);
    float tempC = (float)rawTemp - 40.0f;
    int16_t tempC_x10 = (int16_t)roundf(tempC * 10.0f);

    uint32_t sensorId = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];

    // CRC-8 validation: polynomial 0x07, init 0x80
    uint8_t calcCrc = toyota_crc8(b, 8);
    bool crcValid = (calcCrc == b[8]);

    out.protocol = SENSOR_PROTO_TPMS_TOYOTA;
    strncpy(out.sensorName, "Toyota PMV", sizeof(out.sensorName) - 1);
    out.sensorName[sizeof(out.sensorName) - 1] = '\0';
    out.sensorId = sensorId;
    out.temperatureC_x10 = tempC_x10;
    out.humidityPct = 0;
    out.pressureKpa_x10 = pressureKpa_x10;
    out.batteryLow = false;
    out.channel = 0;
    out.validCrc = crcValid;

    return true;
}

bool decode_tpms_ford(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out) {
    if (bitstream == nullptr || bitLen < 64) return false;

    const uint8_t* b = bitstream;

    uint8_t calcSum = ford_checksum(b, 7);
    bool validChecksum = (calcSum == b[7]);
    if (!validChecksum) {
        return false;
    }

    // Ford validation: Temperature MSB must be 0 (b[5] & 0x80 == 0)
    if ((b[5] & 0x80) != 0) {
        return false;
    }
    int16_t tempC = (int16_t)(b[5] & 0x7F) - 56;
    if (tempC < -50 || tempC > 125) {
        return false;
    }

    uint32_t sensorId = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
    if (sensorId == 0 || sensorId == 0xFFFFFFFF) {
        return false;
    }

    uint16_t rawPress = (uint16_t)b[4] | (((uint16_t)((b[6] >> 5) & 1)) << 8);
    float psi = (float)rawPress * 0.25f;
    float kpa = psi * 6.89476f;
    uint16_t pressureKpa_x10 = (uint16_t)roundf(kpa * 10.0f);

    int16_t tempC_x10 = tempC * 10;

    out.protocol = SENSOR_PROTO_TPMS_FORD;
    strncpy(out.sensorName, "Ford TPMS", sizeof(out.sensorName) - 1);
    out.sensorName[sizeof(out.sensorName) - 1] = '\0';
    out.sensorId = sensorId;
    out.temperatureC_x10 = tempC_x10;
    out.humidityPct = 0;
    out.pressureKpa_x10 = pressureKpa_x10;
    out.batteryLow = false;
    out.channel = 0;
    out.validCrc = true;

    return true;
}

bool decode_tpms_schrader(const uint8_t* bitstream, uint16_t bitLen, SensorTelemetryData &out) {
    if (bitstream == nullptr || bitLen < 56) return false;

    const uint8_t* b = bitstream;

    uint8_t calcCrc = schrader_crc8(b, 6);
    if (calcCrc != b[6]) {
        return false;
    }

    uint32_t sensorId = (((uint32_t)(b[0] & 0x03)) << 22) | (((uint32_t)b[1]) << 14) | (((uint32_t)b[2]) << 6) | (b[3] >> 2);
    uint16_t rawPress = (((uint16_t)(b[3] & 0x03)) << 8) | b[4];
    uint16_t pressureKpa_x10 = (uint16_t)(rawPress * 5); // raw * 0.5 * 10

    int16_t tempC = (int16_t)b[5] - 50;
    int16_t tempC_x10 = tempC * 10;

    out.protocol = SENSOR_PROTO_TPMS_SCHRADER;
    strncpy(out.sensorName, "Schrader TPMS", sizeof(out.sensorName) - 1);
    out.sensorName[sizeof(out.sensorName) - 1] = '\0';
    out.sensorId = sensorId;
    out.temperatureC_x10 = tempC_x10;
    out.humidityPct = 0;
    out.pressureKpa_x10 = pressureKpa_x10;
    out.batteryLow = false;
    out.channel = 0;
    out.validCrc = true;

    return true;
}

// =========================================================================
// UNIFIED PULSE INGESTION
// =========================================================================

bool telemetry_process_pulses(const uint16_t* pulses, uint16_t count, SensorTelemetryData &outData) {
    if (pulses == nullptr || count < 24) return false;

    const uint16_t candidates[3] = { 488, 52, 26 };

    for (int candIdx = 0; candIdx < 3; candIdx++) {
        uint16_t thalf = candidates[candIdx];

        for (int initLevel = 0; initLevel <= 1; initLevel++) {
            uint8_t halfBits[512];
            size_t halfCount = 0;
            uint8_t level = (uint8_t)initLevel;

            for (uint16_t i = 0; i < count && halfCount < 500; i++) {
                uint16_t p = pulses[i];
                int num = (int)((p + (thalf / 2)) / thalf);
                if (num < 1) continue;
                if (num > 6) num = 6;
                for (int k = 0; k < num && halfCount < 500; k++) {
                    halfBits[halfCount++] = level;
                }
                level = 1 - level;
            }

            if (halfCount < 48) continue;

            for (int phase = 0; phase <= 1; phase++) {
                for (int pol = 0; pol <= 1; pol++) {
                    int8_t bits[256];
                    size_t bitCount = 0;
                    size_t idx = phase;

                    while (idx + 1 < halfCount && bitCount < 256) {
                        uint8_t h0 = halfBits[idx];
                        uint8_t h1 = halfBits[idx + 1];
                        if (pol == 0) {
                            if (h0 == 1 && h1 == 0) bits[bitCount++] = 0;
                            else if (h0 == 0 && h1 == 1) bits[bitCount++] = 1;
                            else bits[bitCount++] = -1;
                        } else {
                            if (h0 == 0 && h1 == 1) bits[bitCount++] = 0;
                            else if (h0 == 1 && h1 == 0) bits[bitCount++] = 1;
                            else bits[bitCount++] = -1;
                        }
                        idx += 2;
                    }

                    if (bitCount < 48) continue;

                    // Oregon Scientific v2.1/v3.0 Search
                    if (thalf == 488) {
                        for (size_t s = 0; s + 68 <= bitCount; s++) {
                            // Check sync pattern [0, 1, 0, 1]
                            if (bits[s] == 0 && bits[s + 1] == 1 && bits[s + 2] == 0 && bits[s + 3] == 1) {
                                uint8_t reconstructed[16];
                                memset(reconstructed, 0, sizeof(reconstructed));
                                size_t pos = s + 4;
                                bool validNibbles = true;
                                size_t nibbleIdx = 0;

                                while (pos + 4 <= bitCount && nibbleIdx < 18) {
                                    uint8_t nib = 0;
                                    for (int b = 0; b < 4; b++) {
                                        if (bits[pos + b] < 0) {
                                            validNibbles = false;
                                            break;
                                        }
                                        nib |= ((uint8_t)bits[pos + b] << b);
                                    }
                                    if (!validNibbles) break;

                                    if ((nibbleIdx % 2) == 0) {
                                        reconstructed[nibbleIdx / 2] = (nib << 4);
                                    } else {
                                        reconstructed[nibbleIdx / 2] |= (nib & 0x0F);
                                    }
                                    nibbleIdx++;
                                    pos += 4;
                                }

                                if (validNibbles && nibbleIdx >= 17) {
                                    if (decode_oregon_scientific(reconstructed, (uint16_t)(nibbleIdx * 4), outData)) {
                                        if (outData.validCrc) return true;
                                    }
                                }
                            }
                        }
                    }

                    // TPMS Toyota / Ford / Schrader Search
                    if (thalf == 52 || thalf == 26) {
                        for (size_t s = 0; s + 72 <= bitCount; s++) {
                            uint8_t testBytes[12];
                            bool bytesValid = true;
                            size_t pos = s;

                            for (int byteIdx = 0; byteIdx < 9; byteIdx++) {
                                uint8_t val = 0;
                                for (int b = 0; b < 8; b++) {
                                    if (bits[pos + b] < 0) {
                                        bytesValid = false;
                                        break;
                                    }
                                    val = (val << 1) | (uint8_t)bits[pos + b];
                                }
                                if (!bytesValid) break;
                                testBytes[byteIdx] = val;
                                pos += 8;
                            }

                            if (bytesValid) {
                                if (decode_tpms_toyota(testBytes, 72, outData) && outData.validCrc) {
                                    return true;
                                }
                            }
                        }

                        // Ford Search (64 bits)
                        for (size_t s = 0; s + 64 <= bitCount; s++) {
                            uint8_t testBytes[8];
                            bool bytesValid = true;
                            size_t pos = s;

                            for (int byteIdx = 0; byteIdx < 8; byteIdx++) {
                                uint8_t val = 0;
                                for (int b = 0; b < 8; b++) {
                                    if (bits[pos + b] < 0) {
                                        bytesValid = false;
                                        break;
                                    }
                                    val = (val << 1) | (uint8_t)bits[pos + b];
                                }
                                if (!bytesValid) break;
                                testBytes[byteIdx] = val;
                                pos += 8;
                            }

                            if (bytesValid) {
                                if (decode_tpms_ford(testBytes, 64, outData) && outData.validCrc) {
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return false;
}
