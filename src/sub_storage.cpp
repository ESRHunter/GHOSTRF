#include "sub_storage.h"
#include "radio.h"
#include "keeloq.h"
#include "storage.h"
#include "config.h"
#include <Preferences.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// =========================================================================
// ZERO-ALLOCATION 256-BYTE SECTOR STREAM ADAPTERS FOR LITTLEFS
// =========================================================================

namespace {

class FileStreamReader : public IByteStreamReader {
private:
    fs::File _file;
    uint8_t  _buf[256];
    size_t   _bufLen;
    size_t   _bufPos;
public:
    explicit FileStreamReader(fs::File f) : _file(f), _bufLen(0), _bufPos(0) {}

    bool hasNext() override {
        if (_bufPos < _bufLen) return true;
        if (!_file || !_file.available()) return false;
        _bufLen = _file.read(_buf, sizeof(_buf));
        _bufPos = 0;
        return (_bufLen > 0);
    }

    char readChar() override {
        if (!hasNext()) return '\0';
        return (char)_buf[_bufPos++];
    }
};

class FileStreamWriter : public IByteStreamWriter {
private:
    fs::File _file;
    uint8_t  _buf[256];
    size_t   _bufPos;
public:
    explicit FileStreamWriter(fs::File f) : _file(f), _bufPos(0) {}
    ~FileStreamWriter() override {
        flush();
    }

    void flush() {
        if (_file && _bufPos > 0) {
            _file.write(_buf, _bufPos);
            _bufPos = 0;
            _file.flush();
        }
    }

    bool writeStr(const char* str) override {
        if (!str || !_file) return false;
        while (*str) {
            if (!writeChar(*str++)) return false;
        }
        return true;
    }

    bool writeChar(char c) override {
        if (!_file) return false;
        _buf[_bufPos++] = (uint8_t)c;
        if (_bufPos >= sizeof(_buf)) {
            size_t written = _file.write(_buf, sizeof(_buf));
            _bufPos = 0;
            if (written != sizeof(_buf)) return false;
        }
        return true;
    }
};

static void normalize_path(const char* inPath, char* outPath, size_t outSize, bool forWrite) {
    if (!inPath || !outPath || outSize == 0) return;
    outPath[0] = '\0';

    // If already absolute
    if (inPath[0] == '/') {
        // Strip "/littlefs" prefix if present
        if (strncmp(inPath, "/littlefs/", 10) == 0) {
            snprintf(outPath, outSize, "%s", inPath + 9);
        } else {
            snprintf(outPath, outSize, "%s", inPath);
        }
        return;
    }

    // Relative path, e.g. "Came.sub"
    if (forWrite) {
        snprintf(outPath, outSize, "/sub/%s", inPath);
    } else {
        char candidate[128];
        snprintf(candidate, sizeof(candidate), "/sub/%s", inPath);
        if (LittleFS.exists(candidate)) {
            snprintf(outPath, outSize, "%s", candidate);
            return;
        }
        snprintf(candidate, sizeof(candidate), "/%s", inPath);
        if (LittleFS.exists(candidate)) {
            snprintf(outPath, outSize, "%s", candidate);
            return;
        }
        snprintf(outPath, outSize, "/sub/%s", inPath);
    }
}

} // anonymous namespace

// =========================================================================
// STORAGE LIFECYCLE & PARTITION MANAGEMENT
// =========================================================================

static bool s_fsMounted = false;

bool sub_storage_init() {
    if (s_fsMounted) return true;
    // Mount LittleFS on the 896 KB 'spiffs' partition at 0x310000 with auto-format on fail
    s_fsMounted = LittleFS.begin(true, "/littlefs", 10, "spiffs");
    if (s_fsMounted) {
        if (!LittleFS.exists("/sub")) {
            LittleFS.mkdir("/sub");
        }
    }
    return s_fsMounted;
}

bool sub_storage_format() {
    bool ok = LittleFS.format();
    s_fsMounted = false;
    if (ok) {
        s_fsMounted = LittleFS.begin(true, "/littlefs", 10, "spiffs");
        if (s_fsMounted) {
            LittleFS.mkdir("/sub");
        }
    }
    return ok && s_fsMounted;
}

size_t sub_storage_total_bytes() {
    if (!sub_storage_init()) return 0;
    return LittleFS.totalBytes();
}

size_t sub_storage_used_bytes() {
    if (!sub_storage_init()) return 0;
    return LittleFS.usedBytes();
}

size_t sub_storage_free_bytes() {
    size_t total = sub_storage_total_bytes();
    size_t used = sub_storage_used_bytes();
    return (total >= used) ? (total - used) : 0;
}

bool sub_storage_is_mounted() {
    return s_fsMounted;
}

// =========================================================================
// DIRECTORY & METADATA
// =========================================================================

bool sub_storage_get_meta(const char* filePath, SignalMeta &outMeta) {
    if (!sub_storage_init() || !filePath) return false;
    memset(&outMeta, 0, sizeof(SignalMeta));

    char path[128];
    normalize_path(filePath, path, sizeof(path), false);

    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) {
        return false;
    }

    size_t fSize = f.size();
    outMeta.fileSize = (uint32_t)fSize;
    strncpy(outMeta.filename, path, sizeof(outMeta.filename) - 1);

    const char* base = strrchr(path, '/');
    base = base ? (base + 1) : path;
    strncpy(outMeta.name, base, sizeof(outMeta.name) - 1);
    char* dot = strrchr(outMeta.name, '.');
    if (dot) *dot = '\0';

    FileStreamReader reader(f);
    SubGhzHeader hdr;
    if (!SubFormatParser::parseHeader(reader, hdr)) {
        f.close();
        return false;
    }

    outMeta.fileType = hdr.fileType;
    outMeta.frequency = hdr.frequency;
    strncpy(outMeta.protocol, hdr.protocol, sizeof(outMeta.protocol) - 1);

    if (hdr.fileType == SUB_TYPE_KEY) {
        SubGhzKeyData key;
        if (SubFormatParser::parseKeyData(reader, key)) {
            outMeta.bitLength = key.bitLength;
            outMeta.code = (uint32_t)key.key;
            outMeta.fullCode = key.key;
            outMeta.pulseLength = (uint16_t)key.te;
            outMeta.repeats = key.repeat;
        }
    } else {
        if (fSize <= 4096) {
            size_t count = 0;
            SubFormatParser::parseRawPulses(reader, [](int32_t, void* ctx) {
                (*(size_t*)ctx)++;
            }, &count);
            outMeta.rawCount = (uint16_t)count;
        } else {
            outMeta.rawCount = (uint16_t)(fSize / 8);
        }
    }

    f.close();
    return true;
}

int sub_storage_list_files(SignalMeta* outList, int maxCount, const char* dirPath) {
    if (!sub_storage_init() || !outList || maxCount <= 0) return 0;

    int count = 0;
    const char* scanPath = dirPath ? dirPath : "/sub";

    fs::File root = LittleFS.open(scanPath);
    if (!root || !root.isDirectory()) {
        root = LittleFS.open("/");
    }
    if (!root || !root.isDirectory()) return 0;

    fs::File file = root.openNextFile();
    while (file && count < maxCount) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            size_t len = strlen(name);
            if (len > 4 && (strcasecmp(name + len - 4, ".sub") == 0)) {
                char fullPath[128];
                if (name[0] == '/') {
                    snprintf(fullPath, sizeof(fullPath), "%s", name);
                } else {
                    snprintf(fullPath, sizeof(fullPath), "%s/%s", scanPath, name);
                }
                file.close();
                if (sub_storage_get_meta(fullPath, outList[count])) {
                    count++;
                }
                file = root.openNextFile();
                continue;
            }
        }
        file = root.openNextFile();
    }
    root.close();
    return count;
}

// =========================================================================
// SAVE OPERATIONS
// =========================================================================

bool sub_storage_save_fixed(const char* filename, const char* proto, float freq, uint64_t code, uint8_t bitLen, uint16_t pulseLen, uint8_t repeats) {
    if (!sub_storage_init() || !filename) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), true);

    fs::File f = LittleFS.open(path, "w");
    if (!f) return false;

    SubGhzHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fileType = SUB_TYPE_KEY;
    hdr.version = 1;
    hdr.frequency = freq;
    hdr.frequencyHz = (uint32_t)(freq * 1000000.0f + 0.5f);
    hdr.preset = SUB_PRESET_OOK_650;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr) - 1);
    strncpy(hdr.protocol, (proto && *proto) ? proto : "Princeton", sizeof(hdr.protocol) - 1);

    SubGhzKeyData key;
    memset(&key, 0, sizeof(key));
    key.bitLength = bitLen ? bitLen : 24;
    key.key = code;
    key.te = pulseLen ? pulseLen : 320;
    key.repeat = repeats ? repeats : 5;

    FileStreamWriter writer(f);
    bool ok = SubFormatSerializer::serializeKey(writer, hdr, key);
    writer.flush();
    f.close();
    return ok;
}

bool sub_storage_save_keeloq(const char* filename, float freq, uint64_t fullData64, const char* manufacture, uint8_t repeats) {
    if (!sub_storage_init() || !filename) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), true);

    fs::File f = LittleFS.open(path, "w");
    if (!f) return false;

    SubGhzHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fileType = SUB_TYPE_KEY;
    hdr.version = 1;
    hdr.frequency = freq;
    hdr.frequencyHz = (uint32_t)(freq * 1000000.0f + 0.5f);
    hdr.preset = SUB_PRESET_OOK_650;
    strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr) - 1);
    strncpy(hdr.protocol, "KeeLoq", sizeof(hdr.protocol) - 1);

    SubGhzKeyData key;
    memset(&key, 0, sizeof(key));
    key.bitLength = 64;
    key.key = fullData64;
    key.te = 400;
    key.repeat = repeats ? repeats : 5;
    if (manufacture && *manufacture) {
        strncpy(key.manufacture, manufacture, sizeof(key.manufacture) - 1);
    }

    FileStreamWriter writer(f);
    bool ok = SubFormatSerializer::serializeKey(writer, hdr, key);
    writer.flush();
    f.close();
    return ok;
}

bool sub_storage_save_raw(const char* filename, float freq, const uint16_t* pulses, uint16_t count, uint8_t repeats, const char* presetStr) {
    (void)repeats;
    if (!sub_storage_init() || !filename || !pulses || count == 0) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), true);

    fs::File f = LittleFS.open(path, "w");
    if (!f) return false;

    SubGhzHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.fileType = SUB_TYPE_RAW;
    hdr.version = 1;
    hdr.frequency = freq;
    hdr.frequencyHz = (uint32_t)(freq * 1000000.0f + 0.5f);
    if (presetStr && *presetStr) {
        strncpy(hdr.presetStr, presetStr, sizeof(hdr.presetStr) - 1);
        hdr.preset = sub_preset_from_string(presetStr);
    } else {
        hdr.preset = SUB_PRESET_OOK_650;
        strncpy(hdr.presetStr, "FuriHalSubGhzPresetOok650Async", sizeof(hdr.presetStr) - 1);
    }
    strncpy(hdr.protocol, "RAW", sizeof(hdr.protocol) - 1);

    FileStreamWriter writer(f);
    bool ok = SubFormatSerializer::serializeRawFromUnsigned(writer, hdr, pulses, count);
    writer.flush();
    f.close();
    return ok;
}

// =========================================================================
// DELETE & READ OPERATIONS
// =========================================================================

bool sub_storage_delete(const char* filename) {
    if (!sub_storage_init() || !filename) return false;
    char path[128];
    normalize_path(filename, path, sizeof(path), false);
    if (!LittleFS.exists(path)) return false;
    return LittleFS.remove(path);
}

struct RawReadContext {
    uint16_t* outPulses;
    uint16_t  maxCount;
    uint16_t  actualCount;
};

static void raw_read_callback(int32_t signedPulseUs, void* ctx) {
    auto* c = static_cast<RawReadContext*>(ctx);
    if (c->actualCount < c->maxCount) {
        c->outPulses[c->actualCount++] = (uint16_t)abs(signedPulseUs);
    }
}

bool sub_storage_read_raw(const char* filename, uint16_t* outPulses, uint16_t maxCount, uint16_t &actualCount) {
    actualCount = 0;
    if (!sub_storage_init() || !filename || !outPulses || maxCount == 0) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), false);

    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) return false;

    FileStreamReader reader(f);
    RawReadContext ctx = { outPulses, maxCount, 0 };
    bool ok = SubFormatParser::parseRawPulses(reader, raw_read_callback, &ctx);
    f.close();

    actualCount = ctx.actualCount;
    return ok && (actualCount > 0);
}

bool sub_storage_read_key(const char* filename, SubGhzHeader &hdr, SubGhzKeyData &key) {
    if (!sub_storage_init() || !filename) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), false);

    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) return false;

    FileStreamReader reader(f);
    bool okHdr = SubFormatParser::parseHeader(reader, hdr);
    bool okKey = SubFormatParser::parseKeyData(reader, key);
    f.close();

    return okHdr && okKey;
}

bool sub_storage_stream_raw(const char* filename, SubFormatParser::PulseCallback cb, void* ctx) {
    if (!sub_storage_init() || !filename || !cb) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), false);

    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) return false;

    FileStreamReader reader(f);
    bool ok = SubFormatParser::parseRawPulses(reader, cb, ctx);
    f.close();
    return ok;
}

// =========================================================================
// DIRECT RADIO TRANSMISSION FROM LITTLEFS
// =========================================================================

static uint16_t s_txPulseBuf[2048];

bool sub_storage_transmit(const char* filename, int customRepeats, int paLevel) {
    if (!sub_storage_init() || !filename) return false;

    char path[128];
    normalize_path(filename, path, sizeof(path), false);

    fs::File f = LittleFS.open(path, "r");
    if (!f || f.isDirectory()) return false;

    FileStreamReader reader(f);
    SubGhzHeader hdr;
    if (!SubFormatParser::parseHeader(reader, hdr)) {
        f.close();
        return false;
    }

    float freq = (hdr.frequency > 0.0f) ? hdr.frequency : 433.92f;

    if (hdr.fileType == SUB_TYPE_KEY) {
        SubGhzKeyData key;
        if (!SubFormatParser::parseKeyData(reader, key)) {
            f.close();
            return false;
        }
        f.close();

        uint8_t reps = (customRepeats > 0) ? (uint8_t)customRepeats : (key.repeat > 0 ? key.repeat : 5);

        if (strcasecmp(hdr.protocol, "Came") == 0 || strcasecmp(hdr.protocol, "Came12") == 0) {
            if (key.bitLength == 12) {
                radio_transmit_came12((uint16_t)key.key, freq, paLevel, reps);
            } else {
                radio_transmit_came24((uint32_t)key.key, freq, paLevel, reps);
            }
        }
        else if (strcasecmp(hdr.protocol, "Came24") == 0) {
            radio_transmit_came24((uint32_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "Nice FLO") == 0 || strcasecmp(hdr.protocol, "Nice") == 0 || strcasecmp(hdr.protocol, "NiceFLO") == 0) {
            radio_transmit_nice12((uint16_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "Linear") == 0) {
            radio_transmit_linear10((uint16_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "Somfy") == 0 || strcasecmp(hdr.protocol, "Somfy RTS") == 0) {
            radio_transmit_somfy_rts((uint32_t)(key.key >> 32), (uint16_t)(key.key >> 16), (uint8_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "PT2240") == 0) {
            radio_transmit_pt2240((uint32_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "DoorHan") == 0) {
            radio_transmit_doorhan((uint32_t)(key.key >> 32), (uint16_t)(key.key >> 16), (uint8_t)key.key, freq, paLevel, reps);
        }
        else if (strcasecmp(hdr.protocol, "KeeLoq") == 0) {
            KeeloqFrame66 frame;
            frame.payload64 = key.key;
            frame.flags2 = 0x01; // Bit 64: VLOW = 1 (Battery OK), Bit 65: RPT = 0 (Initial TX)
            uint16_t count = keeloq_frame_to_pulses(&frame, s_txPulseBuf);
            if (count > 0) {
                radio_transmit_raw_pulses(s_txPulseBuf, count, freq, paLevel, reps);
            }
        }
        else {
            uint16_t te = (key.te > 0) ? (uint16_t)key.te : 320;
            uint8_t bitLen = (key.bitLength > 0) ? key.bitLength : 24;
            radio_transmit_fixed((uint32_t)key.key, bitLen, te, 1, freq, paLevel, reps);
        }
        return true;
    }
    else {
        RawReadContext ctx = { s_txPulseBuf, (uint16_t)(sizeof(s_txPulseBuf) / sizeof(s_txPulseBuf[0])), 0 };
        SubFormatParser::parseRawPulses(reader, raw_read_callback, &ctx);
        f.close();

        if (ctx.actualCount == 0) return false;

        uint8_t reps = (customRepeats > 0) ? (uint8_t)customRepeats : 4;
        radio_transmit_raw_pulses(s_txPulseBuf, ctx.actualCount, freq, paLevel, reps);
        return true;
    }
}

// =========================================================================
// NVS SYNC & MIGRATION
// =========================================================================

void sub_storage_check_and_migrate_nvs() {
    if (!sub_storage_init()) return;

    SignalMeta meta[1];
    int existing = sub_storage_list_files(meta, 1, "/sub");
    if (existing > 0) {
        return;
    }

    Preferences prefs;
    if (!prefs.begin("ghostrf", true)) {
        return;
    }

    int count = prefs.getInt("count", 0);
    if (count <= 0 || count > MAX_SAVED_SIGNALS) {
        prefs.end();
        return;
    }

    for (int i = 0; i < count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "s_%d", i);
        SavedSignal sig;
        if (prefs.getBytes(key, &sig, sizeof(SavedSignal)) == sizeof(SavedSignal)) {
            char fname[64];
            snprintf(fname, sizeof(fname), "/sub/%s.sub", sig.name[0] ? sig.name : "remote");
            if (sig.type == SIG_RAW || sig.rawCount > 0) {
                sub_storage_save_raw(fname, sig.freq, sig.rawPulses, sig.rawCount, sig.repeats);
            } else {
                sub_storage_save_fixed(fname, sig.protoName, sig.freq, sig.code, sig.bitLength, sig.pulseLength, sig.repeats);
            }
        }
    }
    prefs.end();
}
