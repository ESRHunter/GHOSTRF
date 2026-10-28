// GHOSTRF E2E Test Suite - Environmental & Automotive Telemetry Decoders
// Authoritative sources: spec_miner/handoff.md & src/sensor_telemetry.h

#include "test_harness.h"
#include "sensor_telemetry.h"
#include <vector>
#include <cstring>
#include <cmath>

// Helper to synthesize Manchester pulses for Oregon Scientific packets
static std::vector<uint16_t> generate_oregon_manchester_pulses(const uint8_t* bytes, size_t numBytes) {
    std::vector<uint8_t> halfBits;
    // 24 half-bits alternating preamble
    for (int i = 0; i < 24; i++) {
        halfBits.push_back((uint8_t)(i % 2));
    }
    // Sync nibble 0xA (binary bits: 0, 1, 0, 1)
    int syncBits[4] = {0, 1, 0, 1};
    for (int b : syncBits) {
        if (b == 0) { halfBits.push_back(1); halfBits.push_back(0); }
        else        { halfBits.push_back(0); halfBits.push_back(1); }
    }
    // Data bytes (each nibble LSB-first)
    for (size_t i = 0; i < numBytes; i++) {
        uint8_t highNib = (bytes[i] >> 4) & 0x0F;
        uint8_t lowNib  = bytes[i] & 0x0F;
        uint8_t nibs[2] = {highNib, lowNib};
        for (uint8_t nib : nibs) {
            for (int b = 0; b < 4; b++) {
                int bit = (nib >> b) & 1;
                if (bit == 0) { halfBits.push_back(1); halfBits.push_back(0); }
                else          { halfBits.push_back(0); halfBits.push_back(1); }
            }
        }
    }

    // Convert halfBits (nominal 488 us per halfBit) to pulse durations
    std::vector<uint16_t> pulses;
    if (halfBits.empty()) return pulses;

    uint8_t curLevel = halfBits[0];
    uint16_t curDuration = 488;
    for (size_t i = 1; i < halfBits.size(); i++) {
        if (halfBits[i] == curLevel) {
            curDuration += 488;
        } else {
            pulses.push_back(curDuration);
            curLevel = halfBits[i];
            curDuration = 488;
        }
    }
    pulses.push_back(curDuration);
    return pulses;
}

// ============================================================================
// TIER 1: Feature Coverage (>=5 test cases per feature)
// ============================================================================

void test_telemetry_tier1_oregon_warm_day() {
    // Vector OS-1 from spec_miner/handoff.md Table 4:
    // Sensor ID: 0x1D20, Channel 1, Rolling Code 0x5A, Battery OK
    // Target Temp: +23.4 C, Target Humidity: 45%
    // Byte Stream: 1D 20 15 A0 43 20 54 02 30 (72 bits)
    const uint8_t packet[] = { 0x1D, 0x20, 0x15, 0xA0, 0x43, 0x20, 0x54, 0x02, 0x30 };

    SensorTelemetryData data = {};
    bool ok = decode_oregon_scientific(packet, 72, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ((int)SENSOR_PROTO_OREGON_V2, (int)data.protocol);
    ASSERT_HEX32_EQ(0x1D20, data.sensorId);
    ASSERT_EQ(1, data.channel);
    ASSERT_FALSE(data.batteryLow);
    ASSERT_EQ(234, data.temperatureC_x10); // +23.4 C
    ASSERT_EQ(45, data.humidityPct);      // 45%
}

void test_telemetry_tier1_oregon_freezing_day() {
    // Vector OS-2 from spec_miner/handoff.md Table 4:
    // Sensor ID: 0x1D20, Channel 2, Rolling Code 0x3F, Battery OK
    // Target Temp: -5.2 C (Negative temperature with sign bit 0x08 in nibble 11!), Target Humidity: 80%
    // Byte Stream: 1D 20 23 F0 25 08 08 0B 30 (72 bits)
    const uint8_t packet[] = { 0x1D, 0x20, 0x23, 0xF0, 0x25, 0x08, 0x08, 0x0B, 0x30 };

    SensorTelemetryData data = {};
    bool ok = decode_oregon_scientific(packet, 72, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ(2, data.channel);
    ASSERT_FALSE(data.batteryLow);
    ASSERT_EQ(-52, data.temperatureC_x10); // -5.2 C
    ASSERT_EQ(80, data.humidityPct);      // 80%
}

void test_telemetry_tier1_toyota_pmv_c210_standard() {
    // Vector TPMS-1 from spec_miner/handoff.md Table 5:
    // Sensor ID: 0x0C34B629, Target Pressure: 32.0 PSI (220.6 kPa), Target Temp: +22.0 C
    // 9-Byte Stream: 0C 34 B6 29 CE 1F 00 63 69 (72 bits)
    const uint8_t packet[] = { 0x0C, 0x34, 0xB6, 0x29, 0xCE, 0x1F, 0x00, 0x63, 0x69 };

    SensorTelemetryData data = {};
    bool ok = decode_tpms_toyota(packet, 72, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ((int)SENSOR_PROTO_TPMS_TOYOTA, (int)data.protocol);
    ASSERT_HEX32_EQ(0x0C34B629U, data.sensorId);
    ASSERT_EQ(2206, data.pressureKpa_x10); // 220.6 kPa (32.0 PSI)
    ASSERT_EQ(220, data.temperatureC_x10);  // +22.0 C
}

void test_telemetry_tier1_toyota_pmv_c210_low_pressure() {
    // Vector TPMS-2 from spec_miner/handoff.md Table 5:
    // Sensor ID: 0xA5B4C3D2, Target Pressure: 26.0 PSI (179.3 kPa), Target Temp: +18.0 C
    // 9-Byte Stream: A5 B4 C3 D2 C2 1D 00 7B 27 (72 bits)
    const uint8_t packet[] = { 0xA5, 0xB4, 0xC3, 0xD2, 0xC2, 0x1D, 0x00, 0x7B, 0x27 };

    SensorTelemetryData data = {};
    bool ok = decode_tpms_toyota(packet, 72, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_HEX32_EQ(0xA5B4C3D2U, data.sensorId);
    ASSERT_EQ(1793, data.pressureKpa_x10); // 179.3 kPa (26.0 PSI)
    ASSERT_EQ(180, data.temperatureC_x10);  // +18.0 C
}

void test_telemetry_tier1_ford_vdo() {
    // Vector Ford-1 from spec_miner/handoff.md Section 2.2:
    // Sensor ID: 0x12345678, Pressure 30.0 PSI (raw 120 = 0x78), Temp: +24.0 C (raw 80 = 0x50)
    // Flags: 0x44 (Moving), Checksum: (0x12+0x34+0x56+0x78+0x78+0x50+0x44) & 0xFF = 0x20
    const uint8_t packet[] = { 0x12, 0x34, 0x56, 0x78, 0x78, 0x50, 0x44, 0x20 };

    SensorTelemetryData data = {};
    bool ok = decode_tpms_ford(packet, 64, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ((int)SENSOR_PROTO_TPMS_FORD, (int)data.protocol);
    ASSERT_HEX32_EQ(0x12345678U, data.sensorId);
    ASSERT_EQ(2068, data.pressureKpa_x10); // 30.0 PSI * 6.89476 = 206.8 kPa
    ASSERT_EQ(240, data.temperatureC_x10);  // +24.0 C
}

void test_telemetry_tier1_schrader() {
    // Schrader TPMS 7-byte packet (56 bits):
    // ID = 0x001234, Pressure raw = 400 (200.0 kPa), Temp raw = 75 (+25.0 C)
    // CRC-8 poly 0x07 init 0xE0
    uint8_t packet[7] = { 0x00, 0x04, 0x8D, 0x01, 0x90, 0x4B, 0x00 };
    packet[6] = schrader_crc8(packet, 6);

    SensorTelemetryData data = {};
    bool ok = decode_tpms_schrader(packet, 56, data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ((int)SENSOR_PROTO_TPMS_SCHRADER, (int)data.protocol);
    ASSERT_EQ(2000, data.pressureKpa_x10); // 200.0 kPa
    ASSERT_EQ(250, data.temperatureC_x10);  // +25.0 C
}

// ============================================================================
// TIER 2: Boundary & Corner Cases
// ============================================================================

void test_telemetry_tier2_oregon_corrupted_checksum() {
    // Start with valid OS-1 packet and corrupt the checksum byte
    uint8_t packet[] = { 0x1D, 0x20, 0x15, 0xA0, 0x43, 0x20, 0x54, 0x02, 0x30 };
    packet[7] ^= 0x01; // Corrupt checksum byte

    SensorTelemetryData data = {};
    bool ok = decode_oregon_scientific(packet, 72, data);
    ASSERT_FALSE(ok);
}

void test_telemetry_tier2_toyota_pressure_mismatch() {
    // In Toyota TPMS, Byte 4/5 raw pressure must match Byte 7 inverted pressure
    uint8_t packet[] = { 0x0C, 0x34, 0xB6, 0x29, 0xCE, 0x1F, 0x00, 0x63, 0x69 };
    packet[7] = 0x50; // Corrupt inverted pressure to mismatch raw pressure

    SensorTelemetryData data = {};
    bool ok = decode_tpms_toyota(packet, 72, data);
    ASSERT_FALSE(ok);
}

void test_telemetry_tier2_toyota_corrupted_crc() {
    // Valid pressure matching, but corrupt CRC-8 byte
    uint8_t packet[] = { 0x0C, 0x34, 0xB6, 0x29, 0xCE, 0x1F, 0x00, 0x63, 0x69 };
    packet[8] ^= 0xFF; // Invert CRC

    SensorTelemetryData data = {};
    decode_tpms_toyota(packet, 72, data);
    ASSERT_FALSE(data.validCrc);
}

void test_telemetry_tier2_toyota_bypass_regression() {
    // Former hardcoded bypass packet from worker_m1:
    // Sensor ID: 0xA5B4C3D2, CRC byte = 0xD4 (real calculated CRC is 0x53)
    const uint8_t bypassPacket[] = { 0xA5, 0xB4, 0xC3, 0xD2, 0xC2, 0x0F, 0x00, 0x7B, 0xD4 };
    SensorTelemetryData data = {};
    bool res = decode_tpms_toyota(bypassPacket, 72, data);
    // Pressure inverted check passes, but CRC check MUST fail:
    ASSERT_TRUE(res);
    ASSERT_FALSE(data.validCrc);
}

void test_telemetry_tier2_ford_negative_temperature() {
    // Ford TPMS with sub-zero Celsius:
    // Temp: -10 C -> raw byte 5 = -10 + 56 = 46 (0x2E)
    // Checksum = (0x12+0x34+0x56+0x78+0x78+0x2E+0x44) & 0xFF = 0xFE
    const uint8_t packet[] = { 0x12, 0x34, 0x56, 0x78, 0x78, 0x2E, 0x44, 0xFE };

    SensorTelemetryData data = {};
    bool ok = decode_tpms_ford(packet, 64, data);

    ASSERT_TRUE(ok);
    ASSERT_EQ(-100, data.temperatureC_x10); // -10.0 C
}

void test_telemetry_tier2_truncated_packet() {
    const uint8_t shortPacket[] = { 0x1D, 0x20, 0x15 };
    SensorTelemetryData data = {};
    ASSERT_FALSE(decode_oregon_scientific(shortPacket, 24, data));
    ASSERT_FALSE(decode_tpms_toyota(shortPacket, 24, data));
    ASSERT_FALSE(decode_tpms_ford(shortPacket, 24, data));
    ASSERT_FALSE(decode_tpms_schrader(shortPacket, 24, data));
}

// ============================================================================
// TIER 3: Cross-Feature Interactions (Pulse-to-Telemetry Pipeline)
// ============================================================================

void test_telemetry_tier3_pulse_to_packet_pipeline() {
    // Synthesize Manchester pulse stream for Oregon Scientific OS-1 packet
    const uint8_t osPkt[] = { 0x1D, 0x20, 0x15, 0xA0, 0x43, 0x20, 0x54, 0x02, 0x30 };
    auto pulses = generate_oregon_manchester_pulses(osPkt, sizeof(osPkt));
    ASSERT_TRUE(pulses.size() >= 100);

    SensorTelemetryData data = {};
    bool ok = telemetry_process_pulses(pulses.data(), (uint16_t)pulses.size(), data);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(data.validCrc);
    ASSERT_EQ(234, data.temperatureC_x10); // +23.4 C
    ASSERT_EQ(45, data.humidityPct);      // 45%
    ASSERT_EQ(1, data.channel);           // Ch 1
    ASSERT_HEX32_EQ(0x1D20, data.sensorId);
}

// ============================================================================
// TIER 4: Real-World Scenarios
// ============================================================================

void test_telemetry_tier4_vehicle_4_wheel_tpms() {
    // Simulate real automotive scenario:
    // 4 wheels transmitting Pacific PMV-C210 packets with realistic pressure & temperature:
    // FL: 32.0 PSI, 22 C
    // FR: 32.5 PSI, 24 C
    // RL: 30.0 PSI, 21 C
    // RR: 29.5 PSI, 20 C
    struct Wheel {
        const char* pos;
        uint32_t id;
        double targetPsi;
        double targetC;
    } wheels[] = {
        {"FL", 0x11223344U, 32.0, 22.0},
        {"FR", 0x22334455U, 32.5, 24.0},
        {"RL", 0x33445566U, 30.0, 21.0},
        {"RR", 0x44556677U, 29.5, 20.0}
    };

    for (const auto& w : wheels) {
        uint8_t rawP = (uint8_t)std::round((w.targetPsi + 7.0) / 0.25);
        uint8_t rawT = (uint8_t)std::round(w.targetC + 40.0);
        uint8_t invP = (uint8_t)(rawP ^ 0xFF);

        uint8_t pkt[9];
        pkt[0] = (w.id >> 24) & 0xFF;
        pkt[1] = (w.id >> 16) & 0xFF;
        pkt[2] = (w.id >> 8) & 0xFF;
        pkt[3] = w.id & 0xFF;
        pkt[4] = 0x80 | ((rawP >> 1) & 0x7F);
        pkt[5] = ((rawP & 0x01) << 7) | ((rawT >> 1) & 0x7F);
        pkt[6] = (rawT & 0x01) << 7;
        pkt[7] = invP;
        pkt[8] = toyota_crc8(pkt, 8);

        SensorTelemetryData out = {};
        bool ok = decode_tpms_toyota(pkt, 72, out);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(out.validCrc);
        ASSERT_EQ(w.id, out.sensorId);
        ASSERT_NEAR((w.targetPsi * 6.89476 * 10.0), (double)out.pressureKpa_x10, 5.0);
        ASSERT_NEAR((w.targetC * 10.0), (double)out.temperatureC_x10, 5.0);
    }
}

void register_telemetry_tests() {
    TEST_SUITE("FSK / Environmental & Automotive Telemetry Verification");

    RUN_TEST_CASE("Oregon Scientific v2.1 warm day (+23.4 C, 45%) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_oregon_warm_day);
    RUN_TEST_CASE("Oregon Scientific v2.1 freezing day (-5.2 C sign bit) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_oregon_freezing_day);
    RUN_TEST_CASE("Toyota PMV-C210 TPMS standard pressure (32 PSI) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_toyota_pmv_c210_standard);
    RUN_TEST_CASE("Toyota PMV-C210 TPMS low pressure (26 PSI) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_toyota_pmv_c210_low_pressure);
    RUN_TEST_CASE("Ford Continental VDO TPMS standard (30 PSI, +24 C) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_ford_vdo);
    RUN_TEST_CASE("Schrader motorcycle TPMS (200 kPa, +25 C) decode", TestTier::TIER_1_FEATURE, test_telemetry_tier1_schrader);

    RUN_TEST_CASE("Oregon Scientific corrupted checksum rejection", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_oregon_corrupted_checksum);
    RUN_TEST_CASE("Toyota PMV-C210 dual inverted pressure mismatch rejection", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_toyota_pressure_mismatch);
    RUN_TEST_CASE("Toyota PMV-C210 corrupted CRC-8 poly 0x07 rejection", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_toyota_corrupted_crc);
    RUN_TEST_CASE("Toyota PMV-C210 bypass regression rejection (0xD4 / 0xA5B4C3D2)", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_toyota_bypass_regression);
    RUN_TEST_CASE("Ford VDO sub-zero temperature decoding (-10.0 C offset)", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_ford_negative_temperature);
    RUN_TEST_CASE("Truncated packet stream boundary safety (< min bytes)", TestTier::TIER_2_BOUNDARY, test_telemetry_tier2_truncated_packet);

    RUN_TEST_CASE("Manchester pulse-to-telemetry end-to-end demodulation", TestTier::TIER_3_CROSS_FEATURE, test_telemetry_tier3_pulse_to_packet_pipeline);
    RUN_TEST_CASE("Real-world vehicle 4-wheel TPMS monitoring bitstream", TestTier::TIER_4_REAL_WORLD, test_telemetry_tier4_vehicle_4_wheel_tpms);
}
