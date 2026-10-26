#include "keeloq.h"

uint32_t keeloq_encrypt(uint32_t data, uint64_t key) {
    uint32_t x = data;
    for (int r = 0; r < 528; r++) {
        uint8_t index = ((x >> 31) & 1) << 4 |
                        ((x >> 26) & 1) << 3 |
                        ((x >> 20) & 1) << 2 |
                        ((x >> 9)  & 1) << 1 |
                        ((x >> 1)  & 1);
        uint32_t bitNLF = (KEELOQ_NLF >> index) & 1;
        uint32_t bitKey = (uint32_t)((key >> (r & 63)) & 1);
        uint32_t n = bitNLF ^ ((x >> 16) & 1) ^ (x & 1) ^ bitKey;
        x = (x >> 1) | (n << 31);
    }
    return x;
}

uint32_t keeloq_decrypt(uint32_t data, uint64_t key) {
    uint32_t x = data;
    for (int r = 527; r >= 0; r--) {
        uint8_t index = ((x >> 30) & 1) << 4 |
                        ((x >> 25) & 1) << 3 |
                        ((x >> 19) & 1) << 2 |
                        ((x >> 8)  & 1) << 1 |
                        (x & 1);
        uint32_t bitNLF = (KEELOQ_NLF >> index) & 1;
        uint32_t bitKey = (uint32_t)((key >> (r & 63)) & 1);
        uint32_t n = bitNLF ^ ((x >> 15) & 1) ^ ((x >> 31) & 1) ^ bitKey;
        x = (x << 1) | n;
    }
    return x;
}

uint16_t keeloq_build_packet(uint32_t serial, uint16_t counter, uint8_t button, uint64_t key, uint16_t* outPulses) {
    // 1. Pack 32-bit plain data:
    // Bits 0..15: counter (16 bits)
    // Bits 16..19: button (4 bits)
    // Bits 20..29: discriminator = lower 10 bits of serial
    // Bits 30..31: 0b00
    uint32_t plain = ((uint32_t)counter & 0xFFFF) |
                     (((uint32_t)button & 0x0F) << 16) |
                     (((uint32_t)serial & 0x03FF) << 20);

    // 2. Encrypt with 64-bit key
    uint32_t hop = keeloq_encrypt(plain, key);

    // 3. Assemble 66-bit frame (LSB first):
    // Bits 0..31: hop (32 bits)
    // Bits 32..59: serial (28 bits)
    // Bits 60..63: button (4 bits)
    // Bits 64..65: status flags = 0b00
    uint64_t fullData = (uint64_t)hop |
                        (((uint64_t)serial & 0x0FFFFFFFULL) << 32) |
                        (((uint64_t)button & 0x0FULL) << 60);

    // 4. Generate timing pulses:
    // Preamble: 12 cycles of 400us HIGH, 400us LOW (24 pulses)
    uint16_t idx = 0;
    for (int i = 0; i < 12; i++) {
        outPulses[idx++] = 400; // HIGH
        outPulses[idx++] = 400; // LOW
    }

    // Header gap: 4000us LOW (1 pulse)
    outPulses[idx++] = 4000;

    // 66 PWM data bits:
    // Bit 0: 800us HIGH, 400us LOW
    // Bit 1: 400us HIGH, 800us LOW
    for (int b = 0; b < 66; b++) {
        bool bit = (fullData >> b) & 1;
        if (!bit) {
            outPulses[idx++] = 800; // HIGH
            outPulses[idx++] = 400; // LOW
        } else {
            outPulses[idx++] = 400; // HIGH
            outPulses[idx++] = 800; // LOW
        }
    }

    return idx; // Total ~157 pulses
}
