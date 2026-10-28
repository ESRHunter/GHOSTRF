#include "sub_format.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

// =========================================================================
// PRESET CONVERSIONS
// =========================================================================

const char* sub_preset_to_string(SubPreset preset) {
    switch (preset) {
        case SUB_PRESET_OOK_650:     return "FuriHalSubGhzPresetOok650Async";
        case SUB_PRESET_OOK_270:     return "FuriHalSubGhzPresetOok270Async";
        case SUB_PRESET_2FSK_DEV238: return "FuriHalSubGhzPreset2FSKDev238Async";
        case SUB_PRESET_2FSK_DEV476: return "FuriHalSubGhzPreset2FSKDev476Async";
        case SUB_PRESET_CUSTOM:      return "FuriHalSubGhzPresetCustom";
        default:                     return "FuriHalSubGhzPresetOok650Async";
    }
}

SubPreset sub_preset_from_string(const char* str) {
    if (!str) return SUB_PRESET_OOK_650;
    if (strstr(str, "Ook270")) return SUB_PRESET_OOK_270;
    if (strstr(str, "Ook650")) return SUB_PRESET_OOK_650;
    if (strstr(str, "Dev238")) return SUB_PRESET_2FSK_DEV238;
    if (strstr(str, "Dev476")) return SUB_PRESET_2FSK_DEV476;
    if (strstr(str, "Custom")) return SUB_PRESET_CUSTOM;
    return SUB_PRESET_OOK_650;
}

// =========================================================================
// STREAM CHAR HELPER WITH SINGLE-BYTE LOOKAHEAD
// =========================================================================

namespace {

class CharStream {
private:
    IByteStreamReader &_reader;
    int _peeked;
public:
    explicit CharStream(IByteStreamReader &r) : _reader(r), _peeked(-1) {}

    bool hasNext() {
        if (_peeked != -1) return true;
        return _reader.hasNext();
    }

    char get() {
        if (_peeked != -1) {
            char c = (char)_peeked;
            _peeked = -1;
            return c;
        }
        return _reader.readChar();
    }

    int peek() {
        if (_peeked != -1) return _peeked;
        if (!_reader.hasNext()) return -1;
        _peeked = (unsigned char)_reader.readChar();
        return _peeked;
    }

    void unget(char c) {
        _peeked = (unsigned char)c;
    }
};

static void trim_whitespace(char* s) {
    if (!s) return;
    // Leading
    char* p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    // Trailing
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
}

static int hex_char_to_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// Read line from stream into buffer, trimming \r\n
static bool stream_read_line(CharStream &stream, char* buf, size_t maxLen) {
    if (maxLen == 0 || !buf) return false;
    buf[0] = '\0';

    while (stream.hasNext()) {
        size_t idx = 0;

        while (stream.hasNext()) {
            char c = stream.get();
            if (c == '\r') continue;
            if (c == '\n') {
                break;
            }
            if (idx + 1 < maxLen) {
                buf[idx++] = c;
            }
        }
        buf[idx] = '\0';

        trim_whitespace(buf);
        if (buf[0] == '#') {
            // Comment line, continue to next line
            continue;
        }
        if (buf[0] == '\0') {
            // Blank line, continue searching if stream still has content
            if (!stream.hasNext()) return false;
            continue;
        }
        return true;
    }
    return false;
}

} // anonymous namespace

// =========================================================================
// PARSER IMPLEMENTATION
// =========================================================================

bool SubFormatParser::parseHeader(IByteStreamReader &reader, SubGhzHeader &outHeader) {
    memset(&outHeader, 0, sizeof(SubGhzHeader));
    outHeader.version = 1;
    outHeader.preset = SUB_PRESET_OOK_650;
    strncpy(outHeader.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(outHeader.presetStr) - 1);

    CharStream stream(reader);
    char line[256];

    bool hasType = false;
    bool hasFreq = false;
    bool hasPreset = false;
    bool hasProto = false;

    while (stream.hasNext()) {
        // Peek ahead: if line starts with Bit: or Key: or RAW_Data:, header is finished!
        int firstChar = stream.peek();
        while (firstChar == ' ' || firstChar == '\t' || firstChar == '\r' || firstChar == '\n') {
            stream.get();
            firstChar = stream.peek();
        }
        if (firstChar == -1) break;

        // If next word is Bit or Key or RAW_Data, do not consume it
        if (firstChar == 'B' || firstChar == 'b' ||
            firstChar == 'K' || firstChar == 'k' ||
            firstChar == 'R' || firstChar == 'r' ||
            firstChar == 'T' || firstChar == 't') {
            // Check if we already have frequency and protocol
            if (hasFreq && (hasProto || hasType)) {
                break;
            }
        }

        if (!stream_read_line(stream, line, sizeof(line))) {
            break;
        }

        char* colon = strchr(line, ':');
        if (!colon) {
            // Check if raw numbers line encountered
            if (isdigit((unsigned char)line[0]) || line[0] == '-' || line[0] == '+') {
                // Raw data encountered, header is done
                outHeader.fileType = SUB_TYPE_RAW;
                if (!hasProto) strncpy(outHeader.protocol, "RAW", sizeof(outHeader.protocol) - 1);
                break;
            }
            continue;
        }

        *colon = '\0';
        char* keyStr = line;
        char* valStr = colon + 1;
        trim_whitespace(keyStr);
        trim_whitespace(valStr);

        if (strcasecmp(keyStr, "Filetype") == 0) {
            hasType = true;
            if (strstr(valStr, "RAW") != nullptr || strstr(valStr, "raw") != nullptr) {
                outHeader.fileType = SUB_TYPE_RAW;
            } else {
                outHeader.fileType = SUB_TYPE_KEY;
            }
        }
        else if (strcasecmp(keyStr, "Version") == 0) {
            outHeader.version = (uint32_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Frequency") == 0) {
            hasFreq = true;
            outHeader.frequencyHz = (uint32_t)strtoul(valStr, nullptr, 10);
            outHeader.frequency = (float)outHeader.frequencyHz / 1000000.0f;
        }
        else if (strcasecmp(keyStr, "Preset") == 0) {
            hasPreset = true;
            strncpy(outHeader.presetStr, valStr, sizeof(outHeader.presetStr) - 1);
            outHeader.preset = sub_preset_from_string(valStr);
        }
        else if (strcasecmp(keyStr, "Protocol") == 0) {
            hasProto = true;
            strncpy(outHeader.protocol, valStr, sizeof(outHeader.protocol) - 1);
            if (strcasecmp(valStr, "RAW") == 0) {
                outHeader.fileType = SUB_TYPE_RAW;
            }
        }
        else if (strcasecmp(keyStr, "Custom_preset_module") == 0) {
            strncpy(outHeader.customModule, valStr, sizeof(outHeader.customModule) - 1);
        }
        else if (strcasecmp(keyStr, "Custom_preset_data") == 0) {
            strncpy(outHeader.customData, valStr, sizeof(outHeader.customData) - 1);
        }
        else if (strcasecmp(keyStr, "Bit") == 0 ||
                 strcasecmp(keyStr, "Key") == 0 ||
                 strcasecmp(keyStr, "TE") == 0 ||
                 strcasecmp(keyStr, "RAW_Data") == 0) {
            // We have read past header into data section
            break;
        }

        // Check if all primary header fields have been read
        if (hasType && hasFreq && hasPreset && hasProto) {
            if (outHeader.preset != SUB_PRESET_CUSTOM || outHeader.customData[0] != '\0') {
                return true;
            }
        }
    }

    return (hasFreq || hasProto || hasType);
}

bool SubFormatParser::parseKeyData(IByteStreamReader &reader, SubGhzKeyData &outKey) {
    memset(&outKey, 0, sizeof(SubGhzKeyData));
    outKey.repeat = 5;

    CharStream stream(reader);
    char line[256];
    bool foundKey = false;
    bool foundBit = false;

    while (stream_read_line(stream, line, sizeof(line))) {
        char* colon = strchr(line, ':');
        if (!colon) continue;

        *colon = '\0';
        char* keyStr = line;
        char* valStr = colon + 1;
        trim_whitespace(keyStr);
        trim_whitespace(valStr);

        if (strcasecmp(keyStr, "Bit") == 0) {
            outKey.bitLength = (uint8_t)strtoul(valStr, nullptr, 10);
            foundBit = true;
        }
        else if (strcasecmp(keyStr, "Key") == 0) {
            uint64_t val = 0;
            const char* p = valStr;
            // Skip optional 0x
            while (*p == ' ' || *p == '\t') p++;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;

            while (*p) {
                if (*p == ' ' || *p == '\t') {
                    p++;
                    continue;
                }
                int h = hex_char_to_val(*p);
                if (h >= 0) {
                    val = (val << 4) | (uint64_t)h;
                }
                p++;
            }
            outKey.key = val;
            foundKey = true;
        }
        else if (strcasecmp(keyStr, "TE") == 0) {
            outKey.te = (uint32_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Repeat") == 0) {
            outKey.repeat = (uint8_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Manufacture") == 0) {
            strncpy(outKey.manufacture, valStr, sizeof(outKey.manufacture) - 1);
        }
    }

    return (foundKey || foundBit);
}

bool SubFormatParser::parseRawPulses(IByteStreamReader &reader, PulseCallback cb, void* ctx) {
    CharStream stream(reader);
    size_t pulseCount = 0;

    enum ParserState {
        STATE_LINE_START,
        STATE_SKIP_LINE,
        STATE_IN_NUMBERS
    };

    ParserState state = STATE_LINE_START;

    while (stream.hasNext()) {
        int peekC = stream.peek();
        if (peekC == -1) break;

        char c = (char)peekC;

        if (c == '\r') {
            stream.get();
            continue;
        }

        if (c == '\n') {
            stream.get();
            state = STATE_LINE_START;
            continue;
        }

        if (c == ' ' || c == '\t') {
            stream.get();
            continue;
        }

        if (c == '#') {
            // Comment
            stream.get();
            state = STATE_SKIP_LINE;
            continue;
        }

        if (state == STATE_SKIP_LINE) {
            stream.get();
            continue;
        }

        if (state == STATE_LINE_START) {
            if (isalpha((unsigned char)c) || c == '_') {
                // Read word
                char word[32];
                size_t widx = 0;
                while (stream.hasNext()) {
                    int p = stream.peek();
                    if (p == -1 || p == ' ' || p == '\t' || p == ':' || p == '\r' || p == '\n') break;
                    if (widx + 1 < sizeof(word)) {
                        word[widx++] = stream.get();
                    } else {
                        stream.get();
                    }
                }
                word[widx] = '\0';

                // Skip optional colon
                if (stream.peek() == ':') stream.get();

                if (strcasecmp(word, "RAW_Data") == 0) {
                    state = STATE_IN_NUMBERS;
                } else {
                    // Header keyword like Frequency or Version
                    state = STATE_SKIP_LINE;
                }
                continue;
            } else if (c == '-' || c == '+' || isdigit((unsigned char)c)) {
                // Continuation line with numbers directly
                state = STATE_IN_NUMBERS;
            } else {
                stream.get();
                continue;
            }
        }

        if (state == STATE_IN_NUMBERS) {
            if (isalpha((unsigned char)c) || c == '_') {
                // Next word, e.g. RAW_Data or another key
                state = STATE_LINE_START;
                continue;
            }

            if (c == '-' || c == '+' || isdigit((unsigned char)c)) {
                // Parse signed number safely (ISO C++ standard saturation, no signed overflow UB)
                char signChar = stream.get();
                int sign = (signChar == '-') ? -1 : 1;
                bool hasDigits = false;
                uint32_t val = 0;
                bool overflow = false;
                const uint32_t maxVal = (sign == -1) ? 2147483648U : 2147483647U;

                if (isdigit((unsigned char)signChar)) {
                    val = (uint32_t)(signChar - '0');
                    hasDigits = true;
                }

                while (stream.hasNext()) {
                    int nextChar = stream.peek();
                    if (nextChar != -1 && isdigit((unsigned char)nextChar)) {
                        hasDigits = true;
                        uint32_t digit = (uint32_t)(stream.get() - '0');
                        if (!overflow) {
                            if (val > (maxVal - digit) / 10U) {
                                val = maxVal;
                                overflow = true;
                            } else {
                                val = val * 10U + digit;
                            }
                        }
                    } else {
                        break;
                    }
                }

                // If no digits were found (e.g. lone trailing '-' or '+'), skip without emitting pulse
                if (!hasDigits) {
                    continue;
                }

                int32_t finalPulse = 0;
                if (sign == -1) {
                    finalPulse = (val >= 2147483648U) ? INT32_MIN : -(int32_t)val;
                } else {
                    finalPulse = (val >= 2147483647U) ? INT32_MAX : (int32_t)val;
                }

                if (cb) {
                    cb(finalPulse, ctx);
                }
                pulseCount++;
                continue;
            }

            stream.get();
        }
    }

    return (pulseCount > 0);
}

bool SubFormatParser::parseFile(IByteStreamReader &reader, SubGhzHeader &outHeader, SubGhzKeyData &outKey, PulseCallback rawCb, void* rawCtx) {
    memset(&outHeader, 0, sizeof(SubGhzHeader));
    memset(&outKey, 0, sizeof(SubGhzKeyData));
    outHeader.version = 1;
    outHeader.preset = SUB_PRESET_OOK_650;
    strncpy(outHeader.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(outHeader.presetStr) - 1);
    outKey.repeat = 5;

    CharStream stream(reader);
    char line[256];
    bool inRaw = false;

    while (stream.hasNext()) {
        if (!stream_read_line(stream, line, sizeof(line))) {
            break;
        }

        if (strncmp(line, "RAW_Data:", 9) == 0 || inRaw) {
            outHeader.fileType = SUB_TYPE_RAW;
            if (outHeader.protocol[0] == '\0') {
                strncpy(outHeader.protocol, "RAW", sizeof(outHeader.protocol) - 1);
            }
            inRaw = true;

            const char* p = line;
            if (strncmp(p, "RAW_Data:", 9) == 0) p += 9;

            while (*p) {
                while (*p == ' ' || *p == '\t' || *p == ',') p++;
                if (!*p) break;
                if (*p == '-' || *p == '+' || isdigit((unsigned char)*p)) {
                    char* endP = nullptr;
                    long val = strtol(p, &endP, 10);
                    if (endP != p) {
                        if (rawCb) rawCb((int32_t)val, rawCtx);
                        p = endP;
                    } else {
                        p++;
                    }
                } else {
                    p++;
                }
            }
            continue;
        }

        char* colon = strchr(line, ':');
        if (!colon) {
            // Check if continuation raw line
            if (isdigit((unsigned char)line[0]) || line[0] == '-' || line[0] == '+') {
                const char* p = line;
                while (*p) {
                    while (*p == ' ' || *p == '\t' || *p == ',') p++;
                    if (!*p) break;
                    if (*p == '-' || *p == '+' || isdigit((unsigned char)*p)) {
                        char* endP = nullptr;
                        long val = strtol(p, &endP, 10);
                        if (endP != p) {
                            if (rawCb) rawCb((int32_t)val, rawCtx);
                            p = endP;
                        } else {
                            p++;
                        }
                    } else {
                        p++;
                    }
                }
            }
            continue;
        }

        *colon = '\0';
        char* keyStr = line;
        char* valStr = colon + 1;
        trim_whitespace(keyStr);
        trim_whitespace(valStr);

        if (strcasecmp(keyStr, "Filetype") == 0) {
            if (strstr(valStr, "RAW") || strstr(valStr, "raw")) {
                outHeader.fileType = SUB_TYPE_RAW;
            } else {
                outHeader.fileType = SUB_TYPE_KEY;
            }
        }
        else if (strcasecmp(keyStr, "Version") == 0) {
            outHeader.version = (uint32_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Frequency") == 0) {
            outHeader.frequencyHz = (uint32_t)strtoul(valStr, nullptr, 10);
            outHeader.frequency = (float)outHeader.frequencyHz / 1000000.0f;
        }
        else if (strcasecmp(keyStr, "Preset") == 0) {
            strncpy(outHeader.presetStr, valStr, sizeof(outHeader.presetStr) - 1);
            outHeader.preset = sub_preset_from_string(valStr);
        }
        else if (strcasecmp(keyStr, "Protocol") == 0) {
            strncpy(outHeader.protocol, valStr, sizeof(outHeader.protocol) - 1);
            if (strcasecmp(valStr, "RAW") == 0) {
                outHeader.fileType = SUB_TYPE_RAW;
            }
        }
        else if (strcasecmp(keyStr, "Custom_preset_module") == 0) {
            strncpy(outHeader.customModule, valStr, sizeof(outHeader.customModule) - 1);
        }
        else if (strcasecmp(keyStr, "Custom_preset_data") == 0) {
            strncpy(outHeader.customData, valStr, sizeof(outHeader.customData) - 1);
        }
        else if (strcasecmp(keyStr, "Bit") == 0) {
            outKey.bitLength = (uint8_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Key") == 0) {
            uint64_t val = 0;
            const char* p = valStr;
            while (*p == ' ' || *p == '\t') p++;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
            while (*p) {
                if (*p == ' ' || *p == '\t') { p++; continue; }
                int h = hex_char_to_val(*p);
                if (h >= 0) val = (val << 4) | (uint64_t)h;
                p++;
            }
            outKey.key = val;
        }
        else if (strcasecmp(keyStr, "TE") == 0) {
            outKey.te = (uint32_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Repeat") == 0) {
            outKey.repeat = (uint8_t)strtoul(valStr, nullptr, 10);
        }
        else if (strcasecmp(keyStr, "Manufacture") == 0) {
            strncpy(outKey.manufacture, valStr, sizeof(outKey.manufacture) - 1);
        }
    }

    return (outHeader.frequencyHz > 0 || outHeader.protocol[0] != '\0');
}

// =========================================================================
// SERIALIZER IMPLEMENTATION
// =========================================================================

bool SubFormatSerializer::serializeHeader(IByteStreamWriter &writer, const SubGhzHeader &hdr) {
    char line[256];
    const char* typeStr = (hdr.fileType == SUB_TYPE_RAW) ? "Flipper SubGhz RAW File" : "Flipper SubGhz Key File";
    snprintf(line, sizeof(line), "Filetype: %s\n", typeStr);
    if (!writer.writeStr(line)) return false;

    uint32_t ver = (hdr.version > 0) ? hdr.version : 1;
    snprintf(line, sizeof(line), "Version: %lu\n", (unsigned long)ver);
    if (!writer.writeStr(line)) return false;

    uint32_t freqHz = hdr.frequencyHz;
    if (freqHz == 0 && hdr.frequency > 0.0f) {
        freqHz = (uint32_t)(hdr.frequency * 1000000.0f + 0.5f);
    }
    if (freqHz == 0) freqHz = 433920000;
    snprintf(line, sizeof(line), "Frequency: %lu\n", (unsigned long)freqHz);
    if (!writer.writeStr(line)) return false;

    const char* preset = (hdr.presetStr[0] != '\0') ? hdr.presetStr : sub_preset_to_string(hdr.preset);
    snprintf(line, sizeof(line), "Preset: %s\n", preset);
    if (!writer.writeStr(line)) return false;

    const char* proto = (hdr.protocol[0] != '\0') ? hdr.protocol : ((hdr.fileType == SUB_TYPE_RAW) ? "RAW" : "Princeton");
    snprintf(line, sizeof(line), "Protocol: %s\n", proto);
    if (!writer.writeStr(line)) return false;

    if (hdr.preset == SUB_PRESET_CUSTOM && hdr.customData[0] != '\0') {
        const char* mod = (hdr.customModule[0] != '\0') ? hdr.customModule : "CC1101";
        snprintf(line, sizeof(line), "Custom_preset_module: %s\n", mod);
        if (!writer.writeStr(line)) return false;
        snprintf(line, sizeof(line), "Custom_preset_data: %s\n", hdr.customData);
        if (!writer.writeStr(line)) return false;
    }

    return true;
}

bool SubFormatSerializer::serializeKey(IByteStreamWriter &writer, const SubGhzHeader &hdr, const SubGhzKeyData &key) {
    SubGhzHeader h = hdr;
    h.fileType = SUB_TYPE_KEY;
    if (!serializeHeader(writer, h)) return false;

    char line[128];
    snprintf(line, sizeof(line), "Bit: %u\n", (unsigned)key.bitLength);
    if (!writer.writeStr(line)) return false;

    // Flipper Zero standard: 8 space-separated uppercase hex bytes
    snprintf(line, sizeof(line), "Key: %02X %02X %02X %02X %02X %02X %02X %02X\n",
             (uint8_t)(key.key >> 56), (uint8_t)(key.key >> 48),
             (uint8_t)(key.key >> 40), (uint8_t)(key.key >> 32),
             (uint8_t)(key.key >> 24), (uint8_t)(key.key >> 16),
             (uint8_t)(key.key >> 8),  (uint8_t)(key.key & 0xFF));
    if (!writer.writeStr(line)) return false;

    if (key.te > 0) {
        snprintf(line, sizeof(line), "TE: %lu\n", (unsigned long)key.te);
        if (!writer.writeStr(line)) return false;
    }

    uint8_t rep = (key.repeat > 0) ? key.repeat : 5;
    snprintf(line, sizeof(line), "Repeat: %u\n", (unsigned)rep);
    if (!writer.writeStr(line)) return false;

    if (key.manufacture[0] != '\0') {
        snprintf(line, sizeof(line), "Manufacture: %s\n", key.manufacture);
        if (!writer.writeStr(line)) return false;
    }

    return true;
}

bool SubFormatSerializer::serializeRawStart(IByteStreamWriter &writer, const SubGhzHeader &hdr) {
    SubGhzHeader h = hdr;
    h.fileType = SUB_TYPE_RAW;
    strncpy(h.protocol, "RAW", sizeof(h.protocol) - 1);
    return serializeHeader(writer, h);
}

bool SubFormatSerializer::serializeRawChunk(IByteStreamWriter &writer, const int32_t* signedPulses, size_t count, size_t &lineCounter) {
    if (!signedPulses || count == 0) return true;
    char numBuf[32];

    for (size_t i = 0; i < count; i++) {
        if (lineCounter == 0) {
            if (!writer.writeStr("RAW_Data:")) return false;
        }
        snprintf(numBuf, sizeof(numBuf), " %ld", (long)signedPulses[i]);
        if (!writer.writeStr(numBuf)) return false;
        lineCounter++;

        // Wrap after 24 values
        if (lineCounter >= 24) {
            if (!writer.writeStr("\n")) return false;
            lineCounter = 0;
        }
    }
    return true;
}

bool SubFormatSerializer::serializeRawEnd(IByteStreamWriter &writer) {
    return writer.writeStr("\n");
}

bool SubFormatSerializer::serializeRawFromUnsigned(IByteStreamWriter &writer, const SubGhzHeader &hdr, const uint16_t* pulses, size_t count) {
    if (!serializeRawStart(writer, hdr)) return false;
    if (!pulses || count == 0) {
        return serializeRawEnd(writer);
    }

    size_t lineCounter = 0;
    int32_t chunk[24];
    size_t chunkIdx = 0;

    for (size_t i = 0; i < count; i++) {
        int32_t p = (int32_t)pulses[i];
        // Alternating signs: even index is positive (HIGH), odd index is negative (LOW)
        chunk[chunkIdx++] = (i % 2 == 0) ? p : -p;
        if (chunkIdx >= 24) {
            if (!serializeRawChunk(writer, chunk, chunkIdx, lineCounter)) return false;
            chunkIdx = 0;
        }
    }
    if (chunkIdx > 0) {
        if (!serializeRawChunk(writer, chunk, chunkIdx, lineCounter)) return false;
    }
    if (lineCounter > 0) {
        if (!writer.writeStr("\n")) return false;
    }
    return true;
}
