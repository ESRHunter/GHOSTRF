// Stress Test Suite 1: Chunked Streaming I/O Memory Safety & Endurance
// Evaluates: 2,048+ RAW pulses, zero-heap allocation, 256-byte sector buffer chunking

#include "test_harness.h"
#include "stress_memory_interceptor.h"
#include "sub_format.h"

#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <random>

using ghostrf_test::TestTier;

namespace {

// Simulated 256-byte sector-buffered reader (mimics LittleFS FileStreamReader)
class SimulatedSectorReader : public IByteStreamReader {
private:
    const uint8_t* _source;
    size_t         _totalLen;
    size_t         _sourcePos;
    uint8_t        _sectorBuf[256];
    size_t         _bufLen;
    size_t         _bufPos;
    size_t         _sectorReads;
public:
    SimulatedSectorReader(const uint8_t* data, size_t len)
        : _source(data), _totalLen(len), _sourcePos(0), _bufLen(0), _bufPos(0), _sectorReads(0) {}

    bool hasNext() override {
        if (_bufPos < _bufLen) return true;
        if (_sourcePos >= _totalLen) return false;
        // Refill from next 256-byte sector
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
class SimulatedSectorWriter : public IByteStreamWriter {
private:
    uint8_t* _dest;
    size_t   _maxDest;
    size_t   _destPos;
    uint8_t  _sectorBuf[256];
    size_t   _bufPos;
    size_t   _sectorWrites;
public:
    SimulatedSectorWriter(uint8_t* dest, size_t maxDest)
        : _dest(dest), _maxDest(maxDest), _destPos(0), _bufPos(0), _sectorWrites(0) {}

    ~SimulatedSectorWriter() override {
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

} // anonymous namespace

// ----------------------------------------------------------------------------
// TEST 1: Continuous Parsing of 2,048, 4,096, and 8,192 RAW Pulses (0 Heap Alloc)
// ----------------------------------------------------------------------------
void test_stress_streaming_parse_zero_alloc() {
    const size_t pulseCounts[] = { 2048, 4096, 8192 };

    for (size_t count : pulseCounts) {
        // 1. Generate synthetic RAW file in memory
        std::string rawData =
            "Filetype: Flipper SubGhz RAW File\n"
            "Version: 1\n"
            "Frequency: 433920000\n"
            "Preset: FuriHalSubGhzPresetOok650Async\n"
            "Protocol: RAW\n";

        std::vector<int32_t> expectedPulses;
        expectedPulses.reserve(count);

        for (size_t i = 0; i < count; i++) {
            if (i % 24 == 0) rawData += "RAW_Data:";
            // Alternating sign pulse, duration varying between 150us and 2500us
            int32_t dur = (int32_t)(150 + ((i * 37) % 2350));
            int32_t signedVal = (i % 2 == 0) ? dur : -dur;
            expectedPulses.push_back(signedVal);
            rawData += " " + std::to_string(signedVal);
            if ((i + 1) % 24 == 0 || i + 1 == count) rawData += "\n";
        }

        // 2. Prepare static verification state (no heap allocations during check)
        struct PulseVerifier {
            const int32_t* expected;
            size_t maxExpected;
            size_t verifiedCount;
            bool mismatchFound;
        };

        PulseVerifier verifier = { expectedPulses.data(), count, 0, false };

        StringStreamReader reader(rawData.c_str(), rawData.length());
        SubGhzHeader hdr;

        // 3. ACTIVATE HEAP MONITOR - Strict 0-byte dynamic heap requirement
        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool headerOk = SubFormatParser::parseHeader(reader, hdr);
        ASSERT_TRUE(headerOk);
        ASSERT_EQ((int)SUB_TYPE_RAW, (int)hdr.fileType);
        ASSERT_EQ(433920000U, hdr.frequencyHz);

        auto pulseCallback = [](int32_t pulse, void* ctx) {
            auto* v = static_cast<PulseVerifier*>(ctx);
            if (v->verifiedCount < v->maxExpected) {
                if (v->expected[v->verifiedCount] != pulse) {
                    v->mismatchFound = true;
                }
            }
            v->verifiedCount++;
        };

        bool rawOk = SubFormatParser::parseRawPulses(reader, pulseCallback, &verifier);

        // 4. VERIFY 0 BYTES ALLOCATED ON DYNAMIC HEAP
        size_t deltaAlloc = heapMon.getDeltaAllocations();
        size_t deltaBytes = heapMon.getDeltaBytes();

        ASSERT_TRUE(rawOk);
        ASSERT_EQ(count, verifier.verifiedCount);
        ASSERT_FALSE(verifier.mismatchFound);
        ASSERT_EQ(0U, deltaAlloc);
        ASSERT_EQ(0U, deltaBytes);
    }
}

// ----------------------------------------------------------------------------
// TEST 2: Chunked Serialization of 2,048+ Pulses (0 Heap Alloc)
// ----------------------------------------------------------------------------
void test_stress_streaming_serialize_zero_alloc() {
    const size_t count = 2048;
    std::vector<int32_t> pulses(count);
    for (size_t i = 0; i < count; i++) {
        int32_t p = (int32_t)(200 + (i % 800));
        pulses[i] = (i % 2 == 0) ? p : -p;
    }

    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 433920000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr));
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    // Allocate output buffer statically
    static char outBuf[65536];
    BufferStreamWriter writer(outBuf, sizeof(outBuf));

    {
        // Strict heap monitor during serialization
        ghostrf_stress::ScopedHeapMonitor heapMon;

        bool s1 = SubFormatSerializer::serializeRawStart(writer, hdr);
        ASSERT_TRUE(s1);

        size_t lineCounter = 0;
        // Feed in 64-pulse chunks
        for (size_t chunkStart = 0; chunkStart < count; chunkStart += 64) {
            size_t chunkSize = 64;
            if (chunkStart + chunkSize > count) chunkSize = count - chunkStart;
            bool sChunk = SubFormatSerializer::serializeRawChunk(writer, pulses.data() + chunkStart, chunkSize, lineCounter);
            ASSERT_TRUE(sChunk);
        }

        bool sEnd = SubFormatSerializer::serializeRawEnd(writer);
        ASSERT_TRUE(sEnd);

        size_t deltaAlloc = heapMon.getDeltaAllocations();
        size_t deltaBytes = heapMon.getDeltaBytes();
        ASSERT_EQ(0U, deltaAlloc);
        ASSERT_EQ(0U, deltaBytes);
    }

    // Verify written content is valid and re-parseable
    StringStreamReader reader(writer.c_str(), writer.getLength());
    SubGhzHeader reHdr;
    ASSERT_TRUE(SubFormatParser::parseHeader(reader, reHdr));
    ASSERT_EQ((int)SUB_TYPE_RAW, (int)reHdr.fileType);

    size_t reCount = 0;
    bool reOk = SubFormatParser::parseRawPulses(reader, [](int32_t, void* ctx) {
        (*static_cast<size_t*>(ctx))++;
    }, &reCount);
    ASSERT_TRUE(reOk);
    ASSERT_EQ(count, reCount);
}

// ----------------------------------------------------------------------------
// TEST 3: 256-Byte Sector Chunked I/O Endurance (50 cycles, 102,400 pulses)
// ----------------------------------------------------------------------------
void test_stress_sector_buffered_endurance_50_cycles() {
    const size_t pulsesPerCycle = 2048;
    const size_t cycles = 50;
    std::vector<int32_t> originalPulses(pulsesPerCycle);
    for (size_t i = 0; i < pulsesPerCycle; i++) {
        int32_t p = (int32_t)(300 + ((i * 19) % 1500));
        originalPulses[i] = (i % 2 == 0) ? p : -p;
    }

    SubGhzHeader hdr = {};
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequencyHz = 433920000;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr));
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol));

    static uint8_t sectorDiskFile[65536];

    size_t totalPulsesStreamed = 0;
    size_t totalSectorWrites = 0;
    size_t totalSectorReads = 0;

    // Strict heap monitor during entire 50-cycle loop
    ghostrf_stress::ScopedHeapMonitor heapMon;

    for (size_t c = 0; c < cycles; c++) {
        // 1. Serialize through 256-byte sector buffer writer
        SimulatedSectorWriter secWriter(sectorDiskFile, sizeof(sectorDiskFile));
        bool sStart = SubFormatSerializer::serializeRawStart(secWriter, hdr);
        ASSERT_TRUE(sStart);

        size_t lineCounter = 0;
        for (size_t chunkStart = 0; chunkStart < pulsesPerCycle; chunkStart += 32) {
            size_t chunkSize = 32;
            if (chunkStart + chunkSize > pulsesPerCycle) chunkSize = pulsesPerCycle - chunkStart;
            bool sChunk = SubFormatSerializer::serializeRawChunk(secWriter, originalPulses.data() + chunkStart, chunkSize, lineCounter);
            ASSERT_TRUE(sChunk);
        }
        bool sEnd = SubFormatSerializer::serializeRawEnd(secWriter);
        ASSERT_TRUE(sEnd);
        secWriter.flush();

        totalSectorWrites += secWriter.getSectorWriteCount();
        size_t fileSize = secWriter.getTotalBytesWritten();
        ASSERT_TRUE(fileSize > 256); // Must have required multiple 256-byte sector flushes

        // 2. Read through 256-byte sector buffer reader
        SimulatedSectorReader secReader(sectorDiskFile, fileSize);
        SubGhzHeader readHdr;
        bool phOk = SubFormatParser::parseHeader(secReader, readHdr);
        ASSERT_TRUE(phOk);

        size_t cyclePulsesRead = 0;
        bool pOk = SubFormatParser::parseRawPulses(secReader, [](int32_t p, void* ctx) {
            auto* cnt = static_cast<size_t*>(ctx);
            (*cnt)++;
            (void)p;
        }, &cyclePulsesRead);

        ASSERT_TRUE(pOk);
        ASSERT_EQ(pulsesPerCycle, cyclePulsesRead);
        totalPulsesStreamed += cyclePulsesRead;
        totalSectorReads += secReader.getSectorReadCount();
    }

    // Verify 0 bytes dynamic heap allocation across all 50 cycles
    size_t deltaAlloc = heapMon.getDeltaAllocations();
    size_t deltaBytes = heapMon.getDeltaBytes();

    ASSERT_EQ(0U, deltaAlloc);
    ASSERT_EQ(0U, deltaBytes);
    ASSERT_EQ(pulsesPerCycle * cycles, totalPulsesStreamed);
    // 2,048 pulses serialize to ~10.2 KB (~40 256-byte sectors per cycle)
    ASSERT_TRUE(totalSectorWrites >= cycles * 35);
    ASSERT_TRUE(totalSectorReads >= cycles * 35);
}

// ----------------------------------------------------------------------------
// TEST 4: Massive Stream Extreme Values & Line Wrapping Variations
// ----------------------------------------------------------------------------
void test_stress_extreme_values_and_irregular_formatting() {
    // Construct stream with edge values: INT32_MAX, -INT32_MAX, 0, 1, -1, 65535, -65535
    std::string streamStr =
        "Filetype: Flipper SubGhz RAW File\n"
        "Frequency: 433920000\n"
        "Protocol: RAW\n"
        "RAW_Data: 2147483647 -2147483647 0 1 -1 65535 -65535\n"
        "# Comment line inside raw data\n"
        "RAW_Data:    100    -200    \n"
        "RAW_Data: 300\n"
        "RAW_Data: -400\n"
        "500 -600 700 -800\n"; // Continuation line without "RAW_Data:" prefix

    StringStreamReader reader(streamStr.c_str(), streamStr.length());

    std::vector<int32_t> parsed;
    ghostrf_stress::ScopedHeapMonitor heapMon;

    bool ok = SubFormatParser::parseRawPulses(reader, [](int32_t p, void* ctx) {
        static_cast<std::vector<int32_t>*>(ctx)->push_back(p);
    }, &parsed);

    ASSERT_TRUE(ok);
    ASSERT_EQ(15U, parsed.size());
    ASSERT_EQ(2147483647, parsed[0]);
    ASSERT_EQ(-2147483647, parsed[1]);
    ASSERT_EQ(0, parsed[2]);
    ASSERT_EQ(1, parsed[3]);
    ASSERT_EQ(-1, parsed[4]);
    ASSERT_EQ(65535, parsed[5]);
    ASSERT_EQ(-65535, parsed[6]);
    ASSERT_EQ(100, parsed[7]);
    ASSERT_EQ(-200, parsed[8]);
    ASSERT_EQ(300, parsed[9]);
    ASSERT_EQ(-400, parsed[10]);
    ASSERT_EQ(500, parsed[11]);
    ASSERT_EQ(-600, parsed[12]);
    ASSERT_EQ(700, parsed[13]);
    ASSERT_EQ(-800, parsed[14]);
}

void register_stress_streaming_tests() {
    TEST_SUITE("Stress Test 1: Chunked Streaming I/O & Memory Safety (2,048+ Pulses)");
    RUN_TEST_CASE("[STRESS] Parse 2,048 / 4,096 / 8,192 RAW pulses with 0B heap allocation", TestTier::TIER_2_BOUNDARY, test_stress_streaming_parse_zero_alloc);
    RUN_TEST_CASE("[STRESS] Chunked serialize 2,048 RAW pulses with 0B heap allocation", TestTier::TIER_2_BOUNDARY, test_stress_streaming_serialize_zero_alloc);
    RUN_TEST_CASE("[STRESS] 256-byte sector buffer I/O endurance (50 cycles, 102,400 pulses)", TestTier::TIER_4_REAL_WORLD, test_stress_sector_buffered_endurance_50_cycles);
    RUN_TEST_CASE("[STRESS] Extreme signed numbers & irregular line wrapping tolerance", TestTier::TIER_2_BOUNDARY, test_stress_extreme_values_and_irregular_formatting);
}
