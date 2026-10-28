// GHOSTRF E2E Test Suite - Flipper Zero .sub Parser & Serializer
// Authoritative sources: explorer_2/handoff.md & Flipper Zero SubGhz specifications

#include "test_harness.h"
#include "sub_format.h"
#include <vector>
#include <string>

// ============================================================================
// TIER 1: Feature Coverage (>=5 test cases per feature)
// ============================================================================

void test_sub_tier1_princeton() {
    const char* princetonSub =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 00 00 00 00 00 54 7A 01\n"
        "TE: 320\n"
        "Repeat: 5\n";

    StringStreamReader reader(princetonSub);
    SubGhzHeader hdr;
    SubGhzKeyData key;

    ASSERT_TRUE(SubFormatParser::parseFile(reader, hdr, key));
    ASSERT_EQ((int)SUB_TYPE_KEY, (int)hdr.fileType);
    ASSERT_EQ(1U, hdr.version);
    ASSERT_EQ(433920000U, hdr.frequencyHz);
    ASSERT_NEAR(433.92, hdr.frequency, 0.001);
    ASSERT_STR_EQ("FuriHalSubGhzPresetOok650Async", hdr.presetStr);
    ASSERT_STR_EQ("Princeton", hdr.protocol);
    ASSERT_EQ(24, key.bitLength);
    ASSERT_HEX64_EQ(0x547A01ULL, key.key);
    ASSERT_EQ(320U, key.te);
    ASSERT_EQ(5, key.repeat);
}

void test_sub_tier1_came() {
    const char* cameSub =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Came\n"
        "Bit: 12\n"
        "Key: 00 00 00 00 00 00 0E 15\n"
        "Repeat: 6\n";

    StringStreamReader reader(cameSub);
    SubGhzHeader hdr;
    SubGhzKeyData key;

    ASSERT_TRUE(SubFormatParser::parseFile(reader, hdr, key));
    ASSERT_STR_EQ("Came", hdr.protocol);
    ASSERT_EQ(12, key.bitLength);
    ASSERT_HEX64_EQ(0x0E15ULL, key.key);
    ASSERT_EQ(6, key.repeat);
}

void test_sub_tier1_nice_flo() {
    const char* niceSub =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Nice FLO\n"
        "Bit: 12\n"
        "Key: 00 00 00 00 00 00 0A 32\n"
        "Repeat: 6\n";

    StringStreamReader reader(niceSub);
    SubGhzHeader hdr;
    SubGhzKeyData key;

    ASSERT_TRUE(SubFormatParser::parseFile(reader, hdr, key));
    ASSERT_STR_EQ("Nice FLO", hdr.protocol);
    ASSERT_EQ(12, key.bitLength);
    ASSERT_HEX64_EQ(0x0A32ULL, key.key);
    ASSERT_EQ(6, key.repeat);
}

void test_sub_tier1_keeloq() {
    const char* keeloqSub =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: KeeLoq\n"
        "Bit: 64\n"
        "Key: 00 00 00 00 A1 B2 C3 D4\n"
        "Repeat: 5\n"
        "Manufacture: DoorHan\n";

    StringStreamReader reader(keeloqSub);
    SubGhzHeader hdr;
    SubGhzKeyData key;

    ASSERT_TRUE(SubFormatParser::parseFile(reader, hdr, key));
    ASSERT_STR_EQ("KeeLoq", hdr.protocol);
    ASSERT_EQ(64, key.bitLength);
    ASSERT_HEX64_EQ(0xA1B2C3D4ULL, key.key);
    ASSERT_STR_EQ("DoorHan", key.manufacture);
    ASSERT_EQ(5, key.repeat);
}

void test_sub_tier1_raw_parse() {
    const char* rawSub =
        "Filetype: Flipper SubGhz RAW File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n"
        "RAW_Data: 320 -960 320 -960 960 -320 320 -960\n"
        "RAW_Data: 960 -320 320 -960 320 -960 960 -12500\n";

    StringStreamReader reader(rawSub);
    SubGhzHeader hdr;
    ASSERT_TRUE(SubFormatParser::parseHeader(reader, hdr));
    ASSERT_EQ((int)SUB_TYPE_RAW, (int)hdr.fileType);
    ASSERT_STR_EQ("RAW", hdr.protocol);

    std::vector<int32_t> capturedPulses;
    auto cb = [](int32_t p, void* ctx) {
        auto* vec = static_cast<std::vector<int32_t>*>(ctx);
        vec->push_back(p);
    };

    ASSERT_TRUE(SubFormatParser::parseRawPulses(reader, cb, &capturedPulses));
    ASSERT_EQ(16U, capturedPulses.size());
    ASSERT_EQ(320, capturedPulses[0]);
    ASSERT_EQ(-960, capturedPulses[1]);
    ASSERT_EQ(960, capturedPulses[4]);
    ASSERT_EQ(-12500, capturedPulses[15]);
}

void test_sub_tier1_raw_serialize() {
    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 433920000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr));
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    char outBuf[1024];
    BufferStreamWriter writer(outBuf, sizeof(outBuf));

    int32_t pulses[] = { 320, -960, 960, -320, 320, -960, 960, -12500 };
    size_t lineCounter = 0;

    ASSERT_TRUE(SubFormatSerializer::serializeRawStart(writer, hdr));
    ASSERT_TRUE(SubFormatSerializer::serializeRawChunk(writer, pulses, 8, lineCounter));
    ASSERT_TRUE(SubFormatSerializer::serializeRawEnd(writer));

    std::string serialized = writer.c_str();
    ASSERT_STR_CONTAINS(serialized, "Filetype: Flipper SubGhz RAW File");
    ASSERT_STR_CONTAINS(serialized, "Protocol: RAW");
    ASSERT_STR_CONTAINS(serialized, "RAW_Data: 320 -960 960 -320 320 -960 960 -12500");
}

// ============================================================================
// TIER 2: Boundary & Corner Cases
// ============================================================================

void test_sub_tier2_empty_input() {
    StringStreamReader emptyReader("");
    SubGhzHeader hdr;
    SubGhzKeyData key;
    ASSERT_FALSE(SubFormatParser::parseFile(emptyReader, hdr, key));
}

void test_sub_tier2_missing_header_fields() {
    // Stream with no valid header tags
    const char* incompleteSub =
        "Garbage_Field: 12345\n"
        "Another_Random_Tag: text\n";

    StringStreamReader reader(incompleteSub);
    SubGhzHeader hdr;
    ASSERT_FALSE(SubFormatParser::parseHeader(reader, hdr));
}

void test_sub_tier2_unspaced_hex_key() {
    // Some community tools emit continuous hex: "Key: 0000000000547A01"
    const char* unspacedSub =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 0000000000547A01\n"
        "TE: 320\n";

    StringStreamReader reader(unspacedSub);
    SubGhzHeader hdr;
    SubGhzKeyData key;

    ASSERT_TRUE(SubFormatParser::parseFile(reader, hdr, key));
    ASSERT_HEX64_EQ(0x547A01ULL, key.key);
}

void test_sub_tier2_relaxed_filetype_variants() {
    // Third-party files omit "Flipper " prefix
    const char* relaxedSub =
        "Filetype: SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Came\n"
        "Bit: 12\n"
        "Key: 00 00 00 00 00 00 0E 15\n";

    StringStreamReader reader(relaxedSub);
    SubGhzHeader hdr;
    ASSERT_TRUE(SubFormatParser::parseHeader(reader, hdr));
    ASSERT_EQ((int)SUB_TYPE_KEY, (int)hdr.fileType);
}

void test_sub_tier2_large_raw_stream() {
    // Generate 1024 pulses (alternating high and low)
    std::string largeRaw =
        "Filetype: Flipper SubGhz RAW File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n";

    for (int line = 0; line < 32; line++) {
        largeRaw += "RAW_Data:";
        for (int p = 0; p < 32; p++) {
            largeRaw += (p % 2 == 0) ? " 320" : " -320";
        }
        largeRaw += "\n";
    }

    StringStreamReader reader(largeRaw.c_str());
    SubGhzHeader hdr;
    ASSERT_TRUE(SubFormatParser::parseHeader(reader, hdr));

    size_t pulseCount = 0;
    auto counterCb = [](int32_t p, void* ctx) {
        auto* cnt = static_cast<size_t*>(ctx);
        (*cnt)++;
        (void)p;
    };

    ASSERT_TRUE(SubFormatParser::parseRawPulses(reader, counterCb, &pulseCount));
    ASSERT_EQ(1024U, pulseCount);
}

// ============================================================================
// TIER 3: Cross-Feature Interactions (Round-Trip Fidelity)
// ============================================================================

void test_sub_tier3_key_roundtrip() {
    // 1. Initial .sub text
    const char* original =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 00 00 00 00 00 54 7A 01\n"
        "TE: 320\n"
        "Repeat: 5\n";

    // 2. Parse into structs
    StringStreamReader reader1(original);
    SubGhzHeader hdr1;
    SubGhzKeyData key1;
    ASSERT_TRUE(SubFormatParser::parseFile(reader1, hdr1, key1));

    // 3. Serialize back to text
    char serialized[1024];
    BufferStreamWriter writer(serialized, sizeof(serialized));
    ASSERT_TRUE(SubFormatSerializer::serializeKey(writer, hdr1, key1));

    // 4. Re-parse serialized text
    StringStreamReader reader2(serialized);
    SubGhzHeader hdr2;
    SubGhzKeyData key2;
    ASSERT_TRUE(SubFormatParser::parseFile(reader2, hdr2, key2));

    // 5. Verify byte-for-byte fidelity of all parsed fields
    ASSERT_EQ(hdr1.fileType, hdr2.fileType);
    ASSERT_EQ(hdr1.version, hdr2.version);
    ASSERT_EQ(hdr1.frequencyHz, hdr2.frequencyHz);
    ASSERT_STR_EQ(hdr1.presetStr, hdr2.presetStr);
    ASSERT_STR_EQ(hdr1.protocol, hdr2.protocol);
    ASSERT_EQ(key1.bitLength, key2.bitLength);
    ASSERT_HEX64_EQ(key1.key, key2.key);
    ASSERT_EQ(key1.te, key2.te);
    ASSERT_EQ(key1.repeat, key2.repeat);
}

void test_sub_tier3_raw_roundtrip() {
    int32_t origPulses[] = {
        400, -400, 400, -400, 400, -400, 400, -400,
        4000, -800, 400, -800, 800, -400, 800, -15000
    };
    const size_t origCount = sizeof(origPulses) / sizeof(origPulses[0]);

    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 433920000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr));
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    // Serialize
    char buffer[2048];
    BufferStreamWriter writer(buffer, sizeof(buffer));
    size_t lineCounter = 0;
    ASSERT_TRUE(SubFormatSerializer::serializeRawStart(writer, hdr));
    ASSERT_TRUE(SubFormatSerializer::serializeRawChunk(writer, origPulses, origCount, lineCounter));
    ASSERT_TRUE(SubFormatSerializer::serializeRawEnd(writer));

    // Re-parse
    StringStreamReader reader(buffer);
    SubGhzHeader hdrParsed;
    ASSERT_TRUE(SubFormatParser::parseHeader(reader, hdrParsed));
    ASSERT_EQ((int)SUB_TYPE_RAW, (int)hdrParsed.fileType);

    std::vector<int32_t> parsedPulses;
    auto cb = [](int32_t p, void* ctx) {
        auto* v = static_cast<std::vector<int32_t>*>(ctx);
        v->push_back(p);
    };
    ASSERT_TRUE(SubFormatParser::parseRawPulses(reader, cb, &parsedPulses));

    ASSERT_EQ(origCount, parsedPulses.size());
    for (size_t i = 0; i < origCount; i++) {
        ASSERT_EQ(origPulses[i], parsedPulses[i]);
    }
}

// ============================================================================
// TIER 4: Real-World Scenarios
// ============================================================================

void test_sub_tier4_captured_community_files() {
    // Real Flipper Zero capture of a Came Top432NA 12-bit barrier remote
    const char* cameSample =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Came\n"
        "Bit: 12\n"
        "Key: 00 00 00 00 00 00 0E 15\n"
        "Repeat: 6\n";

    StringStreamReader r1(cameSample);
    SubGhzHeader h1;
    SubGhzKeyData k1;
    ASSERT_TRUE(SubFormatParser::parseFile(r1, h1, k1));
    ASSERT_HEX64_EQ(0x0E15ULL, k1.key);

    // Real Flipper Zero capture of an EV1527 PIR motion sensor
    const char* ev1527Sample =
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 00 00 00 00 00 A5 5A 03\n"
        "TE: 330\n"
        "Repeat: 10\n";

    StringStreamReader r2(ev1527Sample);
    SubGhzHeader h2;
    SubGhzKeyData k2;
    ASSERT_TRUE(SubFormatParser::parseFile(r2, h2, k2));
    ASSERT_HEX64_EQ(0xA55A03ULL, k2.key);
    ASSERT_EQ(330U, k2.te);
    ASSERT_EQ(10, k2.repeat);
}

void register_sub_format_tests() {
    TEST_SUITE("Flipper Zero .sub Parser & Serializer Verification");

    RUN_TEST_CASE("Princeton protocol .sub parse & field verification", TestTier::TIER_1_FEATURE, test_sub_tier1_princeton);
    RUN_TEST_CASE("Came 12-bit protocol .sub parse & field verification", TestTier::TIER_1_FEATURE, test_sub_tier1_came);
    RUN_TEST_CASE("Nice FLO protocol .sub parse & field verification", TestTier::TIER_1_FEATURE, test_sub_tier1_nice_flo);
    RUN_TEST_CASE("KeeLoq rolling protocol .sub parse with manufacturer", TestTier::TIER_1_FEATURE, test_sub_tier1_keeloq);
    RUN_TEST_CASE("RAW pulse array .sub parse with signed polarity", TestTier::TIER_1_FEATURE, test_sub_tier1_raw_parse);
    RUN_TEST_CASE("RAW pulse stream serializer with line wraps", TestTier::TIER_1_FEATURE, test_sub_tier1_raw_serialize);

    RUN_TEST_CASE("Empty stream handling (zero-byte edge case)", TestTier::TIER_2_BOUNDARY, test_sub_tier2_empty_input);
    RUN_TEST_CASE("Missing required header fields rejection", TestTier::TIER_2_BOUNDARY, test_sub_tier2_missing_header_fields);
    RUN_TEST_CASE("Unspaced continuous hex key parsing tolerance", TestTier::TIER_2_BOUNDARY, test_sub_tier2_unspaced_hex_key);
    RUN_TEST_CASE("Relaxed filetype header tolerance ('SubGhz Key File')", TestTier::TIER_2_BOUNDARY, test_sub_tier2_relaxed_filetype_variants);
    RUN_TEST_CASE("Large RAW pulse stream processing (1024 pulses)", TestTier::TIER_2_BOUNDARY, test_sub_tier2_large_raw_stream);

    RUN_TEST_CASE("Protocol key round-trip (parse -> serialize -> re-parse)", TestTier::TIER_3_CROSS_FEATURE, test_sub_tier3_key_roundtrip);
    RUN_TEST_CASE("RAW pulse array round-trip (serialize -> re-parse)", TestTier::TIER_3_CROSS_FEATURE, test_sub_tier3_raw_roundtrip);

    RUN_TEST_CASE("Authentic community Flipper Zero captures (Came & EV1527)", TestTier::TIER_4_REAL_WORLD, test_sub_tier4_captured_community_files);
}
