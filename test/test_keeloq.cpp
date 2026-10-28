// GHOSTRF E2E Test Suite - KeeLoq Cryptography, Key Derivation & State Machine
// Authoritative source: spec_miner/handoff.md (Microchip AN642 / AN645 / AN662 / HCS301)
// Direct Production Testing: Completely eliminated mock classes, testing src/keeloq_analyzer.cpp directly.

#include "test_harness.h"
#include "Arduino.h"
#include "keeloq.h"
#include "keeloq_analyzer.h"

// Helper to pack 32-bit plain data for testing cipher vectors
inline uint32_t test_keeloq_pack_plain(uint16_t counter, uint8_t button, uint32_t serial) {
    return ((uint32_t)counter & 0xFFFFU) |
           (((uint32_t)button & 0x0FU) << 16) |
           (((uint32_t)serial & 0x03FFU) << 20);
}

// ============================================================================
// TIER 1: Feature Coverage (>=5 test cases per feature)
// ============================================================================

void test_keeloq_tier1_cipher_vectors() {
    // 14 Standard Cryptographic Vectors from spec_miner/handoff.md Table 1
    struct Vector {
        const char* id;
        uint64_t key;
        uint32_t plain;
        uint32_t cipher;
    } vectors[] = {
        {"K1",  0x0000000000000000ULL, 0x00000000U, 0x00000000U},
        {"K2",  0x0000000000000000ULL, 0x12345678U, 0x4F84E711U},
        {"K3",  0x0000000000000000ULL, 0x33C20042U, 0x64BF5323U},
        {"K4",  0x0123456789ABCDEFULL, 0x00000000U, 0x0D326BF8U},
        {"K5",  0x0123456789ABCDEFULL, 0x12345678U, 0xB909A03BU},
        {"K6",  0x0123456789ABCDEFULL, 0x33C20042U, 0xAC6EDF6DU},
        {"K7",  0x0123456789ABCDEFULL, 0xAAAAAAAAU, 0x54E04EAEU},
        {"K8",  0x0123456789ABCDEFULL, 0x55555555U, 0x2E26E37CU},
        {"K9",  0x2749AC7815B284A1ULL, 0x00000000U, 0xF63F6406U}, // DoorHan Master Key
        {"K10", 0x2749AC7815B284A1ULL, 0x12345678U, 0x9107746DU},
        {"K11", 0x2749AC7815B284A1ULL, 0x33C20042U, 0xEC8AF5DBU},
        {"K12", 0xFEDCBA9876543210ULL, 0x00000000U, 0x0807DC04U}, // Inverted Generic Key
        {"K13", 0xFEDCBA9876543210ULL, 0x12345678U, 0x5C419D56U},
        {"K14", 0xFEDCBA9876543210ULL, 0x33C20042U, 0xF533BA92U}
    };

    for (const auto& v : vectors) {
        uint32_t enc = keeloq_encrypt(v.plain, v.key);
        ASSERT_HEX32_EQ(v.cipher, enc);

        uint32_t dec = keeloq_decrypt(enc, v.key);
        ASSERT_HEX32_EQ(v.plain, dec);
    }
}

void test_keeloq_tier1_normal_key_derivation() {
    uint64_t masterKey = 0x0123456789ABCDEFULL;

    // NK1: Serial 0x0123456 -> Device Key 0x9293784DA8273C27
    uint64_t dkey1 = keeloq_derive_key_normal(0x0123456, masterKey);
    ASSERT_HEX64_EQ(0x9293784DA8273C27ULL, dkey1);

    // NK2: Serial 0x01A2B3C -> Device Key 0xCF0239AA696C450E
    uint64_t dkey2 = keeloq_derive_key_normal(0x01A2B3C, masterKey);
    ASSERT_HEX64_EQ(0xCF0239AA696C450EULL, dkey2);

    // NK3: Serial 0x0987654 -> Device Key 0x0DFE6F8B73788710
    uint64_t dkey3 = keeloq_derive_key_normal(0x0987654, masterKey);
    ASSERT_HEX64_EQ(0x0DFE6F8B73788710ULL, dkey3);

    // Additional derivation vectors (boundary serial numbers)
    uint64_t dkeyZero = keeloq_derive_key_normal(0x0000000, masterKey);
    ASSERT_NE(0ULL, dkeyZero);

    uint64_t dkeyMax = keeloq_derive_key_normal(0x0FFFFFF, masterKey);
    ASSERT_NE(0ULL, dkeyMax);
}

void test_keeloq_tier1_packet_assembly_timing() {
    // Verified against Table 3 of spec_miner/handoff.md:
    // Serial: 0x01A2B3C, Button: 0x2, Counter: 0x0042, Key: 0x0123456789ABCDEF
    uint16_t pulses[200];
    uint16_t pulseCount = keeloq_build_packet(0x01A2B3C, 0x0042, 0x2, 0x0123456789ABCDEFULL, pulses);

    // Standard frame has exactly 157 pulses: 24 preamble + 1 gap + 132 PWM
    ASSERT_EQ(157, pulseCount);

    // Check preamble (12 pairs of 400 us HIGH / 400 us LOW)
    for (int i = 0; i < 24; i++) {
        ASSERT_EQ(400, pulses[i]);
    }

    // Check header gap (pulse 24 is 4000 us)
    ASSERT_EQ(4000, pulses[24]);

    // Check PWM data bit 0: Table 3 Encrypted Hop Code = 0xAC6EDF6D (LSB is 1)
    // Bit 0 = 1 -> 400 us HIGH, 800 us LOW
    ASSERT_EQ(400, pulses[25]);
    ASSERT_EQ(800, pulses[26]);
}

void test_keeloq_tier1_field_extraction() {
    uint32_t serial = 0x01A2B3C;
    uint8_t button = 0x2;
    uint16_t counter = 0x0042;
    uint64_t key = 0x0123456789ABCDEFULL;

    uint32_t plain = test_keeloq_pack_plain(counter, button, serial);
    uint32_t hop = keeloq_encrypt(plain, key);

    // Decrypt and inspect fields
    uint32_t decrypted = keeloq_decrypt(hop, key);
    uint16_t decCounter = (uint16_t)(decrypted & 0xFFFF);
    uint8_t decBtn = (uint8_t)((decrypted >> 16) & 0x0F);
    uint16_t decDisc = (uint16_t)((decrypted >> 20) & 0x03FF);

    ASSERT_EQ(counter, decCounter);
    ASSERT_EQ(button, decBtn);
    ASSERT_EQ((serial & 0x03FF), decDisc);
}

void test_keeloq_tier1_resync_state_machine() {
    // Production keeloq_analyzer testing:
    keeloq_analyzer_reset();
    uint32_t serial = 0x01A2B3C;
    uint64_t masterKey = 0x0123456789ABCDEFULL;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);
    keeloq_analyzer_set_last_counter(serial, 100);

    KeeloqAnalysisResult res;

    // Case 1: Sequential press (Counter 101, delta = 1) -> OK_SEQUENTIAL
    uint32_t plain101 = test_keeloq_pack_plain(101, 1, serial);
    uint32_t hop101 = keeloq_encrypt(plain101, devKey);
    bool ok101 = keeloq_analyze_packet(serial, 1, hop101, masterKey, res);
    ASSERT_TRUE(ok101);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
    ASSERT_EQ(1, res.counterDelta);
    ASSERT_EQ(101, res.decryptedCounter);

    // Case 2: Open window skipped press (Counter 105, delta = 4) -> OK_SEQUENTIAL
    uint32_t plain105 = test_keeloq_pack_plain(105, 1, serial);
    uint32_t hop105 = keeloq_encrypt(plain105, devKey);
    bool ok105 = keeloq_analyze_packet(serial, 1, hop105, masterKey, res);
    ASSERT_TRUE(ok105);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
    ASSERT_EQ(4, res.counterDelta);
    ASSERT_EQ(105, res.decryptedCounter);

    // Case 3: Replay attack (Counter 105 repeated, delta = 0) -> REPLAY_DETECTED
    bool okReplay = keeloq_analyze_packet(serial, 1, hop105, masterKey, res);
    ASSERT_TRUE(okReplay);
    ASSERT_EQ((int)KLQ_SYNC_ERR_REPLAY_DETECTED, (int)res.syncStatus);
    ASSERT_EQ(0, res.counterDelta);

    // Case 4: Resync window press 1 (Counter 200, delta = 95) -> PENDING (DESYNCHRONIZED)
    uint32_t plain200 = test_keeloq_pack_plain(200, 1, serial);
    uint32_t hop200 = keeloq_encrypt(plain200, devKey);
    bool ok200 = keeloq_analyze_packet(serial, 1, hop200, masterKey, res);
    ASSERT_TRUE(ok200);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);

    // Case 5: Resync window press 2 (Counter 201, delta = 1 relative to pending) -> OK_RESYNC_WINDOW
    uint32_t plain201 = test_keeloq_pack_plain(201, 1, serial);
    uint32_t hop201 = keeloq_encrypt(plain201, devKey);
    bool ok201 = keeloq_analyze_packet(serial, 1, hop201, masterKey, res);
    ASSERT_TRUE(ok201);
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res.syncStatus);
    ASSERT_EQ(1, res.counterDelta);
    ASSERT_EQ(201, res.decryptedCounter);
}

// ============================================================================
// TIER 2: Boundary & Corner Cases
// ============================================================================

void test_keeloq_tier2_counter_rollover() {
    keeloq_analyzer_reset();
    uint32_t serial = 0x055AA11;
    uint64_t masterKey = KLQ_KEY_DOORHAN; // DoorHan master key
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);
    keeloq_analyzer_set_last_counter(serial, 0xFFFF); // Counter at maximum 16-bit value

    KeeloqAnalysisResult res;

    // Counter rolls from 0xFFFF to 0x0000: (0 - 65535) & 0xFFFF = 1
    uint32_t plain0 = test_keeloq_pack_plain(0x0000, 1, serial);
    uint32_t hop0 = keeloq_encrypt(plain0, devKey);
    bool ok = keeloq_analyze_packet(serial, 1, hop0, masterKey, res);
    ASSERT_TRUE(ok);
    ASSERT_EQ(1, res.counterDelta);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
    ASSERT_EQ(0x0000, res.decryptedCounter);
}

void test_keeloq_tier2_discriminator_mismatch() {
    keeloq_analyzer_reset();
    uint32_t serial = 0x01A2B3C;
    uint64_t masterKey = 0x0123456789ABCDEFULL;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);
    keeloq_analyzer_set_last_counter(serial, 10);

    KeeloqAnalysisResult res;

    // Pack plain data with deliberately corrupted discriminator (0x123 instead of lower 10 bits 0x33C)
    uint32_t plainCorrupt = (11U & 0xFFFFU) | ((1U & 0x0FU) << 16) | ((0x123U & 0x03FFU) << 20);
    uint32_t hop = keeloq_encrypt(plainCorrupt, devKey);

    bool ok = keeloq_analyze_packet(serial, 1, hop, masterKey, res);
    ASSERT_FALSE(ok);
    ASSERT_FALSE(res.discriminatorValid);
    ASSERT_EQ((int)KLQ_SYNC_UNKNOWN, (int)res.syncStatus);
}

void test_keeloq_tier2_window_boundaries() {
    keeloq_analyzer_reset();
    uint32_t serial = 0x0112233;
    uint64_t masterKey = 0x0123456789ABCDEFULL;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);
    keeloq_analyzer_set_last_counter(serial, 100);

    KeeloqAnalysisResult res;

    // Boundary 1: Exactly delta = 16 (highest delta allowed in Zone 1: counter 116)
    uint32_t plain116 = test_keeloq_pack_plain(116, 1, serial);
    uint32_t hop116 = keeloq_encrypt(plain116, devKey);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, hop116, masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
    ASSERT_EQ(16, res.counterDelta);
    ASSERT_EQ(116, res.decryptedCounter);

    // Boundary 2: Delta = 17 (lowest delta entering Zone 2: 133 - 116 = 17)
    uint32_t plain133 = test_keeloq_pack_plain(133, 1, serial);
    uint32_t hop133 = keeloq_encrypt(plain133, devKey);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, hop133, masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);
    ASSERT_EQ(17, res.counterDelta);

    // Boundary 3: Delta >= 32768 (immediately rejected as desynchronized)
    uint32_t plainDesync = test_keeloq_pack_plain(116 + 32768, 1, serial);
    uint32_t hopDesync = keeloq_encrypt(plainDesync, devKey);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, hopDesync, masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);
    ASSERT_TRUE(res.counterDelta >= 32768);
}

void test_keeloq_tier2_resync_interrupted() {
    keeloq_analyzer_reset();
    uint32_t serial = 0x0334455;
    uint64_t masterKey = 0x0123456789ABCDEFULL;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);
    keeloq_analyzer_set_last_counter(serial, 100);

    KeeloqAnalysisResult res;

    // Press 1 in resync window (Counter 500) -> PENDING
    uint32_t plain500 = test_keeloq_pack_plain(500, 1, serial);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, keeloq_encrypt(plain500, devKey), masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);

    // Interrupted! Instead of 501, remote transmits 505 -> Resync fails and re-arms at 505
    uint32_t plain505 = test_keeloq_pack_plain(505, 1, serial);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, keeloq_encrypt(plain505, devKey), masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);

    // Now followed by 506 -> Consecutive press! Resync succeeds!
    uint32_t plain506 = test_keeloq_pack_plain(506, 1, serial);
    ASSERT_TRUE(keeloq_analyze_packet(serial, 1, keeloq_encrypt(plain506, devKey), masterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res.syncStatus);
    ASSERT_EQ(506, res.decryptedCounter);
}

// ============================================================================
// TIER 3: Cross-Feature Interactions
// ============================================================================

void test_keeloq_tier3_capture_decrypt_emulate_reverify() {
    // End-to-end workflow:
    // 1. Manufacturer generates remote using Normal Learning
    uint32_t serial = 0x01A2B3C;
    uint64_t masterKey = 0x0123456789ABCDEFULL;
    uint64_t deviceKey = keeloq_derive_key_normal(serial, masterKey);

    // 2. Receiver listens and syncs to initial remote transmission (Counter 50)
    keeloq_analyzer_reset();
    keeloq_analyzer_set_last_counter(serial, 50);

    // 3. User presses Button 1 (Counter 51)
    uint16_t currentCounter = 51;
    uint8_t buttonCode = 1;
    uint32_t plain = test_keeloq_pack_plain(currentCounter, buttonCode, serial);
    uint32_t hop = keeloq_encrypt(plain, deviceKey);

    // 4. Capture & inspect using production keeloq_analyze_packet
    KeeloqAnalysisResult analysis;
    bool rxOk = keeloq_analyze_packet(serial, buttonCode, hop, masterKey, analysis);
    ASSERT_TRUE(rxOk);
    ASSERT_EQ(currentCounter, analysis.decryptedCounter);
    ASSERT_TRUE(analysis.discriminatorValid);
    ASSERT_EQ(1, analysis.counterDelta);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)analysis.syncStatus);

    // 5. Educational safe single-step emulation: Increment counter to 52 via keeloq_emulate_next
    uint16_t pulses[200];
    uint16_t pulseCount = keeloq_emulate_next(serial, buttonCode, currentCounter, masterKey, pulses);
    ASSERT_EQ(157, pulseCount);

    // 6. Decode generated RF pulses back into 66-bit frame
    KeeloqFrame66 emuFrame;
    bool parsed = keeloq_pulses_to_frame(pulses, pulseCount, &emuFrame);
    ASSERT_TRUE(parsed);
    ASSERT_EQ(serial, emuFrame.getSerial());
    ASSERT_EQ(buttonCode, emuFrame.getButton());
    ASSERT_TRUE(emuFrame.getVlow());
    ASSERT_FALSE(emuFrame.getRepeat());

    // 7. Transmit emulated frame to receiver: Receiver must accept as valid sequential!
    KeeloqAnalysisResult emuAnalysis;
    bool emuOk = keeloq_analyze_packet(emuFrame.getSerial(), emuFrame.getButton(), emuFrame.getHop(), masterKey, emuAnalysis);
    ASSERT_TRUE(emuOk);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)emuAnalysis.syncStatus);
    ASSERT_EQ(currentCounter + 1, emuAnalysis.decryptedCounter);
    ASSERT_EQ(1, emuAnalysis.counterDelta);
}

// ============================================================================
// TIER 4: Real-World Application Scenarios
// ============================================================================

void test_keeloq_tier4_multi_remote_door_system() {
    // Simulate commercial garage door receiver managing 2 separate remotes:
    // Remote A (DoorHan remote) and Remote B (DoorHan remote)
    uint64_t doorhanMasterKey = KLQ_KEY_DOORHAN;

    uint32_t serialA = 0x0234567;
    uint64_t keyA = keeloq_derive_key_normal(serialA, doorhanMasterKey);

    uint32_t serialB = 0x0765432;
    uint64_t keyB = keeloq_derive_key_normal(serialB, doorhanMasterKey);

    keeloq_analyzer_reset();
    keeloq_analyzer_set_last_counter(serialA, 10);
    keeloq_analyzer_set_last_counter(serialB, 80);

    KeeloqAnalysisResult res;

    // Remote A sends sequential clicks (11, 12, 13)
    for (uint16_t c = 11; c <= 13; c++) {
        uint32_t hop = keeloq_encrypt(test_keeloq_pack_plain(c, 1, serialA), keyA);
        ASSERT_TRUE(keeloq_analyze_packet(serialA, 1, hop, doorhanMasterKey, res));
        ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
        ASSERT_EQ(c, res.decryptedCounter);
    }

    // Remote B is pressed in pocket 30 times (now at counter 111)
    // Press 1 (111) enters resync window
    uint32_t hopB1 = keeloq_encrypt(test_keeloq_pack_plain(111, 2, serialB), keyB);
    ASSERT_TRUE(keeloq_analyze_packet(serialB, 2, hopB1, doorhanMasterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);

    // Press 2 (112) resyncs Remote B
    uint32_t hopB2 = keeloq_encrypt(test_keeloq_pack_plain(112, 2, serialB), keyB);
    ASSERT_TRUE(keeloq_analyze_packet(serialB, 2, hopB2, doorhanMasterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res.syncStatus);
    ASSERT_EQ(112, res.decryptedCounter);

    // Remote A remains completely unaffected and operates at counter 14
    uint32_t hopA = keeloq_encrypt(test_keeloq_pack_plain(14, 1, serialA), keyA);
    ASSERT_TRUE(keeloq_analyze_packet(serialA, 1, hopA, doorhanMasterKey, res));
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
    ASSERT_EQ(14, res.decryptedCounter);
}

void register_keeloq_tests() {
    TEST_SUITE("KeeLoq Dynamic Code & Cipher Verification");

    RUN_TEST_CASE("KeeLoq 528-cycle NLFSR 14 standard test vectors (K1..K14)", TestTier::TIER_1_FEATURE, test_keeloq_tier1_cipher_vectors);
    RUN_TEST_CASE("Microchip Normal Learning Key Derivation (NK1..NK3 & boundaries)", TestTier::TIER_1_FEATURE, test_keeloq_tier1_normal_key_derivation);
    RUN_TEST_CASE("KeeLoq 66-bit frame pulse assembly & timing verification", TestTier::TIER_1_FEATURE, test_keeloq_tier1_packet_assembly_timing);
    RUN_TEST_CASE("KeeLoq decrypted plaintext field extraction & validation", TestTier::TIER_1_FEATURE, test_keeloq_tier1_field_extraction);
    RUN_TEST_CASE("KeeLoq 3-zone synchronization window state machine", TestTier::TIER_1_FEATURE, test_keeloq_tier1_resync_state_machine);

    RUN_TEST_CASE("KeeLoq counter rollover boundary (0xFFFF -> 0x0000 wrap)", TestTier::TIER_2_BOUNDARY, test_keeloq_tier2_counter_rollover);
    RUN_TEST_CASE("KeeLoq discriminator mismatch rejection", TestTier::TIER_2_BOUNDARY, test_keeloq_tier2_discriminator_mismatch);
    RUN_TEST_CASE("KeeLoq sync window threshold boundaries (delta=16, 17, 32768)", TestTier::TIER_2_BOUNDARY, test_keeloq_tier2_window_boundaries);
    RUN_TEST_CASE("KeeLoq interrupted / aborted resync double press sequence", TestTier::TIER_2_BOUNDARY, test_keeloq_tier2_resync_interrupted);

    RUN_TEST_CASE("KeeLoq full loop: derive -> capture -> inspect -> single-step emulate", TestTier::TIER_3_CROSS_FEATURE, test_keeloq_tier3_capture_decrypt_emulate_reverify);
    RUN_TEST_CASE("Multi-remote independent synchronization and pocket resync", TestTier::TIER_4_REAL_WORLD, test_keeloq_tier4_multi_remote_door_system);
}
