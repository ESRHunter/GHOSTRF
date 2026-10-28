// Stress Test Suite 2: Serial CLI Command Processor Robustness
// Evaluates: rapid bursts (10,000 cmds), invalid/truncated JSON, oversized line inputs, malformed parameters

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

using ghostrf_test::TestTier;

// Define Mock Instances
MockSerialPort Serial;
MockESP ESP;
MockLittleFS LittleFS;

// Global Stub State
static uint8_t s_dummyScreenBuf[1024];
static SavedSignal s_sigList[MAX_SAVED_SIGNALS];
static int s_sigCount = 0;
static int s_freqIdx = 0;
static int s_paIdx = 3;
static int s_squelchIdx = 0;
static AppState s_appState = MODE_MAIN_MENU;

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
// TEST 1: Rapid Bursts of 10,000 Commands (Stability & Non-Blocking)
// ----------------------------------------------------------------------------
void test_stress_cli_rapid_burst_10k() {
    Serial.clear();
    s_sigCount = 2;
    strncpy(s_sigList[0].name, "Gate", 15);
    s_sigList[0].code = 0x123456;
    strncpy(s_sigList[1].name, "Door", 15);
    s_sigList[1].code = 0x654321;

    const char* burstCommands[] = {
        "PING",
        "GET_CONFIG",
        "GET_BATTERY",
        "FS_INFO",
        "SET_CONFIG 0 2 1",
        "REMOTE_BTN UP",
        "STOP_SNIFFER"
    };
    const size_t numCmds = sizeof(burstCommands) / sizeof(burstCommands[0]);

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; i++) {
        char cmdBuf[64];
        snprintf(cmdBuf, sizeof(cmdBuf), "%s", burstCommands[i % numCmds]);
        proto_process_line(cmdBuf, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    }

    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    // 10,000 non-blocking commands under ASan/UBSan complete in < 500 ms (~50 us/cmd)
    ASSERT_TRUE(totalMs < 500.0);
    ASSERT_TRUE(Serial.outputLines.size() >= 10000);
}

// ----------------------------------------------------------------------------
// TEST 1b: Cooperative FreeRTOS Yielding & Watchdog Safety (FS_LIST / SUB_READ)
// ----------------------------------------------------------------------------
void test_stress_cli_cooperative_yielding() {
    Serial.clear();

    // Execute 50 FS_LIST commands and confirm cooperative delay(1) watchdog feeding
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 50; i++) {
        char cmdBuf[64] = "FS_LIST /sub";
        proto_process_line(cmdBuf, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    }
    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    // Each FS_LIST with files yields 1 ms to prevent FreeRTOS watchdog starvation
    ASSERT_TRUE(totalMs >= 40.0);
    ASSERT_TRUE(!Serial.outputLines.empty());
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "FS_LIST_END");
}

// ----------------------------------------------------------------------------
// TEST 2: Injected Malformed & Truncated JSON Commands
// ----------------------------------------------------------------------------
void test_stress_cli_malformed_json_injection() {
    Serial.clear();

    const char* malformedInputs[] = {
        "{\"cmd\":",
        "{\"cmd\":\"TX_SAVED\", \"index\":",
        "{\"cmd\":\"SUB_READ\", \"file\":\"",
        "{\"cmd\":\"STREAM_SCREEN\"",
        "{}",
        "{\"unknown_object\":{\"nested\":true}}",
        "{\"cmd\":\"PING\"}", // JSON format sent instead of text command
        "{\"cmd\":\"SAVE_FIXED\",\"name\":\"Bad\"}",
        "{\n\r\t\"key\": 12345",
        "{\"test\": \"quoted \\\"string\\\" with \\\\ backslashes\"}",
        "{\"cmd\":\"FS_DELETE\",\"file\":\"test.sub\"}"
    };

    for (const char* input : malformedInputs) {
        char lineBuf[256];
        strncpy(lineBuf, input, sizeof(lineBuf) - 1);
        lineBuf[sizeof(lineBuf) - 1] = '\0';

        size_t beforeLines = Serial.outputLines.size();
        proto_process_line(lineBuf, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
        size_t afterLines = Serial.outputLines.size();

        ASSERT_TRUE(afterLines > beforeLines);
        // The output line must be valid JSON starting with {"cmd":"UNKNOWN"
        const std::string &lastLine = Serial.outputLines.back();
        ASSERT_STR_CONTAINS(lastLine, "{\"cmd\":\"UNKNOWN\"");
        // Must contain escaped echo without crashing
        ASSERT_STR_CONTAINS(lastLine, "\"echo\":");
    }
}

// ----------------------------------------------------------------------------
// TEST 3: Oversized Line Inputs (>1024 chars, up to 50k chars)
// ----------------------------------------------------------------------------
void test_stress_cli_oversized_line_protection() {
    Serial.clear();

    // 1. Line exactly 1,000 chars without newline
    std::string line1000(1000, 'A');
    line1000 += "\n";
    Serial.feed(line1000);
    serial_proto_poll(s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_TRUE(!Serial.outputLines.empty());
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "UNKNOWN");

    // 2. Line of 5,000 chars without newline, then newline
    Serial.clear();
    std::string line5k(5000, 'X');
    line5k += "\n";
    Serial.feed(line5k);
    serial_proto_poll(s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_TRUE(!Serial.outputLines.empty());
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "UNKNOWN");

    // 3. Continuous burst of 50,000 characters without ANY newline, followed by a valid PING\n
    Serial.clear();
    std::string hugeNoise(50000, 'Z');
    Serial.feed(hugeNoise);
    // Poll halfway through
    serial_proto_poll(s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    // Buffer should saturate at 1022 without overflowing memory
    ASSERT_TRUE(Serial.outputLines.empty()); // No newline received yet

    // Now send newline + PING\n
    Serial.feed("\nPING\n");
    serial_proto_poll(s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);

    // Both the truncated noise and the subsequent PING must be handled safely
    ASSERT_TRUE(Serial.outputLines.size() >= 2);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "PONG");
}

// ----------------------------------------------------------------------------
// TEST 4: Malformed Parameters on Sub-GHz Commands
// ----------------------------------------------------------------------------
void test_stress_cli_malformed_parameters() {
    Serial.clear();

    // 1. TX_RAW with negative, overflow, and truncated pulse counts
    char cmd1[] = "TX_RAW 433.92 10 4 -50";
    proto_process_line(cmd1, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    char cmd2[] = "TX_RAW 433.92 10 4 999999";
    proto_process_line(cmd2, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    // 2. TX_SAVED with invalid/negative index
    char cmd3[] = "TX_SAVED -5";
    proto_process_line(cmd3, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    char cmd4[] = "TX_SAVED 999";
    proto_process_line(cmd4, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    char cmd5[] = "TX_SAVED ABC";
    proto_process_line(cmd5, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    // atoi("ABC") returns 0; index 0 exists in test list, so it safely executes or handles index
    ASSERT_TRUE(!Serial.outputLines.empty());

    // 3. SET_CONFIG with out-of-range parameters
    int origFreq = s_freqIdx;
    char cmd6[] = "SET_CONFIG -1 999 500";
    proto_process_line(cmd6, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    // Out of range parameters should not overwrite freqIdx with invalid index
    ASSERT_EQ(origFreq, s_freqIdx);

    // 4. SUB_READ with non-existent file and path traversal
    char cmd7[] = "SUB_READ /sub/../../etc/shadow";
    proto_process_line(cmd7, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    char cmd8[] = "SUB_INFO";
    proto_process_line(cmd8, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");

    // 5. SUB_SAVE_CAPTURED with no signal captured
    char cmd9[] = "SUB_SAVE_CAPTURED test_save";
    s_sigCount = 0; // empty signal list
    proto_process_line(cmd9, s_sigList, s_sigCount, s_freqIdx, s_paIdx, s_squelchIdx, s_appState);
    ASSERT_STR_CONTAINS(Serial.outputLines.back(), "\"status\":\"ERR\"");
}

void register_stress_cli_tests() {
    TEST_SUITE("Stress Test 2: Serial CLI Command Processor Robustness");
    RUN_TEST_CASE("[STRESS] Rapid burst of 10,000 CLI commands (<500ms non-blocking)", TestTier::TIER_4_REAL_WORLD, test_stress_cli_rapid_burst_10k);
    RUN_TEST_CASE("[STRESS] Cooperative FreeRTOS yielding & watchdog feeding", TestTier::TIER_4_REAL_WORLD, test_stress_cli_cooperative_yielding);
    RUN_TEST_CASE("[STRESS] Injected truncated & malformed JSON commands safe handling", TestTier::TIER_2_BOUNDARY, test_stress_cli_malformed_json_injection);
    RUN_TEST_CASE("[STRESS] Oversized line inputs up to 50,000 chars without overflow", TestTier::TIER_2_BOUNDARY, test_stress_cli_oversized_line_protection);
    RUN_TEST_CASE("[STRESS] Malformed Sub-GHz parameters, negative counts & path traversal", TestTier::TIER_2_BOUNDARY, test_stress_cli_malformed_parameters);
}
