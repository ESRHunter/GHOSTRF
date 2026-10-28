#include "keeloq_analyzer.h"

#define KLQ_MAX_TRANSMITTERS 32
#define KLQ_RESYNC_TIMEOUT_MS 20000

struct KeeloqTransmitterEntry {
    uint32_t serial;
    uint16_t lastCounter;
    bool     hasLastCounter;
    bool     resyncPending;
    uint16_t resyncCounter;
    uint32_t resyncTimestampMs;
    uint32_t lastSeenTimestampMs;
    bool     inUse;
};

static KeeloqTransmitterEntry s_transmitters[KLQ_MAX_TRANSMITTERS] = {};
static uint32_t s_manualTimeMs = 0;
static bool s_hasManualTime = false;

#ifdef ARDUINO
static inline uint32_t get_system_time_ms() {
    if (s_hasManualTime) return s_manualTimeMs;
    return millis();
}
#else
#include <chrono>
static inline uint32_t get_system_time_ms() {
    if (s_hasManualTime) return s_manualTimeMs;
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - start).count();
}
#endif

void keeloq_analyzer_reset() {
    for (int i = 0; i < KLQ_MAX_TRANSMITTERS; i++) {
        s_transmitters[i].inUse = false;
        s_transmitters[i].serial = 0;
        s_transmitters[i].lastCounter = 0;
        s_transmitters[i].hasLastCounter = false;
        s_transmitters[i].resyncPending = false;
        s_transmitters[i].resyncCounter = 0;
        s_transmitters[i].resyncTimestampMs = 0;
        s_transmitters[i].lastSeenTimestampMs = 0;
    }
    s_hasManualTime = false;
    s_manualTimeMs = 0;
}

static KeeloqTransmitterEntry* find_or_allocate_entry(uint32_t serial, uint32_t currentTimeMs) {
    uint32_t s = serial & 0x0FFFFFFFU;
    // 1. Search existing
    for (int i = 0; i < KLQ_MAX_TRANSMITTERS; i++) {
        if (s_transmitters[i].inUse && s_transmitters[i].serial == s) {
            return &s_transmitters[i];
        }
    }
    // 2. Find empty slot
    for (int i = 0; i < KLQ_MAX_TRANSMITTERS; i++) {
        if (!s_transmitters[i].inUse) {
            s_transmitters[i].inUse = true;
            s_transmitters[i].serial = s;
            s_transmitters[i].lastCounter = 0;
            s_transmitters[i].hasLastCounter = false;
            s_transmitters[i].resyncPending = false;
            s_transmitters[i].resyncCounter = 0;
            s_transmitters[i].resyncTimestampMs = 0;
            s_transmitters[i].lastSeenTimestampMs = currentTimeMs;
            return &s_transmitters[i];
        }
    }
    // 3. LRU replacement if table is full
    int lruIdx = 0;
    uint32_t maxAge = (uint32_t)(currentTimeMs - s_transmitters[0].lastSeenTimestampMs);
    for (int i = 1; i < KLQ_MAX_TRANSMITTERS; i++) {
        uint32_t age = (uint32_t)(currentTimeMs - s_transmitters[i].lastSeenTimestampMs);
        if (age > maxAge) {
            maxAge = age;
            lruIdx = i;
        }
    }
    s_transmitters[lruIdx].inUse = true;
    s_transmitters[lruIdx].serial = s;
    s_transmitters[lruIdx].lastCounter = 0;
    s_transmitters[lruIdx].hasLastCounter = false;
    s_transmitters[lruIdx].resyncPending = false;
    s_transmitters[lruIdx].resyncCounter = 0;
    s_transmitters[lruIdx].resyncTimestampMs = 0;
    s_transmitters[lruIdx].lastSeenTimestampMs = currentTimeMs;
    return &s_transmitters[lruIdx];
}

void keeloq_analyzer_set_last_counter(uint32_t serial, uint16_t counter) {
    uint32_t s = serial & 0x0FFFFFFFU;
    KeeloqTransmitterEntry* entry = find_or_allocate_entry(s, get_system_time_ms());
    if (entry) {
        entry->lastCounter = counter;
        entry->hasLastCounter = true;
        entry->resyncPending = false;
    }
}

uint16_t keeloq_analyzer_get_last_counter(uint32_t serial, bool* outHasCounter) {
    uint32_t s = serial & 0x0FFFFFFFU;
    for (int i = 0; i < KLQ_MAX_TRANSMITTERS; i++) {
        if (s_transmitters[i].inUse && s_transmitters[i].serial == s) {
            if (outHasCounter) *outHasCounter = s_transmitters[i].hasLastCounter;
            return s_transmitters[i].lastCounter;
        }
    }
    if (outHasCounter) *outHasCounter = false;
    return 0;
}

void keeloq_analyzer_set_manual_time(uint32_t ms) {
    s_manualTimeMs = ms;
    s_hasManualTime = true;
}

bool keeloq_analyze_packet_timed(uint32_t serial, uint8_t button, uint32_t hop, uint64_t masterKey, uint32_t currentTimeMs, KeeloqAnalysisResult &result) {
    result.serial = serial & 0x0FFFFFFFU;
    result.button = button & 0x0FU;
    result.encryptedHop = hop;
    result.decryptedCounter = 0;
    result.discriminator = 0;
    result.discriminatorValid = false;
    result.counterDelta = 0;
    result.syncStatus = KLQ_SYNC_UNKNOWN;

    // 1. Decrypt and validate discriminator
    // First attempt: Microchip Normal Learning derivation
    uint64_t devKey = keeloq_derive_key_normal(result.serial, masterKey);
    uint32_t plain = keeloq_decrypt(hop, devKey);
    uint16_t expDisc = (uint16_t)(result.serial & 0x03FFU);
    uint16_t disc = (uint16_t)((plain >> 20) & 0x03FFU);
    bool discValid = (disc == expDisc);

    // Second attempt: Simple Learning (direct master key used as device key)
    if (!discValid) {
        uint32_t plainDirect = keeloq_decrypt(hop, masterKey);
        uint16_t discDirect = (uint16_t)((plainDirect >> 20) & 0x03FFU);
        if (discDirect == expDisc) {
            plain = plainDirect;
            disc = discDirect;
            discValid = true;
        }
    }

    result.decryptedCounter = (uint16_t)(plain & 0xFFFFU);
    result.discriminator = disc;
    result.discriminatorValid = discValid;

    if (!discValid) {
        result.syncStatus = KLQ_SYNC_UNKNOWN;
        return false;
    }

    // 2. Synchronization Window State Machine
    KeeloqTransmitterEntry* entry = find_or_allocate_entry(result.serial, currentTimeMs);
    if (!entry) {
        result.syncStatus = KLQ_SYNC_UNKNOWN;
        return true;
    }

    if (!entry->hasLastCounter) {
        // First observation establishes baseline
        entry->lastCounter = result.decryptedCounter;
        entry->hasLastCounter = true;
        entry->resyncPending = false;
        entry->lastSeenTimestampMs = currentTimeMs;
        result.counterDelta = 0;
        result.syncStatus = KLQ_SYNC_OK_SEQUENTIAL;
        return true;
    }

    uint16_t rxCounter = result.decryptedCounter;
    uint16_t delta = (uint16_t)(rxCounter - entry->lastCounter);
    result.counterDelta = (int32_t)delta;

    // Check if in pending resynchronization mode:
    // If pending, evaluate whether this is the consecutive next press within 20s
    if (entry->resyncPending) {
        bool notTimedOut = ((uint32_t)(currentTimeMs - entry->resyncTimestampMs) <= (uint32_t)KLQ_RESYNC_TIMEOUT_MS);
        bool isConsecutive = (rxCounter == (uint16_t)(entry->resyncCounter + 1));

        if (notTimedOut && isConsecutive) {
            // Resynchronization verified!
            result.syncStatus = KLQ_SYNC_OK_RESYNC_WINDOW;
            result.counterDelta = 1;
            entry->lastCounter = rxCounter;
            entry->resyncPending = false;
            entry->lastSeenTimestampMs = currentTimeMs;
            return true;
        }
    }

    // 3-Zone Evaluation:
    if (delta == 0) {
        // Zone 3: Blocked (Replay Attack)
        result.syncStatus = KLQ_SYNC_ERR_REPLAY_DETECTED;
        entry->resyncPending = false;
        entry->lastSeenTimestampMs = currentTimeMs;
        return true;
    } else if (delta >= 1 && delta <= 16) {
        // Zone 1: Open Window (1..16)
        result.syncStatus = KLQ_SYNC_OK_SEQUENTIAL;
        entry->lastCounter = rxCounter;
        entry->resyncPending = false;
        entry->lastSeenTimestampMs = currentTimeMs;
        return true;
    } else if (delta >= 17 && delta < 32768) {
        // Zone 2: Resync Window (17..32767)
        // Store pending counter and start 20s timeout window
        entry->resyncPending = true;
        entry->resyncCounter = rxCounter;
        entry->resyncTimestampMs = currentTimeMs;
        entry->lastSeenTimestampMs = currentTimeMs;
        result.syncStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        return true;
    } else {
        // Zone 3: Blocked Window (delta >= 32768)
        result.syncStatus = KLQ_SYNC_ERR_DESYNCHRONIZED;
        entry->resyncPending = false;
        entry->lastSeenTimestampMs = currentTimeMs;
        return true;
    }
}

bool keeloq_analyze_packet(uint32_t serial, uint8_t button, uint32_t hop, uint64_t masterKey, KeeloqAnalysisResult &result) {
    return keeloq_analyze_packet_timed(serial, button, hop, masterKey, get_system_time_ms(), result);
}

bool keeloq_emulate_next_frame_devkey(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t deviceKey, KeeloqFrame66* outFrame) {
    if (!outFrame) return false;
    uint16_t nextCounter = (uint16_t)(lastCounter + 1);
    uint32_t s = serial & 0x0FFFFFFFU;
    uint32_t btn = button & 0x0FU;
    uint32_t disc = s & 0x03FFU;

    uint32_t plain = ((uint32_t)nextCounter & 0xFFFFU) |
                     (btn << 16) |
                     (disc << 20);

    uint32_t hop = keeloq_encrypt(plain, deviceKey);

    keeloq_pack_frame66(hop, s, (uint8_t)btn, true, false, outFrame);
    return true;
}

bool keeloq_emulate_next_frame(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t masterKey, KeeloqFrame66* outFrame, bool useNormalLearning) {
    uint32_t s = serial & 0x0FFFFFFFU;
    uint64_t devKey = useNormalLearning ? keeloq_derive_key_normal(s, masterKey) : masterKey;
    return keeloq_emulate_next_frame_devkey(s, button, lastCounter, devKey, outFrame);
}

uint16_t keeloq_emulate_next_devkey(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t deviceKey, uint16_t* outPulses) {
    KeeloqFrame66 frame;
    if (!keeloq_emulate_next_frame_devkey(serial, button, lastCounter, deviceKey, &frame)) {
        return 0;
    }
    return keeloq_frame_to_pulses(&frame, outPulses);
}

uint16_t keeloq_emulate_next(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t masterKey, uint16_t* outPulses, bool useNormalLearning) {
    KeeloqFrame66 frame;
    if (!keeloq_emulate_next_frame(serial, button, lastCounter, masterKey, &frame, useNormalLearning)) {
        return 0;
    }
    return keeloq_frame_to_pulses(&frame, outPulses);
}
