// GHOSTRF Adversarial Hardening Challenger Test Suite (Challenger R2_1)
// Rigorous verification under AddressSanitizer and UndefinedBehaviorSanitizer
// Scope:
// 1. SubFormatParser 20+ digit pulse durations (99999999999999999999) saturation without UB
// 2. SubFormatParser lone trailing signs (+/- at EOF or with spaces) without 0-duration pulse emission
// 3. KeeLoq resync window across 32-bit uptime rollover boundary (currentTimeMs = 5, resyncTimestampMs = 0xFFFFFFF0)
// 4. KeeLoq out-of-order counter sequences, counter rollover 0xFFFF -> 0, replays, and discriminator validation

#include "test_harness.h"
#include "sub_format.h"
#include "keeloq.h"
#include "keeloq_analyzer.h"

#include <vector>
#include <string>
#include <cstring>
#include <climits>
#include <random>
#include <iostream>

using ghostrf_test::TestTier;

// Helper to collect pulses from parseRawPulses
struct PulseCollector {
    std::vector<int32_t> pulses;
    static void callback(int32_t pulse, void* ctx) {
        auto* self = static_cast<PulseCollector*>(ctx);
        self->pulses.push_back(pulse);
    }
};

// =========================================================================
// TEST SUITE 1: SUB FORMAT PARSER ARITHMETIC SATURATION & LONE SIGN SAFETY
// =========================================================================

void test_sub_format_parse_raw_20_digit_saturation() {
    // 1. 20 digits positive: 99999999999999999999
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 99999999999999999999\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)1, col.pulses.size());
        ASSERT_EQ(INT32_MAX, col.pulses[0]);
    }

    // 2. 20 digits negative: -99999999999999999999
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: -99999999999999999999\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)1, col.pulses.size());
        ASSERT_EQ(INT32_MIN, col.pulses[0]);
    }

    // 3. 35 digits extreme saturation (both positive and negative)
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 10000000000000000000000000000000000 -10000000000000000000000000000000000\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)2, col.pulses.size());
        ASSERT_EQ(INT32_MAX, col.pulses[0]);
        ASSERT_EQ(INT32_MIN, col.pulses[1]);
    }

    // 4. Exact boundary values around INT32_MAX (2147483647)
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 2147483646 2147483647 2147483648 2147483649\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)4, col.pulses.size());
        ASSERT_EQ(2147483646, col.pulses[0]);
        ASSERT_EQ(INT32_MAX, col.pulses[1]);
        ASSERT_EQ(INT32_MAX, col.pulses[2]); // Saturated
        ASSERT_EQ(INT32_MAX, col.pulses[3]); // Saturated
    }

    // 5. Exact boundary values around INT32_MIN (-2147483648)
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: -2147483647 -2147483648 -2147483649 -2147483650\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)4, col.pulses.size());
        ASSERT_EQ(-2147483647, col.pulses[0]);
        ASSERT_EQ(INT32_MIN, col.pulses[1]);
        ASSERT_EQ(INT32_MIN, col.pulses[2]); // Saturated
        ASSERT_EQ(INT32_MIN, col.pulses[3]); // Saturated
    }

    // 6. Interleaved array with 20-digit values and standard pulses
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 500 -99999999999999999999 1234 99999999999999999999 -300\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)5, col.pulses.size());
        ASSERT_EQ(500, col.pulses[0]);
        ASSERT_EQ(INT32_MIN, col.pulses[1]);
        ASSERT_EQ(1234, col.pulses[2]);
        ASSERT_EQ(INT32_MAX, col.pulses[3]);
        ASSERT_EQ(-300, col.pulses[4]);
    }
}

void test_sub_format_parse_raw_lone_trailing_sign() {
    // 1. Lone trailing '-' at EOF
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 100 -200 -";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        // Must emit strictly 2 pulses, NO 0-duration pulse!
        ASSERT_EQ((size_t)2, col.pulses.size());
        ASSERT_EQ(100, col.pulses[0]);
        ASSERT_EQ(-200, col.pulses[1]);
        for (int32_t p : col.pulses) {
            ASSERT_NE(0, p);
        }
    }

    // 2. Lone trailing '+' at EOF
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 100 -200 +";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)2, col.pulses.size());
        ASSERT_EQ(100, col.pulses[0]);
        ASSERT_EQ(-200, col.pulses[1]);
        for (int32_t p : col.pulses) {
            ASSERT_NE(0, p);
        }
    }

    // 3. Lone trailing signs followed by spaces/tabs at EOF
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 350 -450 - \t  ";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)2, col.pulses.size());
        ASSERT_EQ(350, col.pulses[0]);
        ASSERT_EQ(-450, col.pulses[1]);
        for (int32_t p : col.pulses) {
            ASSERT_NE(0, p);
        }
    }

    // 4. Lone signs scattered between valid numbers with spaces
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: 100 -  200 +   -300 - \n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)3, col.pulses.size());
        ASSERT_EQ(100, col.pulses[0]);
        ASSERT_EQ(200, col.pulses[1]);
        ASSERT_EQ(-300, col.pulses[2]);
        for (int32_t p : col.pulses) {
            ASSERT_NE(0, p);
        }
    }

    // 5. Lines containing ONLY lone signs
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: -\n"
            "RAW_Data: +\n"
            "RAW_Data: -   +   -\n"
            "RAW_Data: 700 -800\n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        ASSERT_TRUE(ok);
        ASSERT_EQ((size_t)2, col.pulses.size());
        ASSERT_EQ(700, col.pulses[0]);
        ASSERT_EQ(-800, col.pulses[1]);
        for (int32_t p : col.pulses) {
            ASSERT_NE(0, p);
        }
    }

    // 6. Entire file has only lone signs (no digits anywhere)
    {
        const char* sub =
            "Filetype: Flipper SubGhz RAW File\n"
            "Frequency: 433920000\n"
            "Protocol: RAW\n"
            "RAW_Data: - + - + - \n";
        StringStreamReader reader(sub);
        PulseCollector col;
        bool ok = SubFormatParser::parseRawPulses(reader, PulseCollector::callback, &col);
        // Must return false because pulseCount == 0, and callback was never called with 0
        ASSERT_FALSE(ok);
        ASSERT_EQ((size_t)0, col.pulses.size());
    }
}

// =========================================================================
// TEST SUITE 2: KEELOQ 32-BIT UPTIME ROLLOVER RESYNC STATE MACHINE
// =========================================================================

void test_keeloq_uptime_rollover_resync_state_machine() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0123456;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    // Initial baseline counter set at 1000
    uint16_t baselineCounter = 1000;
    keeloq_analyzer_set_last_counter(serial, baselineCounter);

    // Target Scenario:
    // First press jumps to counter 1100 (delta = 100, zone 2 resync window 17..32767)
    // resyncTimestampMs is recorded at 0xFFFFFFF0 (16 ms before 32-bit wrap)
    const uint32_t t1 = 0xFFFFFFF0U;
    uint16_t jumpCounter = 1100;
    uint32_t plain1 = ((uint32_t)jumpCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t hop1 = keeloq_encrypt(plain1, devKey);

    KeeloqAnalysisResult res1;
    bool ok1 = keeloq_analyze_packet_timed(serial, 0x01, hop1, masterKey, t1, res1);
    ASSERT_TRUE(ok1);
    ASSERT_TRUE(res1.discriminatorValid);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res1.syncStatus);
    ASSERT_EQ(100, res1.counterDelta);
    ASSERT_EQ(jumpCounter, res1.decryptedCounter);

    // Second press: consecutive counter (1101) arrives at currentTimeMs = 5
    // Duration elapsed across 32-bit unsigned rollover: (uint32_t)(5 - 0xFFFFFFF0) = 21 ms
    // 21 ms <= 20,000 ms (KLQ_RESYNC_TIMEOUT_MS) -> MUST BE ACCEPTED
    const uint32_t t2 = 5U;
    uint16_t consecutiveCounter = 1101;
    uint32_t plain2 = ((uint32_t)consecutiveCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t hop2 = keeloq_encrypt(plain2, devKey);

    KeeloqAnalysisResult res2;
    bool ok2 = keeloq_analyze_packet_timed(serial, 0x01, hop2, masterKey, t2, res2);
    ASSERT_TRUE(ok2);
    ASSERT_TRUE(res2.discriminatorValid);
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res2.syncStatus);
    ASSERT_EQ(1, res2.counterDelta);
    ASSERT_EQ(consecutiveCounter, res2.decryptedCounter);

    // Third press: next sequential counter (1102) arrives at currentTimeMs = 50
    // Receiver should now be fully synchronized in Zone 1 (Open Window)
    const uint32_t t3 = 50U;
    uint16_t nextSeqCounter = 1102;
    uint32_t plain3 = ((uint32_t)nextSeqCounter & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
    uint32_t hop3 = keeloq_encrypt(plain3, devKey);

    KeeloqAnalysisResult res3;
    bool ok3 = keeloq_analyze_packet_timed(serial, 0x01, hop3, masterKey, t3, res3);
    ASSERT_TRUE(ok3);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res3.syncStatus);
    ASSERT_EQ(1, res3.counterDelta);
    ASSERT_EQ(nextSeqCounter, res3.decryptedCounter);
}

void test_keeloq_uptime_rollover_timeout_boundaries() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0987654;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    keeloq_analyzer_set_last_counter(serial, 2000);

    // 1. Boundary condition: Exactly 20,000 ms elapsed across rollover
    // t1 = 0xFFFFFFF0U. (0xFFFFFFF0 + 20000 = 19984)
    const uint32_t t1 = 0xFFFFFFF0U;
    const uint32_t tExact = 19984U; // elapsed = (uint32_t)(19984 - 0xFFFFFFF0) = 20000 ms
    uint16_t jump1 = 2050;
    uint32_t hopJump1 = keeloq_encrypt(((uint32_t)jump1 & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);

    KeeloqAnalysisResult resJ1;
    keeloq_analyze_packet_timed(serial, 0x01, hopJump1, masterKey, t1, resJ1);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resJ1.syncStatus);

    uint16_t consec1 = 2051;
    uint32_t hopConsec1 = keeloq_encrypt(((uint32_t)consec1 & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult resC1;
    keeloq_analyze_packet_timed(serial, 0x01, hopConsec1, masterKey, tExact, resC1);
    ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resC1.syncStatus);

    // 2. Boundary condition: Exactly 20,001 ms elapsed across rollover (Timeout Expired)
    // tJump2 = 0xFFFFFFF0U, tExpired = 19985U (elapsed = 20001 ms)
    keeloq_analyzer_set_last_counter(serial, 3000);
    uint16_t jump2 = 3050;
    uint32_t hopJump2 = keeloq_encrypt(((uint32_t)jump2 & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult resJ2;
    keeloq_analyze_packet_timed(serial, 0x01, hopJump2, masterKey, t1, resJ2);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resJ2.syncStatus);

    uint16_t consec2 = 3051;
    uint32_t hopConsec2 = keeloq_encrypt(((uint32_t)consec2 & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult resC2;
    keeloq_analyze_packet_timed(serial, 0x01, hopConsec2, masterKey, 19985U, resC2);
    // Timeout has expired: must NOT resync!
    ASSERT_NE((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)resC2.syncStatus);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)resC2.syncStatus);
}

void test_keeloq_lru_eviction_across_uptime_rollover() {
    keeloq_analyzer_reset();

    // Register 32 transmitters at t = 0xFFFFFF00U (~49.7 days before rollover)
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t s = 0x0100000U + i;
        keeloq_analyzer_set_last_counter(s, 100);
    }

    // Transmitter 0 is actively seen after rollover at t = 5U
    // Its timestamp becomes 5U (age at t=10U is 5 ms)
    // Transmitters 1..31 have timestamp 0xFFFFFF00U (age at t=10U is 10 - 0xFFFFFF00 = 266 ms)
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint32_t s0 = 0x0100000U;
    uint64_t devKey0 = keeloq_derive_key_normal(s0, masterKey);
    uint32_t plain0 = (101U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)s0 & 0x03FFU) << 20);
    uint32_t hop0 = keeloq_encrypt(plain0, devKey0);
    KeeloqAnalysisResult res0;
    keeloq_analyze_packet_timed(s0, 0x01, hop0, masterKey, 5U, res0);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res0.syncStatus);

    // Now at t = 10U, allocate 33rd transmitter (0x0100099U)
    // Under correct unsigned rollover subtraction, transmitter 0 has age 5 ms, while 1..31 have age 266 ms.
    // Transmitter 0 must NOT be evicted!
    uint32_t sNew = 0x0100099U;
    uint64_t devKeyNew = keeloq_derive_key_normal(sNew, masterKey);
    uint32_t plainNew = (50U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)sNew & 0x03FFU) << 20);
    uint32_t hopNew = keeloq_encrypt(plainNew, devKeyNew);
    KeeloqAnalysisResult resNew;
    keeloq_analyze_packet_timed(sNew, 0x01, hopNew, masterKey, 10U, resNew);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)resNew.syncStatus);

    // Verify transmitter 0 is still retained in table!
    bool hasCounter = false;
    uint16_t c0 = keeloq_analyzer_get_last_counter(s0, &hasCounter);
    ASSERT_TRUE(hasCounter);
    ASSERT_EQ(101, c0);
}

// =========================================================================
// TEST SUITE 3: KEELOQ INVARIANTS: OUT-OF-ORDER, ROLLOVER, REPLAY, DISC
// =========================================================================

void test_keeloq_counter_wrap_0xffff_to_0() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0445566;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    // Baseline near top: 0xFFFE
    keeloq_analyzer_set_last_counter(serial, 0xFFFE);

    // 1. Single increment to 0xFFFF
    {
        uint32_t plain = (0xFFFFU & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1000, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
        ASSERT_EQ(1, res.counterDelta);
        ASSERT_EQ(0xFFFF, res.decryptedCounter);
    }

    // 2. Rollover increment to 0x0000
    {
        uint32_t plain = (0x0000U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1100, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
        ASSERT_EQ(1, res.counterDelta);
        ASSERT_EQ(0x0000, res.decryptedCounter);
    }

    // 3. Increment across wrap to 0x0005 (delta = 5, within Open Window 1..16)
    {
        uint32_t plain = (0x0005U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1200, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);
        ASSERT_EQ(5, res.counterDelta);
        ASSERT_EQ(0x0005, res.decryptedCounter);
    }

    // 4. Jump into Resync Window across wrap: 0x0005 -> 0x0020 (delta = 27, zone 2)
    {
        uint32_t plain = (0x0020U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1300, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);
        ASSERT_EQ(27, res.counterDelta);
    }

    // 5. Consecutive resync packet: 0x0021 at 1400 ms
    {
        uint32_t plain = (0x0021U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1400, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_OK_RESYNC_WINDOW, (int)res.syncStatus);
        ASSERT_EQ(1, res.counterDelta);
        ASSERT_EQ(0x0021, res.decryptedCounter);
    }

    // 6. Backward jump across wrap: lastCounter is 0x0021, packet arrives with 0xFFF0
    // Unsigned delta = (uint16_t)(0xFFF0 - 0x0021) = 65487 >= 32768 (Zone 3 Blocked)
    {
        uint32_t plain = (0xFFF0U & 0xFFFFU) | (0x01U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);
        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x01, hop, masterKey, 1500, res);
        ASSERT_TRUE(ok);
        ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)res.syncStatus);
    }
}

void test_keeloq_adversarial_replay_and_discriminator() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x06789AB;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    uint16_t cnt = 4000;
    keeloq_analyzer_set_last_counter(serial, cnt);

    // 1. Advance counter normally (+1)
    uint32_t hopValid = keeloq_encrypt(((uint32_t)4001 & 0xFFFFU) | (0x02U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult res;
    bool ok = keeloq_analyze_packet_timed(serial, 0x02, hopValid, masterKey, 1000, res);
    ASSERT_TRUE(ok);
    ASSERT_EQ((int)KLQ_SYNC_OK_SEQUENTIAL, (int)res.syncStatus);

    // 2. Direct Replay: Send identical packet 4001 again (delta = 0)
    for (int rep = 0; rep < 10; rep++) {
        KeeloqAnalysisResult repRes;
        bool repOk = keeloq_analyze_packet_timed(serial, 0x02, hopValid, masterKey, 1100 + rep * 10, repRes);
        ASSERT_TRUE(repOk);
        ASSERT_TRUE(repRes.discriminatorValid);
        ASSERT_EQ((int)KLQ_SYNC_ERR_REPLAY_DETECTED, (int)repRes.syncStatus);
        ASSERT_EQ(0, repRes.counterDelta);
    }

    // 3. Stale Replay: Send older counter (4000) -> delta = 65535 >= 32768
    uint32_t hopOld = keeloq_encrypt(((uint32_t)4000 & 0xFFFFU) | (0x02U << 16) | (((uint32_t)serial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult oldRes;
    bool oldOk = keeloq_analyze_packet_timed(serial, 0x02, hopOld, masterKey, 1300, oldRes);
    ASSERT_TRUE(oldOk);
    ASSERT_EQ((int)KLQ_SYNC_ERR_DESYNCHRONIZED, (int)oldRes.syncStatus);

    // 4. Discriminator validation:
    // Expected discriminator is serial & 0x03FF = 0x1AB
    // Corrupt bit 0: 0x1AA
    uint32_t badDisc1 = ((uint32_t)serial & 0x03FFU) ^ 1U;
    uint32_t hopBadDisc1 = keeloq_encrypt(((uint32_t)4002 & 0xFFFFU) | (0x02U << 16) | (badDisc1 << 20), devKey);
    KeeloqAnalysisResult badRes1;
    bool badOk1 = keeloq_analyze_packet_timed(serial, 0x02, hopBadDisc1, masterKey, 1400, badRes1);
    ASSERT_FALSE(badOk1);
    ASSERT_FALSE(badRes1.discriminatorValid);
    ASSERT_EQ((int)KLQ_SYNC_UNKNOWN, (int)badRes1.syncStatus);

    // Corrupt bit 9: 0x3AB
    uint32_t badDisc2 = ((uint32_t)serial & 0x03FFU) ^ (1U << 9);
    uint32_t hopBadDisc2 = keeloq_encrypt(((uint32_t)4002 & 0xFFFFU) | (0x02U << 16) | (badDisc2 << 20), devKey);
    KeeloqAnalysisResult badRes2;
    bool badOk2 = keeloq_analyze_packet_timed(serial, 0x02, hopBadDisc2, masterKey, 1500, badRes2);
    ASSERT_FALSE(badOk2);
    ASSERT_FALSE(badRes2.discriminatorValid);
    ASSERT_EQ((int)KLQ_SYNC_UNKNOWN, (int)badRes2.syncStatus);

    // Completely wrong serial in frame
    uint32_t wrongSerial = 0x0112233;
    uint32_t hopWrongSerial = keeloq_encrypt(((uint32_t)4002 & 0xFFFFU) | (0x02U << 16) | (((uint32_t)wrongSerial & 0x03FFU) << 20), devKey);
    KeeloqAnalysisResult badRes3;
    bool badOk3 = keeloq_analyze_packet_timed(serial, 0x02, hopWrongSerial, masterKey, 1600, badRes3);
    ASSERT_FALSE(badOk3);
    ASSERT_FALSE(badRes3.discriminatorValid);
}

void test_keeloq_high_entropy_out_of_order_stream() {
    keeloq_analyzer_reset();
    const uint32_t serial = 0x0332211;
    const uint64_t masterKey = KLQ_KEY_DOORHAN;
    uint64_t devKey = keeloq_derive_key_normal(serial, masterKey);

    uint16_t currentCounter = 10000;
    keeloq_analyzer_set_last_counter(serial, currentCounter);

    std::mt19937 rng(777);
    std::uniform_int_distribution<int> typeDist(0, 5);

    // Execute 2,000 randomized state transitions under ASan/UBSan
    for (int i = 0; i < 2000; i++) {
        int type = typeDist(rng);
        uint16_t counterToSend = currentCounter;
        KeeloqSyncStatus expectedStatus = KLQ_SYNC_UNKNOWN;

        if (type == 0) {
            // Open window consecutive (+1)
            counterToSend = (uint16_t)(currentCounter + 1);
            expectedStatus = KLQ_SYNC_OK_SEQUENTIAL;
        } else if (type == 1) {
            // Open window burst (+2..+16)
            uint16_t inc = (rng() % 15) + 2;
            counterToSend = (uint16_t)(currentCounter + inc);
            expectedStatus = KLQ_SYNC_OK_SEQUENTIAL;
        } else if (type == 2) {
            // Replay attack (delta = 0)
            counterToSend = currentCounter;
            expectedStatus = KLQ_SYNC_ERR_REPLAY_DETECTED;
        } else if (type == 3) {
            // Stale counter drop (delta >= 32768)
            uint16_t dec = (rng() % 1000) + 1;
            counterToSend = (uint16_t)(currentCounter - dec);
            expectedStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        } else if (type == 4) {
            // Jump into resync window (+30..+500)
            uint16_t jump = (rng() % 470) + 30;
            counterToSend = (uint16_t)(currentCounter + jump);
            expectedStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        } else {
            // Far out-of-sync jump (delta >= 32768)
            uint16_t blockedJump = (rng() % 1000) + 33000;
            counterToSend = (uint16_t)(currentCounter + blockedJump);
            expectedStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        }

        uint32_t plain = ((uint32_t)counterToSend & 0xFFFFU) | (0x03U << 16) | (((uint32_t)serial & 0x03FFU) << 20);
        uint32_t hop = keeloq_encrypt(plain, devKey);

        KeeloqAnalysisResult res;
        bool ok = keeloq_analyze_packet_timed(serial, 0x03, hop, masterKey, (uint32_t)(50000 + i * 50), res);
        ASSERT_TRUE(ok);
        ASSERT_TRUE(res.discriminatorValid);
        ASSERT_EQ(counterToSend, res.decryptedCounter);
        ASSERT_EQ((int)expectedStatus, (int)res.syncStatus);

        if (res.syncStatus == KLQ_SYNC_OK_SEQUENTIAL) {
            currentCounter = counterToSend;
        }
    }
}

// =========================================================================
// MAIN ENTRYPOINT
// =========================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    std::cout << "\n";
    std::cout << "======================================================================\n";
    std::cout << "     GHOSTRF ADVERSARIAL HARDENING CHALLENGER SUITE (R2_1)            \n";
    std::cout << "======================================================================\n";
    std::cout << " Platform: Linux x86_64 / Host C++17 (Strict ASan + UBSan)\n";
    std::cout << " Focus: 20-digit pulse saturation, Lone signs, Uptime rollover, Invariants\n";

    auto startOverall = std::chrono::steady_clock::now();

    TEST_SUITE("SubFormatParser Adversarial Hardening");
    RUN_TEST_CASE("parseRawPulses 20+ digit saturation without signed overflow UB", TestTier::TIER_2_BOUNDARY, test_sub_format_parse_raw_20_digit_saturation);
    RUN_TEST_CASE("parseRawPulses lone trailing +/- at EOF and with spaces (no 0 pulse)", TestTier::TIER_2_BOUNDARY, test_sub_format_parse_raw_lone_trailing_sign);

    TEST_SUITE("KeeLoq Resync Window 32-Bit Uptime Rollover Hardening");
    RUN_TEST_CASE("Resync window acceptance across 32-bit uptime rollover (t=5, resync=0xFFFFFFF0)", TestTier::TIER_2_BOUNDARY, test_keeloq_uptime_rollover_resync_state_machine);
    RUN_TEST_CASE("Resync timeout exact boundaries across rollover (20,000ms vs 20,001ms)", TestTier::TIER_2_BOUNDARY, test_keeloq_uptime_rollover_timeout_boundaries);
    RUN_TEST_CASE("LRU table eviction correctness across 32-bit uptime rollover", TestTier::TIER_2_BOUNDARY, test_keeloq_lru_eviction_across_uptime_rollover);

    TEST_SUITE("KeeLoq Dynamic Invariants: Rollover, Replays, Discriminator & Out-of-Order");
    RUN_TEST_CASE("KeeLoq 0xFFFF -> 0x0000 wrap, resync, and backward jump boundaries", TestTier::TIER_2_BOUNDARY, test_keeloq_counter_wrap_0xffff_to_0);
    RUN_TEST_CASE("KeeLoq replay attack matrix and 10-bit discriminator validation", TestTier::TIER_2_BOUNDARY, test_keeloq_adversarial_replay_and_discriminator);
    RUN_TEST_CASE("KeeLoq 2,000-packet high-entropy out-of-order stream stress", TestTier::TIER_2_BOUNDARY, test_keeloq_high_entropy_out_of_order_stream);

    auto endOverall = std::chrono::steady_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(endOverall - startOverall).count();

    auto& ctx = ::ghostrf_test::TestContext::instance();
    ctx.printSummary();

    std::cout << " Total Test Execution Time: " << std::fixed << std::setprecision(2)
              << totalElapsedMs << " ms\n\n";

    return (ctx.getFailedCount() == 0) ? 0 : 1;
}
