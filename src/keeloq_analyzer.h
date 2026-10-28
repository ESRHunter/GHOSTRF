#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "keeloq.h"

// 3-Zone Synchronization State Status
enum KeeloqSyncStatus {
    KLQ_SYNC_UNKNOWN = 0,
    KLQ_SYNC_OK_SEQUENTIAL,
    KLQ_SYNC_OK_RESYNC_WINDOW,
    KLQ_SYNC_ERR_REPLAY_DETECTED,
    KLQ_SYNC_ERR_DESYNCHRONIZED
};

// Result of packet inspection and cryptographic decryption
struct KeeloqAnalysisResult {
    uint32_t serial;
    uint8_t  button;
    uint32_t encryptedHop;
    uint16_t decryptedCounter;
    uint16_t discriminator;
    bool     discriminatorValid;
    int32_t  counterDelta;
    KeeloqSyncStatus syncStatus;
};

// Core Packet Analysis:
// Decrypts 32-bit hopping code (using Normal Learning or direct key), validates 10-bit discriminator,
// inspects 16-bit counter against 3-zone synchronization window, and populates result.
// Returns true if discriminator validation succeeds (or packet was analyzed), false if discriminator mismatch.
bool keeloq_analyze_packet(uint32_t serial, uint8_t button, uint32_t hop, uint64_t masterKey, KeeloqAnalysisResult &result);

// Packet analysis with explicit timestamp injection for deterministic timing tests
bool keeloq_analyze_packet_timed(uint32_t serial, uint8_t button, uint32_t hop, uint64_t masterKey, uint32_t currentTimeMs, KeeloqAnalysisResult &result);

// Resets internal transmitter tracking and pending resync states
void keeloq_analyzer_reset();

// Explicitly register / configure baseline counter for a transmitter
void keeloq_analyzer_set_last_counter(uint32_t serial, uint16_t counter);

// Query last counter for a transmitter
uint16_t keeloq_analyzer_get_last_counter(uint32_t serial, bool* outHasCounter = nullptr);

// Injects manual mock time for keeloq_analyze_packet
void keeloq_analyzer_set_manual_time(uint32_t ms);

// Safe Single-Step Emulation:
// Computes CNT_next = lastCounter + 1 (with 16-bit rollover), assembles plain payload with button
// and 10-bit discriminator (serial & 0x03FF), encrypts using device key (derived via Normal Learning
// from masterKey, or direct key if useNormalLearning is false), assembles 66-bit frame, and generates
// 157 PWM pulses for transmission.
uint16_t keeloq_emulate_next(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t masterKey, uint16_t* outPulses, bool useNormalLearning = true);

// Single-step emulation using direct 64-bit device key
uint16_t keeloq_emulate_next_devkey(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t deviceKey, uint16_t* outPulses);

// Emulate next frame without generating timing pulses
bool keeloq_emulate_next_frame(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t masterKey, KeeloqFrame66* outFrame, bool useNormalLearning = true);
bool keeloq_emulate_next_frame_devkey(uint32_t serial, uint8_t button, uint16_t lastCounter, uint64_t deviceKey, KeeloqFrame66* outFrame);
