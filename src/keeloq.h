#pragma once
#include <Arduino.h>

#define KEELOQ_NLF 0x3A5C742E

// 64-bit Master / Crypt Keys
const uint64_t KLQ_KEY_GENERIC = 0x0123456789ABCDEFULL;
const uint64_t KLQ_KEY_DOORHAN = 0x2749AC7815B284A1ULL;

uint32_t keeloq_encrypt(uint32_t data, uint64_t key);
uint32_t keeloq_decrypt(uint32_t data, uint64_t key);

// Builds 66-bit KeeLoq transmission pulses (Preamble + Gap + 66 PWM bits)
// Returns number of pulses written into outPulses (typically 157 pulses)
uint16_t keeloq_build_packet(uint32_t serial, uint16_t counter, uint8_t button, uint64_t key, uint16_t* outPulses);
