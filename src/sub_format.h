#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// =========================================================================
// FLIPPER ZERO .SUB FILE TYPES & PRESETS
// =========================================================================

enum SubFileType {
    SUB_TYPE_KEY = 0,
    SUB_TYPE_RAW = 1
};

enum SubPreset {
    SUB_PRESET_OOK_650 = 0,
    SUB_PRESET_OOK_270,
    SUB_PRESET_2FSK_DEV238,
    SUB_PRESET_2FSK_DEV476,
    SUB_PRESET_CUSTOM
};

struct SubGhzHeader {
    SubFileType fileType;
    uint32_t    version;
    float       frequency;       // In MHz (e.g. 433.92)
    uint32_t    frequencyHz;     // In Hz (e.g. 433920000)
    SubPreset   preset;
    char        presetStr[32];
    char        protocol[32];
    char        customModule[16];
    char        customData[128];
};

struct SubGhzKeyData {
    uint8_t  bitLength;
    uint64_t key;
    uint32_t te;
    uint8_t  repeat;
    char     manufacture[32];
};

struct SignalMeta {
    char        filename[64];
    char        name[32];
    SubFileType fileType;
    char        protocol[32];
    float       frequency;
    uint8_t     repeats;
    uint8_t     bitLength;
    uint32_t    code;
    uint64_t    fullCode;
    uint16_t    pulseLength;
    uint32_t    fileSize;
    uint16_t    rawCount;
};

// =========================================================================
// ZERO-ALLOCATION STREAMING INTERFACES
// =========================================================================

class IByteStreamReader {
public:
    virtual ~IByteStreamReader() {}
    virtual bool hasNext() = 0;
    virtual char readChar() = 0;
};

class IByteStreamWriter {
public:
    virtual ~IByteStreamWriter() {}
    virtual bool writeStr(const char* str) = 0;
    virtual bool writeChar(char c) = 0;
};

// In-memory String Reader (host tests & fast parsing)
class StringStreamReader : public IByteStreamReader {
private:
    const char* _buf;
    size_t      _len;
    size_t      _pos;
public:
    StringStreamReader(const char* buf, size_t len = 0) : _buf(buf), _pos(0) {
        _len = len ? len : (buf ? strlen(buf) : 0);
    }
    bool hasNext() override { return _pos < _len; }
    char readChar() override { return (_pos < _len) ? _buf[_pos++] : '\0'; }
    void reset() { _pos = 0; }
};

// In-memory Buffer Writer (host tests & bounded scratchpads)
class BufferStreamWriter : public IByteStreamWriter {
private:
    char*  _buf;
    size_t _maxLen;
    size_t _pos;
public:
    BufferStreamWriter(char* buf, size_t maxLen) : _buf(buf), _maxLen(maxLen), _pos(0) {
        if (_buf && _maxLen > 0) _buf[0] = '\0';
    }
    bool writeStr(const char* str) override {
        if (!str || !_buf) return true;
        size_t slen = strlen(str);
        if (_pos + slen >= _maxLen) return false;
        memcpy(_buf + _pos, str, slen);
        _pos += slen;
        _buf[_pos] = '\0';
        return true;
    }
    bool writeChar(char c) override {
        if (!_buf || _pos + 1 >= _maxLen) return false;
        _buf[_pos++] = c;
        _buf[_pos] = '\0';
        return true;
    }
    size_t getLength() const { return _pos; }
    const char* c_str() const { return _buf ? _buf : ""; }
    void reset() {
        _pos = 0;
        if (_buf && _maxLen > 0) _buf[0] = '\0';
    }
};

// =========================================================================
// HELPER FUNCTIONS & FORMAT CONVERTERS
// =========================================================================

const char* sub_preset_to_string(SubPreset preset);
SubPreset   sub_preset_from_string(const char* str);

// =========================================================================
// PURE C++ .SUB PARSER & SERIALIZER
// =========================================================================

class SubFormatParser {
public:
    static bool parseHeader(IByteStreamReader &reader, SubGhzHeader &outHeader);
    static bool parseKeyData(IByteStreamReader &reader, SubGhzKeyData &outKey);
    typedef void (*PulseCallback)(int32_t signedPulseUs, void* ctx);
    static bool parseRawPulses(IByteStreamReader &reader, PulseCallback cb, void* ctx);
    static bool parseFile(IByteStreamReader &reader, SubGhzHeader &outHeader, SubGhzKeyData &outKey, PulseCallback rawCb = nullptr, void* rawCtx = nullptr);
};

class SubFormatSerializer {
public:
    static bool serializeHeader(IByteStreamWriter &writer, const SubGhzHeader &hdr);
    static bool serializeKey(IByteStreamWriter &writer, const SubGhzHeader &hdr, const SubGhzKeyData &key);
    static bool serializeRawStart(IByteStreamWriter &writer, const SubGhzHeader &hdr);
    static bool serializeRawChunk(IByteStreamWriter &writer, const int32_t* signedPulses, size_t count, size_t &lineCounter);
    static bool serializeRawEnd(IByteStreamWriter &writer);
    static bool serializeRawFromUnsigned(IByteStreamWriter &writer, const SubGhzHeader &hdr, const uint16_t* pulses, size_t count);
};
