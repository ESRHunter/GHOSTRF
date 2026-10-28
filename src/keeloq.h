#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef ARDUINO
#include <Arduino.h>
#endif

#define KEELOQ_NLF 0x3A5C742E

// 64-bit Master / Crypt Keys
const uint64_t KLQ_KEY_GENERIC = 0x0123456789ABCDEFULL;
const uint64_t KLQ_KEY_DOORHAN = 0x2749AC7815B284A1ULL;

// 66-bit KeeLoq transmission frame structure
// Eliminates 64-bit shift overflow (UB on shifting uint64_t by >= 64)
// Bits 0..31: 32-bit encrypted hop code (LSB first)
// Bits 32..59: 28-bit transmitter serial number (LSB first)
// Bits 60..63: 4-bit button code (S3..S0)
// Bit 64: VLOW (low battery flag: 1 = Battery OK, 0 = Low Battery)
// Bit 65: RPT (repeat status: 0 = First transmission, 1 = Repeat)
struct KeeloqFrame66 {
    uint64_t payload64; // Bits 0..63 (Hop[0..31] | Serial[32..59] | Button[60..63])
    uint8_t  flags2;    // Bit 0: VLOW (bit 64), Bit 1: RPT (bit 65)

    uint32_t getHop() const { return (uint32_t)(payload64 & 0xFFFFFFFFULL); }
    uint32_t getSerial() const { return (uint32_t)((payload64 >> 32) & 0x0FFFFFFFULL); }
    uint8_t  getButton() const { return (uint8_t)((payload64 >> 60) & 0x0FULL); }
    bool     getVlow() const { return (flags2 & 0x01) != 0; }
    bool     getRepeat() const { return (flags2 & 0x02) != 0; }
};

// 528-cycle NLFSR KeeLoq block cipher
uint32_t keeloq_encrypt(uint32_t data, uint64_t key);
uint32_t keeloq_decrypt(uint32_t data, uint64_t key);

// Key Derivation Functions
// Microchip Normal Learning (AN642 / AN645 / AN662):
// K1 = Decrypt((serial & 0x0FFFFFFF) | 0x20000000, masterKey)
// K2 = Decrypt((serial & 0x0FFFFFFF) | 0x60000000, masterKey)
// DeviceKey = ((uint64_t)K2 << 32) | K1
uint64_t keeloq_derive_key_normal(uint32_t serial, uint64_t masterKey);

// Simple Learning (DeviceKey = MasterKey)
uint64_t keeloq_derive_key_simple(uint64_t masterKey);

// Secure Learning with 32-bit Seed
uint64_t keeloq_derive_key_secure(uint32_t serial, uint32_t seed, uint64_t masterKey);

// 66-bit Frame Packing & Unpacking
void keeloq_pack_frame66(uint32_t hop, uint32_t serial, uint8_t button, bool vlow, bool repeat, KeeloqFrame66* frame);
void keeloq_unpack_frame66(const KeeloqFrame66* frame, uint32_t* hop, uint32_t* serial, uint8_t* button, bool* vlow, bool* repeat);

// Bit-level safe getter and setter for 0 <= bitIndex < 66
bool keeloq_get_bit66(const KeeloqFrame66* frame, uint8_t bitIndex);
void keeloq_set_bit66(KeeloqFrame66* frame, uint8_t bitIndex, bool value);

// Builds 66-bit KeeLoq transmission pulses (Preamble + Gap + 66 PWM bits)
// Returns number of pulses written into outPulses (157 pulses)
uint16_t keeloq_frame_to_pulses(const KeeloqFrame66* frame, uint16_t* outPulses);

// Parses received KeeLoq timing pulses back into a 66-bit frame
bool keeloq_pulses_to_frame(const uint16_t* pulses, uint16_t count, KeeloqFrame66* outFrame);

// Compatibility builder: packs plain payload with counter, button, discriminator (serial & 0x03FF),
// encrypts with key, packs 66-bit frame (VLOW=1, RPT=0), and generates 157 pulses.
uint16_t keeloq_build_packet(uint32_t serial, uint16_t counter, uint8_t button, uint64_t key, uint16_t* outPulses);

// Extended builder with explicit vlow and repeat status
uint16_t keeloq_build_packet_ex(uint32_t serial, uint16_t counter, uint8_t button, bool vlow, bool repeat, uint64_t key, uint16_t* outPulses);
