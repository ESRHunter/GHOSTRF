// GHOSTRF Challenger R2_2: Chunked Streaming I/O & Memory Safety Stress Harness
// Focus: Massive RAW pulse streaming (up to 32,768 pulses), 256-byte sector chunking,
// zero dynamic heap allocations, and boundary fuzzing.

#include "test_harness.h"
#include "stress_memory_interceptor.h"
#include "sub_format.h"

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <iostream>

using ghostrf_test::TestTier;

namespace {

// Simulated 256-byte sector-buffered reader (mimics LittleFS FileStreamReader)
class SectorStreamReader : public IByteStreamReader {
private:
    const uint8_t* _source;
    size_t         _totalLen;
    size_t         _sourcePos;
    uint8_t        _sectorBuf[256];
    size_t         _bufLen;
    size_t         _bufPos;
    size_t         _sectorReads;
public:
    SectorStreamReader(const uint8_t* data, size_t len)
        : _source(data), _totalLen(len), _sourcePos(0), _bufLen(0), _bufPos(0), _sectorReads(0) {}

    bool hasNext() override {
        if (_bufPos < _bufLen) return true;
        if (_sourcePos >= _totalLen) return false;
        size_t toRead = sizeof(_sectorBuf);
        if (_sourcePos + toRead > _totalLen) {
            toRead = _totalLen - _sourcePos;
        }
        memcpy(_sectorBuf, _source + _sourcePos, toRead);
        _sourcePos += toRead;
        _bufLen = toRead;
        _bufPos = 0;
        _sectorReads++;
        return (_bufLen > 0);
    }

    char readChar() override {
        if (!hasNext()) return '\0';
        return (char)_sectorBuf[_bufPos++];
    }

    size_t getSectorReadCount() const { return _sectorReads; }
};

// Simulated 256-byte sector-buffered writer (mimics LittleFS FileStreamWriter)
class SectorStreamWriter : public IByteStreamWriter {
private:
    uint8_t* _dest;
    size_t   _maxDest;
    size_t   _destPos;
    uint8_t  _sectorBuf[256];
    size_t   _bufPos;
    size_t   _sectorWrites;
public:
    SectorStreamWriter(uint8_t* dest, size_t maxDest)
        : _dest(dest), _maxDest(maxDest), _destPos(0), _bufPos(0), _sectorWrites(0) {}

    ~SectorStreamWriter() override {
        flush();
    }

    void flush() {
        if (_bufPos > 0 && _dest) {
            if (_destPos + _bufPos <= _maxDest) {
                memcpy(_dest + _destPos, _sectorBuf, _bufPos);
                _destPos += _bufPos;
                _sectorWrites++;
            }
            _bufPos = 0;
        }
    }

    bool writeStr(const char* str) override {
        if (!str) return false;
        while (*str) {
            if (!writeChar(*str++)) return false;
        }
        return true;
    }

    bool writeChar(char c) override {
        _sectorBuf[_bufPos++] = (uint8_t)c;
        if (_bufPos >= sizeof(_sectorBuf)) {
            if (_destPos + sizeof(_sectorBuf) > _maxDest) return false;
            memcpy(_dest + _destPos, _sectorBuf, sizeof(_sectorBuf));
            _destPos += sizeof(_sectorBuf);
            _bufPos = 0;
            _sectorWrites++;
        }
        return true;
    }

    size_t getTotalBytesWritten() const { return _destPos + _bufPos; }
    size_t getSectorWriteCount() const { return _sectorWrites; }
};

struct PulseVerifierContext {
    const int32_t* expected;
    size_t         maxExpected;
    size_t         verifiedCount;
    bool           mismatchFound;
    int32_t        firstMismatchExpected;
    int32_t        firstMismatchActual;
};

static void verify_pulse_callback(int32_t pulse, void* ctx) {
    auto* v = static_cast<PulseVerifierContext*>(ctx);
    if (v->verifiedCount < v->maxExpected) {
        if (v->expected[v->verifiedCount] != pulse && !v->mismatchFound) {
            v->mismatchFound = true;
            v->firstMismatchExpected = v->expected[v->verifiedCount];
            v->firstMismatchActual = pulse;
        }
    }
    v->verifiedCount++;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// TEST 1: Massive Stream Scale (2k, 4k, 8k, 16k, 32k) & Zero Heap Allocation
// ----------------------------------------------------------------------------
void test_r2_stress_streaming_scale_zero_heap() {
    const size_t testScales[] = { 2048, 4096, 8192, 16384, 32768 };

    for (size_t count : testScales) {
        // Construct memory buffer
        std::string rawData =
            "Filetype: Flipper SubGhz RAW File\n"
            "Version: 1\n"
            "Frequency: 433920000\n"
            "Preset: FuriHalSubGhzPresetOok650Async\n"
            "Protocol: RAW\n";

        std::vector<int32_t> expected(count);
        for (size_t i = 0; i < count; i++) {
            if (i % 24 == 0) rawData += "RAW_Data:";
            int32_t dur = (int32_t)(120 + ((i * 43) % 2400));
            int32_t sVal = (i % 2 == 0) ? dur : -dur;
            expected[i] = sVal;
            rawData += " " + std::to_string(sVal);
            if ((i + 1) % 24 == 0 || (i + 1) == count) rawData += "\n";
        }

        PulseVerifierContext verifier = { expected.data(), count, 0, false, 0, 0 };
        StringStreamReader reader(rawData.c_str(), rawData.length());
        SubGhzHeader hdr;

        // Strict heap monitor during entire parse
        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool okHdr = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(okHdr);
        ASSERT_EQ((int)SUB_TYPE_RAW, (int)hdr.fileType);
        ASSERT_EQ(433920000U, hdr.frequencyHz);

        bool okPulses = SubFormatParser::parseRawPulses(reader, verify_pulse_callback, &verifier);
        ASSERT_TRUE(okPulses);
        ASSERT_EQ(count, verifier.verifiedCount);
        ASSERT_FALSE(verifier.mismatchFound);

        size_t deltaAlloc = heapMon.getDeltaAllocations();
        size_t deltaBytes = heapMon.getDeltaBytes();

        // Must allocate ZERO bytes on dynamic heap
        ASSERT_EQ(0U, deltaAlloc);
        ASSERT_EQ(0U, deltaBytes);
    }
}

// ----------------------------------------------------------------------------
// TEST 2: 100-Cycle 256-Byte Sector Chunked I/O Endurance (204,800 Pulses)
// ----------------------------------------------------------------------------
void test_r2_stress_sector_endurance_100_cycles() {
    const size_t pulsesPerCycle = 2048;
    const size_t cycles = 100;
    std::vector<int32_t> pulses(pulsesPerCycle);
    for (size_t i = 0; i < pulsesPerCycle; i++) {
        int32_t p = (int32_t)(250 + ((i * 31) % 1800));
        pulses[i] = (i % 2 == 0) ? p : -p;
    }

    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 433920000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr));
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    static uint8_t sectorDisk[65536];
    size_t totalStreamedPulses = 0;
    size_t totalSectorWrites = 0;
    size_t totalSectorReads = 0;

    ghostrf_stress::ScopedHeapMonitor heapMon;

    for (size_t c = 0; c < cycles; c++) {
        // Serialize via 256-byte sector buffer
        SectorStreamWriter secWriter(sectorDisk, sizeof(sectorDisk));
        bool sStart = SubFormatSerializer::serializeRawStart(secWriter, hdr);
        ASSERT_TRUE(sStart);

        size_t lineCounter = 0;
        for (size_t startIdx = 0; startIdx < pulsesPerCycle; startIdx += 32) {
            size_t chunkSize = (startIdx + 32 > pulsesPerCycle) ? (pulsesPerCycle - startIdx) : 32;
            bool sChunk = SubFormatSerializer::serializeRawChunk(secWriter, pulses.data() + startIdx, chunkSize, lineCounter);
            ASSERT_TRUE(sChunk);
        }
        bool sEnd = SubFormatSerializer::serializeRawEnd(secWriter);
        ASSERT_TRUE(sEnd);
        secWriter.flush();

        totalSectorWrites += secWriter.getSectorWriteCount();
        size_t writtenBytes = secWriter.getTotalBytesWritten();
        ASSERT_TRUE(writtenBytes > 256);

        // Parse via 256-byte sector buffer
        SectorStreamReader secReader(sectorDisk, writtenBytes);
        SubGhzHeader readHdr;
        bool okHdr = SubFormatParser::parseHeader(secReader, readHdr);
        ASSERT_TRUE(okHdr);

        size_t cycleReadCount = 0;
        bool okPulses = SubFormatParser::parseRawPulses(secReader, [](int32_t, void* ctx) {
            (*static_cast<size_t*>(ctx))++;
        }, &cycleReadCount);

        ASSERT_TRUE(okPulses);
        ASSERT_EQ(pulsesPerCycle, cycleReadCount);
        totalStreamedPulses += cycleReadCount;
        totalSectorReads += secReader.getSectorReadCount();
    }

    size_t deltaAlloc = heapMon.getDeltaAllocations();
    size_t deltaBytes = heapMon.getDeltaBytes();

    ASSERT_EQ(0U, deltaAlloc);
    ASSERT_EQ(0U, deltaBytes);
    ASSERT_EQ(pulsesPerCycle * cycles, totalStreamedPulses);
    ASSERT_TRUE(totalSectorWrites >= cycles * 35);
    ASSERT_TRUE(totalSectorReads >= cycles * 35);
}

// ----------------------------------------------------------------------------
// TEST 3: Irregular Chunk Sizes & Line Wrap Stress (1 to 128 Pulses per Chunk)
// ----------------------------------------------------------------------------
void test_r2_stress_irregular_chunks_and_line_wraps() {
    const size_t totalPulses = 1000;
    std::vector<int32_t> pulses(totalPulses);
    for (size_t i = 0; i < totalPulses; i++) {
        int32_t p = (int32_t)(350 + (i % 500));
        pulses[i] = (i % 2 == 0) ? p : -p;
    }

    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 868350000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPreset2FSKDev476Async", sizeof(hdr.presetStr) - 1);
    hdr.presetStr[sizeof(hdr.presetStr) - 1] = '\0';
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    const size_t chunkSizes[] = { 1, 3, 7, 15, 23, 24, 25, 48, 64, 128 };

    for (size_t cSize : chunkSizes) {
        static char outBuf[32768];
        BufferStreamWriter writer(outBuf, sizeof(outBuf));

        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool s1 = SubFormatSerializer::serializeRawStart(writer, hdr);
        ASSERT_TRUE(s1);

        size_t lineCounter = 0;
        for (size_t i = 0; i < totalPulses; i += cSize) {
            size_t n = (i + cSize > totalPulses) ? (totalPulses - i) : cSize;
            bool sChunk = SubFormatSerializer::serializeRawChunk(writer, pulses.data() + i, n, lineCounter);
            ASSERT_TRUE(sChunk);
        }
        bool sEnd = SubFormatSerializer::serializeRawEnd(writer);
        ASSERT_TRUE(sEnd);

        ASSERT_EQ(0U, heapMon.getDeltaAllocations());

        // Re-parse and verify exact counts and values
        StringStreamReader reader(writer.c_str(), writer.getLength());
        SubGhzHeader rHdr;
        ASSERT_TRUE(SubFormatParser::parseHeader(reader, rHdr));
        ASSERT_EQ(868350000U, rHdr.frequencyHz);

        PulseVerifierContext verifier = { pulses.data(), totalPulses, 0, false, 0, 0 };
        bool ok = SubFormatParser::parseRawPulses(reader, verify_pulse_callback, &verifier);
        ASSERT_TRUE(ok);
        ASSERT_EQ(totalPulses, verifier.verifiedCount);
        ASSERT_FALSE(verifier.mismatchFound);
    }
}

// ----------------------------------------------------------------------------
// TEST 4: Extreme Numerical Boundaries, Lone Signs & 10,000-Pulse Single Lines
// ----------------------------------------------------------------------------
void test_r2_stress_pathological_stream_inputs() {
    // 1. Extreme signed numbers, overflow saturation, lone signs, comments
    std::string streamStr =
        "Filetype: Flipper SubGhz RAW File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: RAW\n"
        "# Comment at start\n"
        "RAW_Data: 2147483647 -2147483648 0 1 -1\n"
        "# Overflow test tokens:\n"
        "RAW_Data: 99999999999999999999 -99999999999999999999\n"
        "# Lone signs discarded without emitting 0-duration pulses:\n"
        "RAW_Data: + - 150 -250\n"
        "RAW_Data: - 400 + 500 -\n"
        "# Multiple spaces and tabs:\n"
        "RAW_Data:        600        -700       \n";

    {
        int32_t parsed[32];
        size_t count = 0;
        struct ParseCtx {
            int32_t* arr;
            size_t* cnt;
        } ctx = { parsed, &count };

        StringStreamReader reader(streamStr.c_str(), streamStr.length());

        ghostrf_stress::ScopedHeapMonitor heapMon;
        bool ok = SubFormatParser::parseRawPulses(reader, [](int32_t p, void* c) {
            auto* pc = static_cast<ParseCtx*>(c);
            if (*pc->cnt < 32) {
                pc->arr[(*pc->cnt)++] = p;
            }
        }, &ctx);

        ASSERT_TRUE(ok);
        ASSERT_EQ(0U, heapMon.getDeltaAllocations());
        ASSERT_EQ(0U, heapMon.getDeltaBytes());

        ASSERT_EQ(13U, count);
        ASSERT_EQ(2147483647, parsed[0]);
        ASSERT_EQ(INT32_MIN, parsed[1]);
        ASSERT_EQ(0, parsed[2]);
        ASSERT_EQ(1, parsed[3]);
        ASSERT_EQ(-1, parsed[4]);
        ASSERT_EQ(2147483647, parsed[5]); // Clamped
        ASSERT_EQ(INT32_MIN, parsed[6]);  // Clamped
        ASSERT_EQ(150, parsed[7]);
        ASSERT_EQ(-250, parsed[8]);
        ASSERT_EQ(400, parsed[9]);
        ASSERT_EQ(500, parsed[10]);
        ASSERT_EQ(600, parsed[11]);
        ASSERT_EQ(-700, parsed[12]);
    }

    // 2. Single line with 5,000 pulses without any newline (stressing token loop)
    std::string hugeLine =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Protocol: RAW\n"
        "RAW_Data:";
    for (int i = 0; i < 5000; i++) {
        hugeLine += (i % 2 == 0) ? " 300" : " -300";
    }
    hugeLine += "\n";

    {
        size_t count = 0;
        StringStreamReader reader(hugeLine.c_str(), hugeLine.length());
        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool ok = SubFormatParser::parseRawPulses(reader, [](int32_t, void* ctx) {
            (*static_cast<size_t*>(ctx))++;
        }, &count);

        ASSERT_TRUE(ok);
        ASSERT_EQ(5000U, count);
        ASSERT_EQ(0U, heapMon.getDeltaAllocations());
    }

    // 3. Degenerate and empty streams
    {
        StringStreamReader emptyReader("");
        SubGhzHeader emptyHdr;
        bool ok = SubFormatParser::parseHeader(emptyReader, emptyHdr);
        ASSERT_FALSE(ok);
    }
    {
        StringStreamReader commentsOnly("# Just a comment\n# Another comment\n");
        SubGhzHeader h;
        bool ok = SubFormatParser::parseHeader(commentsOnly, h);
        ASSERT_FALSE(ok);
    }
}

// ----------------------------------------------------------------------------
// TEST 5: Protocol Key File Streaming Zero-Heap Allocation
// ----------------------------------------------------------------------------
void test_r2_stress_protocol_key_zero_heap() {
    const char* keyFiles[] = {
        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: Princeton\n"
        "Bit: 24\n"
        "Key: 00 00 00 00 00 54 7A 01\n"
        "TE: 390\n"
        "Repeat: 5\n",

        "Filetype: Flipper SubGhz Key File\n"
        "Version: 1\n"
        "Frequency: 433920000\n"
        "Preset: FuriHalSubGhzPresetOok650Async\n"
        "Protocol: KeeLoq\n"
        "Bit: 64\n"
        "Key: 2B 7E 15 16 28 AE D2 A6\n"
        "Repeat: 3\n"
        "Manufacture: DoorHan\n"
    };

    for (const char* kf : keyFiles) {
        StringStreamReader reader(kf);
        SubGhzHeader hdr;
        SubGhzKeyData key;

        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool okHdr = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(okHdr);
        ASSERT_EQ((int)SUB_TYPE_KEY, (int)hdr.fileType);

        bool okKey = SubFormatParser::parseKeyData(reader, key);
        ASSERT_TRUE(okKey);

        char outBuf[512];
        BufferStreamWriter writer(outBuf, sizeof(outBuf));
        bool okSer = SubFormatSerializer::serializeKey(writer, hdr, key);
        ASSERT_TRUE(okSer);

        size_t deltaAlloc = heapMon.getDeltaAllocations();
        size_t deltaBytes = heapMon.getDeltaBytes();

        ASSERT_EQ(0U, deltaAlloc);
        ASSERT_EQ(0U, deltaBytes);
    }
}

void register_r2_stress_streaming_tests() {
    TEST_SUITE("R2 Stress Suite 1: Chunked Streaming I/O & Memory Safety (Zero Heap)");
    RUN_TEST_CASE("[R2-STRESS] Massive scale RAW streaming (2k, 4k, 8k, 16k, 32k pulses) with 0B heap", TestTier::TIER_2_BOUNDARY, test_r2_stress_streaming_scale_zero_heap);
    RUN_TEST_CASE("[R2-STRESS] 100-cycle 256-byte sector chunked endurance (204,800 pulses)", TestTier::TIER_4_REAL_WORLD, test_r2_stress_sector_endurance_100_cycles);
    RUN_TEST_CASE("[R2-STRESS] Irregular chunk sizes (1..128) & 24-pulse line wraps", TestTier::TIER_2_BOUNDARY, test_r2_stress_irregular_chunks_and_line_wraps);
    RUN_TEST_CASE("[R2-STRESS] Extreme numerical boundaries (INT32_MIN/MAX), lone signs, 5k single lines", TestTier::TIER_2_BOUNDARY, test_r2_stress_pathological_stream_inputs);
    RUN_TEST_CASE("[R2-STRESS] Protocol key file parse/serialize with 0B dynamic heap", TestTier::TIER_2_BOUNDARY, test_r2_stress_protocol_key_zero_heap);
}
