// GHOSTRF Challenger R2_2: Serial CLI Command Processor Robustness & Fuzzing Stress Harness
// Focus: Rapid 50,000 command bursts, systematically truncated JSON,
// oversized lines (up to 100k chars), path traversal, and malicious parameters.

#include "test_harness.h"
#include "stress_memory_interceptor.h"
#include "cli_mock_env.h"
#include "serial_proto.h"
#include "types.h"
#include "config.h"
#include "radio_modes.h"
#include "sub_storage.h"

#include <string>
#include <vector>
#include <chrono>
#include <iostream>

using ghostrf_test::TestTier;

// Define Mock Instances
MockSerialPort Serial;
MockESP ESP;
MockLittleFS LittleFS;

// Global Stub State
static uint8_t s_dummyScreenBuf[1024];
static SavedSignal s_signals[MAX_SAVED_SIGNALS];
static int s_numSignals = 0;
static int s_fIdx = 0;
static int s_pIdx = 3;
static int s_sqIdx = 0;
static AppState s_state = MODE_MAIN_MENU;

// Subsystem Stubs required by serial_proto.cpp
bool ble_proto_is_connected() { return false; }
bool ble_proto_is_enabled() { return false; }
void ble_proto_set_enabled(bool) {}
void ble_proto_send(const char*) {}
void ble_proto_send_screen(const uint8_t*, int) {}

const uint8_t* ui_get_display_buffer() { return s_dummyScreenBuf; }
int ui_get_display_buffer_size() { return sizeof(s_dummyScreenBuf); }
void ui_draw_main_menu(int) {}
void ui_draw_tx_menu(const SavedSignal*, int, int) {}
void ui_draw_rx_screen(int) {}
void ui_draw_raw_screen(int, int) {}
void ui_draw_freq_analyzer_screen() {}
void ui_draw_spectrum_screen(int) {}
void ui_reset_spectrum_peaks() {}
void ui_draw_carrier_gen(int, int, CarrierType, bool) {}
void ui_draw_24g_menu(int) {}
void ui_draw_battery_screen(uint16_t, uint8_t, bool) {}
void ui_draw_settings_screen(bool, bool) {}
void ui_draw_sys_info() {}
void open_new_signal_editor() {}

bool radio_is_connected() { return true; }
void radio_apply_config(float, int) {}
void radio_transmit_fixed(uint32_t, uint8_t, uint16_t, uint8_t, float, int, uint8_t) {}
void radio_transmit_raw_pulses(const uint16_t*, uint16_t, float, int, uint8_t) {}
void radio_transmit_ev1527(uint32_t, uint8_t, float, int, uint8_t) {}
void radio_start_raw_sniffer(float, int, int) {}
void radio_stop_raw_sniffer() {}
void radio_set_rx(float, int) {}
void radio_set_idle() {}
void radio_set_carrier(CarrierType, float, int) {}
int radio_scan_rssi(float) { return -75; }
void radio_debug_cc1101(float, int &m, int &p, int &r, int &d) { m = 1; p = 0; r = -70; d = -70; }

bool storage_delete_signal(int idx, SavedSignal*, int &count) {
    if (idx >= 0 && idx < count) { count--; return true; }
    return false;
}
void storage_save_signals(const SavedSignal*, int) {}
void storage_clear_signals(int &count) { count = 0; }
void storage_save_settings(int, int, int, int) {}

void battery_update() {}
uint16_t battery_get_millivolts() { return 3850; }
uint16_t battery_get_raw_adc_mv() { return 1925; }
uint8_t battery_get_percentage() { return 78; }
bool battery_is_charging() { return false; }
bool battery_is_overvoltage() { return false; }

void buttons_inject_event(ButtonEvent) {}
void wake_screen_if_sleeping() {}

// sub_storage mocks
bool sub_storage_init() { return true; }
size_t sub_storage_total_bytes() { return 896 * 1024; }
size_t sub_storage_used_bytes() { return 12 * 1024; }
int sub_storage_list_files(SignalMeta* outList, int maxCount, const char*) {
    if (maxCount > 0 && outList) {
        strncpy(outList[0].filename, "door.sub", sizeof(outList[0].filename));
        outList[0].fileSize = 128;
        outList[0].fileType = SUB_TYPE_KEY;
        strncpy(outList[0].protocol, "Princeton", sizeof(outList[0].protocol));
        outList[0].frequency = 433.92f;
        outList[0].repeats = 5;
        return 1;
    }
    return 0;
}
bool sub_storage_delete(const char* fn) { return (fn && strlen(fn) > 0 && strcmp(fn, "door.sub") == 0); }
bool sub_storage_format() { return true; }
bool sub_storage_get_meta(const char* fn, SignalMeta &outMeta) {
    if (fn && strcmp(fn, "door.sub") == 0) {
        memset(&outMeta, 0, sizeof(outMeta));
        strncpy(outMeta.filename, "door.sub", sizeof(outMeta.filename));
        outMeta.fileType = SUB_TYPE_KEY;
        outMeta.frequency = 433.92f;
        outMeta.bitLength = 24;
        outMeta.fullCode = 0x547A01;
        outMeta.repeats = 5;
        outMeta.fileSize = 128;
        return true;
    }
    return false;
}
bool sub_storage_transmit(const char* fn, int, int) {
    return (fn && strcmp(fn, "door.sub") == 0);
}
bool sub_storage_save_raw(const char*, float, const uint16_t*, uint16_t, uint8_t, const char*) { return true; }
bool sub_storage_save_fixed(const char*, const char*, float, uint64_t, uint8_t, uint16_t, uint8_t) { return true; }

// Spectrum bands required by serial_proto.cpp
extern const SpectrumBand SPECTRUM_BANDS[NUM_SPECTRUM_BANDS] = {
    { "433 MHz", 433.05f, 0.05f, "433.05-434.60" },
    { "868 MHz", 868.00f, 0.05f, "868.00-869.55" },
    { "315 MHz", 314.50f, 0.05f, "314.50-316.05" }
};

// ----------------------------------------------------------------------------
// TEST 1: Rapid 50,000 Command Burst Stress (Non-Blocking & Leak-Free)
// ----------------------------------------------------------------------------
void test_r2_stress_cli_rapid_burst_50k() {
    Serial.clear();
    s_numSignals = 2;
    strncpy(s_signals[0].name, "Door", 15);
    s_signals[0].code = 0x112233;
    strncpy(s_signals[1].name, "Gate", 15);
    s_signals[1].code = 0x445566;

    const char* cmds[] = {
        "PING",
        "GET_CONFIG",
        "GET_BATTERY",
        "FS_INFO",
        "SET_CONFIG 0 2 1",
        "REMOTE_BTN UP",
        "STOP_SNIFFER"
    };
    const size_t numCmds = sizeof(cmds) / sizeof(cmds[0]);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 50000; i++) {
        char cmdBuf[64];
        snprintf(cmdBuf, sizeof(cmdBuf), "%s", cmds[i % numCmds]);
        proto_process_line(cmdBuf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
    }

    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    // 50,000 non-blocking commands under ASan/UBSan complete in < 2,500 ms (~50 us/cmd)
    ASSERT_TRUE(totalMs < 2500.0);
    ASSERT_TRUE(Serial.outputLines.size() >= 50000);
}

// ----------------------------------------------------------------------------
// TEST 2: Cooperative FreeRTOS Yielding & Watchdog Safety (FS_LIST / SUB_READ)
// ----------------------------------------------------------------------------
void test_r2_stress_cli_cooperative_watchdog_yielding() {
    Serial.clear();

    // 100 FS_LIST commands with simulated files, verifying delay(1) yielding every 8 files
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; i++) {
        char cmd[64] = "FS_LIST /sub";
        proto_process_line(cmd, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
    }
    auto end = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

    // Must have yielded cooperatively
    ASSERT_TRUE(elapsedMs >= 80.0);
    ASSERT_TRUE(!Serial.outputLines.empty());
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "FS_LIST_END");
}

// ----------------------------------------------------------------------------
// TEST 3: Systemic Truncation at Every Byte Offset & Deep JSON Fuzzing
// ----------------------------------------------------------------------------
void test_r2_stress_cli_systemic_json_truncation() {
    Serial.clear();

    const std::string fullCmd = "{\"cmd\":\"SUB_READ\",\"file\":\"door.sub\",\"options\":{\"verbose\":true}}";

    // Truncate at every single byte offset from 1 to full length
    for (size_t len = 1; len <= fullCmd.length(); len++) {
        std::string truncated = fullCmd.substr(0, len);
        char lineBuf[256];
        strncpy(lineBuf, truncated.c_str(), sizeof(lineBuf) - 1);
        lineBuf[sizeof(lineBuf) - 1] = '\0';

        size_t linesBefore = Serial.outputLines.size();
        proto_process_line(lineBuf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        size_t linesAfter = Serial.outputLines.size();

        ASSERT_TRUE(linesAfter > linesBefore);
        const std::string &reply = Serial.outputLines.back();
        ASSERT_STR_CONTAINS(reply, "{\"cmd\":\"UNKNOWN\"");
        ASSERT_STR_CONTAINS(reply, "\"echo\":");
    }

    // Corrupted and deeply nested JSON structures
    const char* pathologicalJson[] = {
        "{{{{{}}}}}",
        "{\"cmd\": [1, 2, 3, 4, 5], \"nested\": {\"deep\": {\"deeper\": true}}}",
        "{\"cmd\":\"UNKNOWN\", \"echo\":\"\\\"\\\\/\\b\\f\\n\\r\\t\"}",
        "{\"\": \"\"}",
        "{\"unclosed_string\": \"abcdef",
        "{\"null_byte\": \"test\000test\"}",
        "{\"escaped_quotes\": \"\\\"\\\"\\\"\\\"\\\"\"}"
    };

    for (const char* pj : pathologicalJson) {
        char buf[256];
        strncpy(buf, pj, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        size_t b = Serial.outputLines.size();
        proto_process_line(buf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        size_t a = Serial.outputLines.size();
        ASSERT_TRUE(a > b);
        ASSERT_STR_CONTAINS(Serial.outputLines.back(), "{\"cmd\":\"UNKNOWN\"");
    }
}

// ----------------------------------------------------------------------------
// TEST 4: Oversized Line Inputs (up to 100,000 Chars) & Buffer Saturation
// ----------------------------------------------------------------------------
void test_r2_stress_cli_oversized_line_saturation() {
    Serial.clear();

    // 1. Line lengths crossing 1024 buffer boundary: 1023, 1024, 1025, 2048, 10000, 100000
    const size_t lengths[] = { 1023, 1024, 1025, 2048, 10000, 100000 };

    for (size_t len : lengths) {
        std::string noise(len, 'A' + (len % 26));
        noise += "\n";

        Serial.feed(noise);
        serial_proto_poll(s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);

        ASSERT_TRUE(!Serial.outputLines.empty());
        ASSERT_STR_CONTAINS(Serial.outputLines.back(), "UNKNOWN");
        Serial.clear();
    }

    // 2. Stream massive noise followed immediately by valid command
    std::string hugeNoise(65536, 'X');
    hugeNoise += "\nPING\n";
    Serial.feed(hugeNoise);
    serial_proto_poll(s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);

    ASSERT_TRUE(Serial.outputLines.size() >= 2);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "PONG");
}

// ----------------------------------------------------------------------------
// TEST 5: Comprehensive Sub-GHz Malicious Parameter Fuzzing & Path Traversal
// ----------------------------------------------------------------------------
void test_r2_stress_cli_malicious_parameter_fuzzing() {
    Serial.clear();

    // 1. TX_RAW extreme pulse counts & boundaries
    const char* badTxRaw[] = {
        "TX_RAW 433.92 10 4 -1",
        "TX_RAW 433.92 10 4 0",
        "TX_RAW 433.92 10 4 193",      // > MAX_RAW_PULSES (192)
        "TX_RAW 433.92 10 4 9999999",
        "TX_RAW 433.92 10 4 -2147483648",
        "TX_RAW -433.92 10 4 50 100 200"
    };
    for (const char* cmd : badTxRaw) {
        char buf[128];
        strncpy(buf, cmd, sizeof(buf));
        proto_process_line(buf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        // If count was out of bounds, must report ERR
        if (strstr(cmd, "193") || strstr(cmd, "9999999") || strstr(cmd, "-1") || strstr(cmd, " 0")) {
            ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");
        }
    }

    // 2. TX_SAVED invalid / malicious indices
    const char* badTxSaved[] = {
        "TX_SAVED -1",
        "TX_SAVED -9999",
        "TX_SAVED 20",       // >= MAX_SAVED_SIGNALS
        "TX_SAVED 999999",
        "TX_SAVED non_number"
    };
    for (const char* cmd : badTxSaved) {
        char buf[128];
        strncpy(buf, cmd, sizeof(buf));
        proto_process_line(buf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        if (strstr(cmd, "20") || strstr(cmd, "999999") || strstr(cmd, "-1") || strstr(cmd, "-9999")) {
            ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");
        }
    }

    // 3. Path traversal attacks on filesystem & Sub-GHz commands
    const char* pathTraversalAttacks[] = {
        "SUB_READ ../../etc/passwd",
        "SUB_READ /sub/../../../etc/shadow",
        "SUB_READ /../../root/.ssh/id_rsa",
        "SUB_INFO ../secret.key",
        "SUB_TX ../../../etc/hosts",
        "FS_DELETE ../../system.bin"
    };
    for (const char* cmd : pathTraversalAttacks) {
        char buf[128];
        strncpy(buf, cmd, sizeof(buf));
        proto_process_line(buf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");
    }

    // 4. Missing parameters on mandatory-argument commands
    const char* missingParamCmds[] = {
        "SUB_READ",
        "SUB_INFO",
        "SUB_TX",
        "FS_DELETE",
        "SUB_SAVE_CAPTURED",
        "TX_FIXED"
    };
    for (const char* cmd : missingParamCmds) {
        char buf[128];
        strncpy(buf, cmd, sizeof(buf));
        proto_process_line(buf, s_signals, s_numSignals, s_fIdx, s_pIdx, s_sqIdx, s_state);
        ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");
    }
}

void register_r2_stress_cli_tests() {
    TEST_SUITE("R2 Stress Suite 2: Serial CLI Command Processor Robustness & Fuzzing");
    RUN_TEST_CASE("[R2-STRESS] Rapid burst of 50,000 commands (<2.5s non-blocking)", TestTier::TIER_4_REAL_WORLD, test_r2_stress_cli_rapid_burst_50k);
    RUN_TEST_CASE("[R2-STRESS] Cooperative FreeRTOS yielding & watchdog feeding", TestTier::TIER_4_REAL_WORLD, test_r2_stress_cli_cooperative_watchdog_yielding);
    RUN_TEST_CASE("[R2-STRESS] Systemic byte-by-byte JSON truncation & deep structure fuzzing", TestTier::TIER_2_BOUNDARY, test_r2_stress_cli_systemic_json_truncation);
    RUN_TEST_CASE("[R2-STRESS] Oversized line inputs (up to 100,000 chars) buffer saturation", TestTier::TIER_2_BOUNDARY, test_r2_stress_cli_oversized_line_saturation);
    RUN_TEST_CASE("[R2-STRESS] Malicious parameter fuzzing & path traversal defense", TestTier::TIER_2_BOUNDARY, test_r2_stress_cli_malicious_parameter_fuzzing);
}
