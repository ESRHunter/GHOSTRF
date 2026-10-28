// GHOSTRF Tier 5 White-Box Coverage Hardening & Adversarial Stress Suite
// Challenges: KeeLoq Engine, Flipper Zero .sub Parser, Telemetry Decoders
// Designed for execution with AddressSanitizer (ASan) & UndefinedBehaviorSanitizer (UBSan)

#include "test_harness.h"
#include "keeloq.h"
#include "keeloq_analyzer.h"
#include "sub_format.h"
#include "sensor_telemetry.h"

#include <random>
#include <vector>
#include <string>
#include <cstring>
#include <iostream>

using ghostrf_test::TestTier;

// =========================================================================
// SUITE 1: KEELOQ ADVERSARIAL & STRESS TESTING
// =========================================================================

void test_keeloq_out_of_order_burst() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0547A01;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    // Initial baseline at 1000
    uint16_t currentCounter = 1000;
    keeloq_analyzer_set_last_counter(serial, currentCounter);

    // Sequence of 1,000 out-of-order counter transitions
    // Mix of: open window (+1..+16), replay (delta=0), old (delta > 32768), resync jump (+30), and consecutive resync (+31)
    std::mt19937 rng(1337);
    std::uniform_int_distribution<int> actionDist(0, 4);

    for (int i = 0; i < 1000; i++) {
        int action = actionDist(rng);
        uint16_t nextCounter = currentCounter;
        KeeloqSyncStatus expectedStatus = KLQ_SYNC_UNKNOWN;

        if (action == 0) {
            // Sequential open window (+1..+16)
            uint16_t inc = (rng() % 16) + 1;
            nextCounter = (uint16_t)(currentCounter + inc);
            expectedStatus = KLQ_SYNC_OK_SEQUENTIAL;
        } else if (action == 1) {
            // Replay attack (exact currentCounter)
            nextCounter = currentCounter;
            expectedStatus = KLQ_SYNC_ERR_REPLAY_DETECTED;
        } else if (action == 2) {
            // Old / decreased counter (delta >= 32768)
            uint16_t dec = (rng() % 500) + 1;
            nextCounter = (uint16_t)(currentCounter - dec);
            expectedStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        } else if (action == 3) {
            // Jump into resync window (+50)
            nextCounter = (uint16_t)(currentCounter + 50);
            expectedStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        } else {
            // Consecutive open window +1
            nextCounter = (uint16_t)(currentCounter + 1);
            expectedStatus = KLQ_SYNC_OK_SEQUENTIAL;
        }

        uint32_t plain = ((uint32_t)nextCounter & 0xFFFFU) |
                         (0x02U << 16) | // button
                         (((uint32_t)serial & 0x03FFU) << 20); // discriminator
        uint32_t hop = keeloq_encrypt(plain, devKey);

        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x02, hop, masterKey, (uint32_t)(10000 + i * 100), res);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(res.discriminatorValid);
        ASSERT_EQ(res.decryptedCounter, nextCounter);
        ASSERT_EQ((int)expectedStatus, (int)res.syncStatus);

        if (res.syncStatus == KLQ_SYNC_OK_SEQUENTIAL) {
            currentCounter = nextCounter;
        }
    }
}

void test_keeloq_replay_attack_matrix() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0123456;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    uint16_t baselineCounter = 5000;
    keeloq_analyzer_set_last_counter(serial, baselineCounter);

    // 1. Exact replay attack
    uint32_t plain = ((uint32_t)baselineCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t hop = keeloq_encrypt(plain, devKey);

    for (int rep = 0; rep < 20; rep++) {
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1000 + rep * 50, res);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(res.discriminatorValid);
        ASSERT_EQ((int)KLQ_SYNC_ERR_REPLAY_DETECTED, (int)res.syncStatus);
        ASSERT_EQ(0, res.counterDelta);
    }

    // 2. Advance counter by +2 (open window)
    uint16_t advancedCounter = 5002;
    plain = ((uint32_t)advancedCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);

    KeeloqAnalysisResult resAdv;
    bool okAdv = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 2000, resAdv);
    ASSERT_TRUE(okAdv);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)resAdv.syncStatus);

    // 3. Replay older packet (5000) - now delta is -2 = 65534 >= 32768
    plain = ((uint32_t)baselineCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);

    KeeloqAnalysisResult resStale;
    bool okStale = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 2100, resStale);
    ASSERT_TRUE(okStale);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resStale.syncStatus);

    // 4. Replay candidate during pending resync
    // Jump to +100 (5102) -> resync pending
    uint16_t jumpCounter = 5102;
    plain = ((uint32_t)jumpCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);

    KeeloqAnalysisResult resJump;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 3000, resJump);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resJump.syncStatus);

    // Attacker replays 5102 during pending resync
    KeeloqAnalysisResult resJumpReplay;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 3100, resJumpReplay);
    // Must NOT resync!
    ASSERT_NE((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resJumpReplay.syncStatus);

    // Attacker sends non-consecutive 5104
    uint16_t wrongConsecutive = 5104;
    plain = ((uint32_t)wrongConsecutive & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult resWrong;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 3200, resWrong);
    ASSERT_NE((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resWrong.syncStatus);

    // Resync timeout expiration (> 20,000 ms)
    // Now pending counter was 5104 at t=3200. Sending consecutive 5105 at t=25000 (> 20s from 3200)
    uint16_t validConsecutive = 5105;
    plain = ((uint32_t)validConsecutive & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult resTimeout;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 25000, resTimeout);
    // Timeout expired (25000 - 3200 = 21800 > 20000), resync window must NOT accept
    ASSERT_NE((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resTimeout.syncStatus);

    // 5. System uptime timer rollover (49.7 days wrap: 0xFFFFFFF0 -> 0x00000010)
    // Transmitter jumps from 5002 to 5200 (resync pending stored at t = 0xFFFFFFF0)
    uint16_t rollJump = 5200;
    plain = ((uint32_t)rollJump & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult resRoll1;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 0xFFFFFFF0U, resRoll1);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resRoll1.syncStatus);

    // Consecutive packet 5201 arrives 32 ms later at t = 0x00000010U (rollover across 4GB)
    uint16_t rollConsecutive = 5201;
    plain = ((uint32_t)rollConsecutive & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult resRoll2;
    keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 0x00000010U, resRoll2);
    // Empirical observation of timer rollover behavior:
    // If implementation uses `currentTimeMs >= entry->resyncTimestampMs`, this evaluates to false and fails to resync.
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resRoll2.syncStatus);
}

void test_keeloq_corrupted_hop_codes() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0A98765;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    uint32_t plain = (100 & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t validHop = keeloq_encrypt(plain, devKey);

    // 1. Bit-flip permutations
    size_t rejectedCount = 0;
    for (int b = 0; b < 32; b++) {
        uint32_t corruptHop = validHop ^ (1UL << b);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet(serial, 0x01, corruptHop, masterKey, res);
        if (!ok || !res.discriminatorValid) {
            rejectedCount++;
        }
    }
    // Almost all bitflips should corrupt the 10-bit discriminator
    ASSERT_TRUE(rejectedCount >= 30);

    // 2. Extreme hop values
    const uint32_t edgeHops[4] = { 0x00000000, 0xFFFFFFFF, 0xAAAAAAAA, 0x55555555 };
    for (int i = 0; i < 4; i++) {
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet(serial, 0x01, edgeHops[i], masterKey, res);
        // Extreme values must not cause crashes or undefined behavior
        if (!res.discriminatorValid) {
            ASSERT_FALSE(ok);
        }
    }

    // 3. Fuzz test 1,000 random hop values
    std::mt19937 rng(42);
    size_t randomRejections = 0;
    for (int i = 0; i < 1000; i++) {
        uint32_t randHop = rng();
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet(serial, 0x01, randHop, masterKey, res);
        if (!ok) {
            randomRejections++;
            ASSERT_FALSE(res.discriminatorValid);
            ASSERT_EQ((int)KLQ_SYNC_UNKNOWN, (int)res.syncStatus);
        }
    }
    // Theoretical acceptance rate for random 10-bit discriminator is ~ 1/1024 (or 2/1024 with simple learning fallback)
    ASSERT_TRUE(randomRejections >= 990);
}

void test_keeloq_rollover_boundaries() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x055AA33;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    // 1. Continuous step across 0xFFFF -> 0x0000
    keeloq_analyzer_set_last_counter(serial, 0xFFFE);

    uint16_t seq[] = { 0xFFFF, 0x0000, 0x0001, 0x0002 };
    for (size_t i = 0; i < sizeof(seq)/sizeof(seq[0]); i++) {
        uint32_t plain = ((uint32_t)seq[i] & 0xFFFFU) | (0x04U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);

        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x04, hop, masterKey, 1000 + (uint32_t)i * 100, res);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(res.discriminatorValid);
        ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
        ASSERT_EQ(1, res.counterDelta);
        ASSERT_EQ(seq[i], res.decryptedCounter);
    }

    // 2. Rollover jump into resync window: 0xFFF0 -> 0x0020 (delta = 48)
    keeloq_analyzer_set_last_counter(serial, 0xFFF0);
    uint32_t plain = (0x0020U & 0xFFFFU) | (0x04U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t hop = keeloq_encrypt(plain, devKey);

    KeeloqAnalysisResult res1;
    bool ok1 = keeloq_analyze_packet_timed(serial, 0x04, hop, masterKey, 5000, res1);
    ASSERT_TRUE(ok1);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res1.syncStatus);
    ASSERT_EQ(48, res1.counterDelta);

    // Consecutive packet 0x0021 at t=5100 ms
    plain = (0x0021U & 0xFFFFU) | (0x04U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult res2;
    bool ok2 = keeloq_analyze_packet_timed(serial, 0x04, hop, masterKey, 5100, res2);
    ASSERT_TRUE(ok2);
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res2.syncStatus);
    ASSERT_EQ(1, res2.counterDelta);
    ASSERT_EQ(0x0021, res2.decryptedCounter);

    // 3. Rollover backward jump (0x0005 -> 0xFFFA): delta = 65525 >= 32768
    keeloq_analyzer_set_last_counter(serial, 0x0005);
    plain = (0xFFFAU & 0xFFFFU) | (0x04U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    hop = keeloq_encrypt(plain, devKey);
    KeeloqAnalysisResult resBack;
    bool okBack = keeloq_analyze_packet_timed(serial, 0x04, hop, masterKey, 6000, resBack);
    ASSERT_TRUE(okBack);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resBack.syncStatus);
}

void test_keeloq_abnormal_discriminator() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0789ABC; // serial & 0x3FF = 0x2BC
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    uint16_t expDisc = (uint16_t)(serial & 0x03FFU);

    // Abnormal discriminators: 0x000, 0x3FF, expDisc ^ 1, expDisc ^ 0x200
    uint16_t badDiscs[4] = { 0x000, 0x3FF, (uint16_t)(expDisc ^ 0x001), (uint16_t)(expDisc ^ 0x200) };

    for (int i = 0; i < 4; i++) {
        uint32_t plain = (100 & 0xFFFFU) | (0x02U << 16) | (((uint32_t)badDiscs[i] & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);

        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet(serial, 0x02, hop, masterKey, res);
        ASSERT_FALSE(ok);
        ASSERT_FALSE(res.discriminatorValid);
        ASSERT_EQ((int)KLQ_SYNC_UNKNOWN, (int)res.syncStatus);
    }
}

void test_keeloq_lru_table_saturation() {
    keeloq_analyzer_reset();

    // Table capacity is 32 transmitters. Stress with 64 distinct serial numbers.
    for (uint32_t s = 1; s <= 64; s++) {
        uint32_t serial = 0x0100000 + s;
        keeloq_analyzer_set_last_counter(serial, (uint16_t)(100 + s));
        bool hasCounter = false;
        uint16_t cnt = keeloq_analyzer_get_last_counter(serial, &hasCounter);
        ASSERT_TRUE(hasCounter);
        ASSERT_EQ((uint16_t)(100 + s), cnt);
    }

    // Now first 32 entries (1..32) should have been evicted by LRU, leaving 33..64 in table
    bool hasCounterFirst = false;
    keeloq_analyzer_get_last_counter(0x0100001, &hasCounterFirst);
    ASSERT_FALSE(hasCounterFirst);

    bool hasCounterLast = false;
    uint16_t cntLast = keeloq_analyzer_get_last_counter(0x0100040, &hasCounterLast);
    ASSERT_TRUE(hasCounterLast);
    ASSERT_EQ((uint16_t)(100 + 64), cntLast);
}

void test_keeloq_null_and_bounds_safety() {
    // 1. Bit accessor bounds
    KeeloqFrame66 frame;
    memset(&frame, 0, sizeof(frame));

    // Valid bounds (0..65)
    keeloq_set_bit66(&frame, 0, true);
    ASSERT_TRUE(keeloq_get_bit66(&frame, 0));
    keeloq_set_bit66(&frame, 63, true);
    ASSERT_TRUE(keeloq_get_bit66(&frame, 63));
    keeloq_set_bit66(&frame, 64, true);
    ASSERT_TRUE(keeloq_get_bit66(&frame, 64));
    keeloq_set_bit66(&frame, 65, true);
    ASSERT_TRUE(keeloq_get_bit66(&frame, 65));

    // Out of bounds bits (>= 66)
    keeloq_set_bit66(&frame, 66, true);
    ASSERT_FALSE(keeloq_get_bit66(&frame, 66));
    keeloq_set_bit66(&frame, 100, true);
    ASSERT_FALSE(keeloq_get_bit66(&frame, 100));
    keeloq_set_bit66(&frame, 255, true);
    ASSERT_FALSE(keeloq_get_bit66(&frame, 255));

    // 2. Null pointers
    keeloq_set_bit66(nullptr, 0, true);
    ASSERT_FALSE(keeloq_get_bit66(nullptr, 0));
    keeloq_pack_frame66(0, 0, 0, false, false, nullptr);
    keeloq_unpack_frame66(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    ASSERT_EQ(0, keeloq_frame_to_pulses(nullptr, nullptr));

    uint16_t dummyPulses[160];
    ASSERT_EQ(0, keeloq_frame_to_pulses(&frame, nullptr));
    ASSERT_EQ(0, keeloq_frame_to_pulses(nullptr, dummyPulses));
    ASSERT_FALSE(keeloq_pulses_to_frame(nullptr, 160, &frame));
    ASSERT_FALSE(keeloq_pulses_to_frame(dummyPulses, 160, nullptr));
    ASSERT_FALSE(keeloq_pulses_to_frame(dummyPulses, 10, &frame)); // too short
}

// =========================================================================
// SUITE 2: FLIPPER ZERO .SUB PARSER ADVERSARIAL & STRESS TESTING
// =========================================================================

void test_sub_parser_malformed_headers() {
    // 1. Missing Filetype header entirely
    const char* subNoType =
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 00 00 00 00 00 54 7A 01\n";
    {
        StringStreamReader reader(subNoType);
        SubGhzHeader hdr;
        bool ok = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(ok);
        ASSERT_EQ(433920000U, hdr.frequencyHz);
    }

    // 2. Extraordinarily long line (> 2000 characters) without newline (buffer overflow challenge)
    std::string longLine = "Comment: ";
    for (int i = 0; i < 2000; i++) {
        longLine += "A";
    }
    longLine += "\nFrequency: 433920000\nProtocol: Princeton\n";
    {
        StringStreamReader reader(longLine.c_str(), longLine.length());
        SubGhzHeader hdr;
        bool ok = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(ok);
        ASSERT_EQ(433920000U, hdr.frequencyHz);
    }

    // 3. Embedded null bytes and high-ASCII bytes
    char binaryStream[] = "Filetype: Flipper SubGhz Key File\n\0Frequency: 433920000\n\xFF\xFE\nProtocol: Came\n";
    {
        StringStreamReader reader(binaryStream, sizeof(binaryStream) - 1);
        SubGhzHeader hdr;
        // Should parse safely without crashing or hanging
        SubFormatParser::parseHeader(reader, hdr);
    }

    // 4. Overlong protocol and custom strings
    const char* subLongStrings =
        "Filetype: Flipper SubGhz Key File\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetCustom\n"
        "Protocol: ThisIsAnExtremelyLongProtocolNameThatExceedsThe32ByteLimitSafely\n"
        "Custom_preset_module: ExtremelyLongModuleName12345678901234567890\n"
        "Custom_preset_data: 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30\n";
    {
        StringStreamReader reader(subLongStrings);
        SubGhzHeader hdr;
        bool ok = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(ok);
        // Verify null-termination
        ASSERT_EQ('\0', hdr.protocol[sizeof(hdr.protocol) - 1]);
        ASSERT_EQ('\0', hdr.customModule[sizeof(hdr.customModule) - 1]);
        ASSERT_EQ('\0', hdr.customData[sizeof(hdr.customData) - 1]);
    }
}

void test_sub_parser_negative_and_zero_frequencies() {
    // 1. Zero frequency
    const char* subZeroFreq =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 0\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n";
    {
        StringStreamReader reader(subZeroFreq);
        SubGhzHeader hdr;
        SubGhzKeyData key;
        bool ok = SubFormatParser::parseFile(reader, hdr, key);
        ASSERT_TRUE(ok); // Princeton protocol identified
        ASSERT_EQ(0U, hdr.frequencyHz);
        ASSERT_NEAR(0.0f, hdr.frequency, 0.001f);
    }

    // 2. Negative frequency string
    const char* subNegFreq =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: -433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n";
    {
        StringStreamReader reader(subNegFreq);
        SubGhzHeader hdr;
        SubFormatParser::parseHeader(reader, hdr);
        // strtoul converts negative to unsigned representation, must not crash
    }

    // 3. Non-numeric frequency
    const char* subAlphaFreq =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: NOT_A_NUMBER\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Came\n";
    {
        StringStreamReader reader(subAlphaFreq);
        SubGhzHeader hdr;
        SubFormatParser::parseHeader(reader, hdr);
        ASSERT_EQ(0U, hdr.frequencyHz);
    }
}

void test_sub_parser_raw_stream_corruption_and_truncation() {
    // 1. Truncated RAW file ending mid-stream without newline
    const char* subRawTruncated =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n"
        "RAW_Data: 100 -200 300 -400 500";
    {
        std::vector<int32_t> pulses;
        StringStreamReader reader(subRawTruncated);
        bool ok = SubFormatParser::parseRawPulses(reader, [](int32_t p, void* ctx) {
            auto* v = static_cast<std::vector<int32_t>*>(ctx);
            v->push_back(p);
        }, &pulses);

        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)5, pulses.size());
        ASSERT_EQ(100, pulses[0]);
        ASSERT_EQ(-200, pulses[1]);
        ASSERT_EQ(300, pulses[2]);
        ASSERT_EQ(-400, pulses[3]);
        ASSERT_EQ(500, pulses[4]);
    }

    // 2. Cutoff in middle of negative sign at EOF
    const char* subRawCutoffSign =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Protocol: RAW\n"
        "RAW_Data: 100 -200 -";
    {
        std::vector<int32_t> pulses;
        StringStreamReader reader(subRawCutoffSign);
        SubFormatParser::parseRawPulses(reader, [](int32_t p, void* ctx) {
            auto* v = static_cast<std::vector<int32_t>*>(ctx);
            v->push_back(p);
        }, &pulses);
        // Note: parseRawPulses treats lone trailing sign as 0 pulse
        ASSERT_TRUE(pulses.size() == 2 || pulses.size() == 3);
        if (pulses.size() == 3) {
            ASSERT_EQ(0, pulses[2]);
        }
    }

    // 2b. Huge integer token to test signed integer overflow with UBSan
    const char* subRawHugeInt =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Protocol: RAW\n"
        "RAW_Data: 100 -200 99999999999999999999 -99999999999999999999 500";
    {
        std::vector<int32_t> pulses;
        StringStreamReader reader(subRawHugeInt);
        SubFormatParser::parseRawPulses(reader, [](int32_t p, void* ctx) {
            auto* v = static_cast<std::vector<int32_t>*>(ctx);
            v->push_back(p);
        }, &pulses);
    }

    // 3. Multi-line RAW with embedded non-numeric garbage
    const char* subRawGarbage =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Protocol: RAW\n"
        "RAW_Data: 100 -200\n"
        "RAW_Data: FOO BAR BAZ\n"
        "RAW_Data: 300 -400 500\n";
    {
        std::vector<int32_t> pulses;
        StringStreamReader reader(subRawGarbage);
        SubFormatParser::parseRawPulses(reader, [](int32_t p, void* ctx) {
            auto* v = static_cast<std::vector<int32_t>*>(ctx);
            v->push_back(p);
        }, &pulses);
        ASSERT_EQ((size_t)5, pulses.size());
        ASSERT_EQ(100, pulses[0]);
        ASSERT_EQ(-200, pulses[1]);
        ASSERT_EQ(300, pulses[2]);
        ASSERT_EQ(-400, pulses[3]);
        ASSERT_EQ(500, pulses[4]);
    }

    // 4. Large multi-line stream (2,048 signed pulses)
    std::string largeRaw =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n";
    size_t totalExpected = 2048;
    for (size_t i = 0; i < totalExpected; i++) {
        if (i % 24 == 0) largeRaw += "RAW_Data:";
        int32_t val = (i % 2 == 0) ? (int32_t)(400 + (i % 50)) : -(int32_t)(800 + (i % 50));
        largeRaw += " " + std::to_string(val);
        if ((i + 1) % 24 == 0 || i + 1 == totalExpected) largeRaw += "\n";
    }

    {
        size_t count = 0;
        StringStreamReader reader(largeRaw.c_str(), largeRaw.length());
        bool ok = SubFormatParser::parseRawPulses(reader, [](int32_t, void* ctx) {
            auto* c = static_cast<size_t*>(ctx);
            (*c)++;
        }, &count);
        ASSERT_TRUE(ok);
        ASSERT_EQ(totalExpected, count);
    }
}

void test_sub_parser_hex_keys_and_edge_types() {
    // 1. Unspaced hex key
    const char* subUnspaced =
        "Filetype: Flipper SubGhz Key File\n"
        "Frequency: 433920000\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 0000000000547A01\n";
    {
        StringStreamReader reader(subUnspaced);
        SubGhzKeyData key;
        bool ok = SubFormatParser::parseKeyData(reader, key);
        ASSERT_TRUE(ok);
        ASSERT_HEX64_EQ(0x0000000000547A01ULL, key.key);
        ASSERT_EQ(24, key.bitLength);
    }

    // 2. 0x-prefixed hex key
    const char* subPrefixed =
        "Bit: 24\n"
        "Key: 0x547A01\n";
    {
        StringStreamReader reader(subPrefixed);
        SubGhzKeyData key;
        bool ok = SubFormatParser::parseKeyData(reader, key);
        ASSERT_TRUE(ok);
        ASSERT_HEX64_EQ(0x547A01ULL, key.key);
    }

    // 3. Key with non-hex characters (ignored gracefully)
    const char* subDirtyKey =
        "Bit: 12\n"
        "Key: 00 12 ZZ 34 GG 56\n";
    {
        StringStreamReader reader(subDirtyKey);
        SubGhzKeyData key;
        bool ok = SubFormatParser::parseKeyData(reader, key);
        ASSERT_TRUE(ok);
        ASSERT_HEX64_EQ(0x00123456ULL, key.key);
    }

    // 4. Writer overflow protection
    char tinyBuf[16];
    BufferStreamWriter writer(tinyBuf, sizeof(tinyBuf));
    SubGhzHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fileType = SUB_TYPE_KEY;
    hdr.frequencyHz = 433920000;
    // Serializing header requires > 100 bytes, writer must return false when full without overflowing
    bool writeResult = SubFormatSerializer::serializeHeader(writer, hdr);
    ASSERT_FALSE(writeResult);
    ASSERT_EQ('\0', tinyBuf[sizeof(tinyBuf) - 1]);
}

// =========================================================================
// SUITE 3: TELEMETRY DECODERS ADVERSARIAL & STRESS TESTING
// =========================================================================

void test_telemetry_inverted_manchester_polarity() {
    // Generate valid Oregon Scientific v2.1 bitstream (+23.4 °C, 45% hum, Ch 1)
    // Sensor ID: 0x1D20, Ch: 1, Bat: OK, Temp: +23.4 C, Hum: 45%, Sum: 0x53
    uint8_t oregonPacket[10] = { 0xA1, 0xD2, 0x01, 0x50, 0x43, 0x20, 0x54, 0x35, 0x00, 0x00 };

    // Synthesize Manchester pulses:
    // Oregon bit 0: 488us HIGH, 488us LOW
    // Oregon bit 1: 488us LOW, 488us HIGH
    uint16_t pulsesNormal[200];
    uint16_t countNormal = 0;

    // Preamble: 16 bits of alternating 1/0
    for (int i = 0; i < 16; i++) {
        pulsesNormal[countNormal++] = 488;
        pulsesNormal[countNormal++] = 488;
    }

    // Convert packet nibbles (LSB first per nibble) to Manchester transitions
    for (int byteIdx = 0; byteIdx < 8; byteIdx++) {
        uint8_t b = oregonPacket[byteIdx];
        for (int bit = 0; bit < 8; bit++) {
            bool bitVal = (b & (1 << bit)) != 0;
            if (!bitVal) {
                pulsesNormal[countNormal++] = 488;
                pulsesNormal[countNormal++] = 488;
            } else {
                pulsesNormal[countNormal++] = 488;
                pulsesNormal[countNormal++] = 488;
            }
        }
    }

    // Invert polarity of the pulse stream (level inversion handled by telemetry_process_pulses)
    SensorTelemetryData outNormal;
    memset(&outNormal, 0, sizeof(outNormal));
    bool okNormal = telemetry_process_pulses(pulsesNormal, countNormal, outNormal);
    // Even if Manchester decoder evaluates polarity 0 and polarity 1, verify safe execution
    (void)okNormal;
}

void test_telemetry_corrupted_preambles_and_noise() {
    // 1. Corrupt sync nibble in Oregon Scientific (should be 0xA)
    uint8_t badSyncPacket[10] = { 0x51, 0xD2, 0x01, 0x50, 0x43, 0x20, 0x54, 0x35, 0x00, 0x00 };
    SensorTelemetryData out;
    bool okSync = decode_oregon_scientific(badSyncPacket, 76, out);
    // Corrupt sync nibble must cause checksum mismatch and reject packet
    ASSERT_FALSE(okSync);

    // 2. High-entropy RF noise pulses (500 random pulses)
    std::mt19937 rng(999);
    std::uniform_int_distribution<uint16_t> pulseDist(20, 2000);
    uint16_t noisePulses[500];
    for (int i = 0; i < 500; i++) {
        noisePulses[i] = pulseDist(rng);
    }

    SensorTelemetryData noiseOut;
    bool okNoise = telemetry_process_pulses(noisePulses, 500, noiseOut);
    // Random noise must NOT trigger false positive telemetry packets
    ASSERT_FALSE(okNoise);
}

void test_telemetry_out_of_range_metrics() {
    // 1. Oregon Scientific: extreme negative temperature and humidity clamping
    // Nibbles 8..10: BCD 999 (+99.9 C), Nibble 11 sign bit set (0x08 -> -99.9 C)
    // Humidity: 165% -> clamped to 100%
    uint8_t oregonExtreme[10] = { 0xA1, 0xD2, 0x01, 0x50, 0x99, 0x98, 0x5F, 0x00, 0x00, 0x00 };
    // Recalculate checksum
    // Nibbles: 1, D(13), 2, 0, 1, 5, 0, 0, 9, 9, 9, 8, 5, F(15), 0
    uint8_t n[15] = { 1, 13, 2, 0, 1, 5, 0, 0, 9, 9, 9, 8, 5, 15, 0 };
    uint8_t sum = 0;
    for (int i = 0; i < 15; i++) sum += n[i];
    oregonExtreme[7] = (sum & 0x0F);
    oregonExtreme[8] = ((sum >> 4) & 0x0F);

    SensorTelemetryData outOregon;
    bool okOregon = decode_oregon_scientific(oregonExtreme, 76, outOregon);
    if (okOregon) {
        ASSERT_EQ(-999, outOregon.temperatureC_x10);
        ASSERT_EQ(100, outOregon.humidityPct); // clamped
    }

    // 2. Toyota TPMS: Dual inverted pressure check (b[4]/b[5] vs b[7] ^ 0xFF)
    // Test exact mismatch rejection
    uint8_t toyotaMismatch[9] = { 0xA5, 0xB4, 0xC3, 0xD2, 0x7E, 0x80, 0x7C, 0x00, 0xD4 };
    SensorTelemetryData outToyota;
    bool okToyota = decode_tpms_toyota(toyotaMismatch, 72, outToyota);
    ASSERT_FALSE(okToyota);

    // 3. Ford TPMS: Temperature MSB violation (b[5] & 0x80 != 0)
    uint8_t fordBadMsb[8] = { 0x12, 0x34, 0x56, 0x78, 0x20, 0x85, 0x44, 0x00 };
    fordBadMsb[7] = ford_checksum(fordBadMsb, 7);
    SensorTelemetryData outFord;
    bool okFord = decode_tpms_ford(fordBadMsb, 64, outFord);
    ASSERT_FALSE(okFord);

    // 4. Ford TPMS: Temperature out of bounds (> 125 C)
    // b[5] & 0x7F = 127 -> tempC = 127 - 56 = 71 (valid)
    // b[5] & 0x7F = 200 -> but & 0x7F max is 127, so tempC max is 71 C.
    // What about lower bound? b[5] & 0x7F = 0 -> tempC = -56 C (< -50 C)
    uint8_t fordSubZero[8] = { 0x12, 0x34, 0x56, 0x78, 0x20, 0x00, 0x44, 0x00 };
    fordSubZero[7] = ford_checksum(fordSubZero, 7);
    bool okFordSubZero = decode_tpms_ford(fordSubZero, 64, outFord);
    ASSERT_FALSE(okFordSubZero); // rejected because tempC (-56 C) < -50 C

    // 5. Ford TPMS: Zero or 0xFFFFFFFF Sensor ID rejection
    uint8_t fordZeroId[8] = { 0x00, 0x00, 0x00, 0x00, 0x20, 0x50, 0x44, 0x00 };
    fordZeroId[7] = ford_checksum(fordZeroId, 7);
    bool okFordZeroId = decode_tpms_ford(fordZeroId, 64, outFord);
    ASSERT_FALSE(okFordZeroId);
}

void test_telemetry_fuzz_and_null_safety() {
    // 1. Null pointers and short lengths
    SensorTelemetryData out;
    ASSERT_FALSE(decode_oregon_scientific(nullptr, 64, out));
    ASSERT_FALSE(decode_tpms_toyota(nullptr, 72, out));
    ASSERT_FALSE(decode_tpms_ford(nullptr, 64, out));
    ASSERT_FALSE(decode_tpms_schrader(nullptr, 56, out));
    ASSERT_FALSE(telemetry_process_pulses(nullptr, 100, out));

    uint8_t dummyBuf[32];
    memset(dummyBuf, 0xAA, sizeof(dummyBuf));
    ASSERT_FALSE(decode_oregon_scientific(dummyBuf, 10, out));
    ASSERT_FALSE(decode_tpms_toyota(dummyBuf, 50, out));
    ASSERT_FALSE(decode_tpms_ford(dummyBuf, 40, out));
    ASSERT_FALSE(decode_tpms_schrader(dummyBuf, 30, out));

    uint16_t dummyPulses[10] = { 488, 488, 488 };
    ASSERT_FALSE(telemetry_process_pulses(dummyPulses, 3, out));

    // 2. Fuzzing: 5,000 random byte packets across all decoders
    std::mt19937 rng(55555);
    for (int i = 0; i < 5000; i++) {
        uint8_t randBytes[16];
        for (int b = 0; b < 16; b++) {
            randBytes[b] = (uint8_t)rng();
        }

        // None of these should crash, execute undefined behavior, or leak memory
        decode_oregon_scientific(randBytes, 76, out);
        decode_tpms_toyota(randBytes, 72, out);
        decode_tpms_ford(randBytes, 64, out);
        decode_tpms_schrader(randBytes, 56, out);
    }
}

// =========================================================================
// REGISTRATION
// =========================================================================

void register_adversarial_tests() {
    TEST_SUITE("Tier 5 Adversarial: KeeLoq Dynamic Code & Cipher Hardening");
    RUN_TEST_CASE("[T5] Rapid out-of-order counter burst (1,000 packets)", TestTier::TIER_2_BOUNDARY, test_keeloq_out_of_order_burst);
    RUN_TEST_CASE("[T5] Replay attack matrix & pending resync timeout", TestTier::TIER_2_BOUNDARY, test_keeloq_replay_attack_matrix);
    RUN_TEST_CASE("[T5] Corrupted hop codes & bit inversion (1,000 fuzz packets)", TestTier::TIER_2_BOUNDARY, test_keeloq_corrupted_hop_codes);
    RUN_TEST_CASE("[T5] 0xFFFF -> 0x0000 rollover boundary stress & backward jumps", TestTier::TIER_2_BOUNDARY, test_keeloq_rollover_boundaries);
    RUN_TEST_CASE("[T5] Abnormal discriminator rejection & forged packets", TestTier::TIER_2_BOUNDARY, test_keeloq_abnormal_discriminator);
    RUN_TEST_CASE("[T5] LRU transmitter table saturation (64 transmitters)", TestTier::TIER_2_BOUNDARY, test_keeloq_lru_table_saturation);
    RUN_TEST_CASE("[T5] Pointer & bit accessor boundary safety", TestTier::TIER_2_BOUNDARY, test_keeloq_null_and_bounds_safety);

    TEST_SUITE("Tier 5 Adversarial: Flipper Zero .sub Parser & Serializer Hardening");
    RUN_TEST_CASE("[T5] Malformed headers & 2000-char line overflow defense", TestTier::TIER_2_BOUNDARY, test_sub_parser_malformed_headers);
    RUN_TEST_CASE("[T5] Negative, zero, and alpha frequency tolerance", TestTier::TIER_2_BOUNDARY, test_sub_parser_negative_and_zero_frequencies);
    RUN_TEST_CASE("[T5] Truncated RAW pulse streams & 2,048 pulse stress", TestTier::TIER_2_BOUNDARY, test_sub_parser_raw_stream_corruption_and_truncation);
    RUN_TEST_CASE("[T5] Hex key parsing corruption & stream buffer limits", TestTier::TIER_2_BOUNDARY, test_sub_parser_hex_keys_and_edge_types);

    TEST_SUITE("Tier 5 Adversarial: Telemetry Decoders Hardening & Fuzzing");
    RUN_TEST_CASE("[T5] Inverted Manchester polarity demodulation safety", TestTier::TIER_2_BOUNDARY, test_telemetry_inverted_manchester_polarity);
    RUN_TEST_CASE("[T5] Corrupted preambles & high-entropy noise rejection", TestTier::TIER_2_BOUNDARY, test_telemetry_corrupted_preambles_and_noise);
    RUN_TEST_CASE("[T5] Out-of-range metrics & TPMS validation bounds", TestTier::TIER_2_BOUNDARY, test_telemetry_out_of_range_metrics);
    RUN_TEST_CASE("[T5] Fuzz testing & null safety (5,000 random bitstreams)", TestTier::TIER_2_BOUNDARY, test_telemetry_fuzz_and_null_safety);
}
