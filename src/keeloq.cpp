#include "keeloq.h"

uint32_t keeloq_encrypt(uint32_t data, uint64_t key) {
    uint32_t x = data;
    for (int r = 0; r < 528; r++) {
        uint8_t index = (uint8_t)(((x >> 31) & 1) << 4 |
                                  ((x >> 26) & 1) << 3 |
                                  ((x >> 20) & 1) << 2 |
                                  ((x >> 9)  & 1) << 1 |
                                  ((x >> 1)  & 1));
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
        uint8_t index = (uint8_t)(((x >> 30) & 1) << 4 |
                                  ((x >> 25) & 1) << 3 |
                                  ((x >> 19) & 1) << 2 |
                                  ((x >> 8)  & 1) << 1 |
                                  (x & 1));
        uint32_t bitNLF = (KEELOQ_NLF >> index) & 1;
        uint32_t bitKey = (uint32_t)((key >> (r & 63)) & 1);
        uint32_t n = bitNLF ^ ((x >> 15) & 1) ^ ((x >> 31) & 1) ^ bitKey;
        x = (x << 1) | n;
    }
    return x;
}

uint64_t keeloq_derive_key_normal(uint32_t serial, uint64_t masterKey) {
    uint32_t s = serial & 0x0FFFFFFFU;
    uint32_t k1 = keeloq_decrypt(s | 0x20000000U, masterKey);
    uint32_t k2 = keeloq_decrypt(s | 0x60000000U, masterKey);
    return ((uint64_t)k2 << 32) | (uint64_t)k1;
}

uint64_t keeloq_derive_key_simple(uint64_t masterKey) {
    return masterKey;
}

uint64_t keeloq_derive_key_secure(uint32_t serial, uint32_t seed, uint64_t masterKey) {
    uint32_t s = serial & 0x0FFFFFFFU;
    uint32_t k1 = keeloq_decrypt(s, masterKey);
    uint32_t k2 = keeloq_decrypt(seed, masterKey);
    return ((uint64_t)k1 << 32) | (uint64_t)k2;
}

void keeloq_pack_frame66(uint32_t hop, uint32_t serial, uint8_t button, bool vlow, bool repeat, KeeloqFrame66* frame) {
    if (!frame) return;
    frame->payload64 = ((uint64_t)hop & 0xFFFFFFFFULL) |
                       (((uint64_t)serial & 0x0FFFFFFFULL) << 32) |
                       (((uint64_t)button & 0x0FULL) << 60);
    frame->flags2 = (uint8_t)((vlow ? 0x01 : 0x00) | (repeat ? 0x02 : 0x00));
}

void keeloq_unpack_frame66(const KeeloqFrame66* frame, uint32_t* hop, uint32_t* serial, uint8_t* button, bool* vlow, bool* repeat) {
    if (!frame) return;
    if (hop) *hop = (uint32_t)(frame->payload64 & 0xFFFFFFFFULL);
    if (serial) *serial = (uint32_t)((frame->payload64 >> 32) & 0x0FFFFFFFULL);
    if (button) *button = (uint8_t)((frame->payload64 >> 60) & 0x0FULL);
    if (vlow) *vlow = (frame->flags2 & 0x01) != 0;
    if (repeat) *repeat = (frame->flags2 & 0x02) != 0;
}

bool keeloq_get_bit66(const KeeloqFrame66* frame, uint8_t bitIndex) {
    if (!frame || bitIndex >= 66) return false;
    if (bitIndex < 64) {
        return ((frame->payload64 >> bitIndex) & 1ULL) != 0;
    } else if (bitIndex == 64) {
        return (frame->flags2 & 0x01) != 0;
    } else { // bitIndex == 65
        return (frame->flags2 & 0x02) != 0;
    }
}

void keeloq_set_bit66(KeeloqFrame66* frame, uint8_t bitIndex, bool value) {
    if (!frame || bitIndex >= 66) return;
    if (bitIndex < 64) {
        if (value) {
            frame->payload64 |= (1ULL << bitIndex);
        } else {
            frame->payload64 &= ~(1ULL << bitIndex);
        }
    } else if (bitIndex == 64) {
        if (value) frame->flags2 |= 0x01;
        else frame->flags2 &= (uint8_t)~0x01;
    } else { // bitIndex == 65
        if (value) frame->flags2 |= 0x02;
        else frame->flags2 &= (uint8_t)~0x02;
    }
}

uint16_t keeloq_frame_to_pulses(const KeeloqFrame66* frame, uint16_t* outPulses) {
    if (!frame || !outPulses) return 0;

    uint16_t idx = 0;
    // Preamble: 12 cycles of 400us HIGH, 400us LOW (24 pulses)
    for (int i = 0; i < 12; i++) {
        outPulses[idx++] = 400; // HIGH
        outPulses[idx++] = 400; // LOW
    }

    // Header gap: 4000us LOW (1 pulse)
    outPulses[idx++] = 4000;

    // 66 PWM data bits (LSB first):
    // Bit 0: 800us HIGH, 400us LOW
    // Bit 1: 400us HIGH, 800us LOW
    for (int b = 0; b < 66; b++) {
        bool bit = keeloq_get_bit66(frame, (uint8_t)b);
        if (!bit) {
            outPulses[idx++] = 800; // HIGH
            outPulses[idx++] = 400; // LOW
        } else {
            outPulses[idx++] = 400; // HIGH
            outPulses[idx++] = 800; // LOW
        }
    }

    return idx; // 24 + 1 + 132 = 157 pulses
}

bool keeloq_pulses_to_frame(const uint16_t* pulses, uint16_t count, KeeloqFrame66* outFrame) {
    if (!pulses || !outFrame) return false;

    // Minimum pulses: header gap (1) + 66 bits (132 pulses) = 133 pulses
    if (count < 133) return false;

    // Look for header gap: nominally 4000us LOW (acceptable range: 2000..8000us)
    int gapIdx = -1;
    for (uint16_t i = 0; i < count; i++) {
        if (pulses[i] >= 2000 && pulses[i] <= 8000) {
            gapIdx = (int)i;
            break;
        }
    }
    if (gapIdx < 0 || (count - gapIdx - 1) < 66 * 2) {
        return false;
    }

    outFrame->payload64 = 0;
    outFrame->flags2 = 0;

    uint16_t pIdx = (uint16_t)(gapIdx + 1);
    for (int b = 0; b < 66; b++) {
        uint16_t tHigh = pulses[pIdx++];
        uint16_t tLow = pulses[pIdx++];
        // Bit 0: 800us HIGH, 400us LOW (tHigh > tLow)
        // Bit 1: 400us HIGH, 800us LOW (tLow > tHigh)
        bool bit = (tLow > tHigh);
        keeloq_set_bit66(outFrame, (uint8_t)b, bit);
    }

    return true;
}

uint16_t keeloq_build_packet_ex(uint32_t serial, uint16_t counter, uint8_t button, bool vlow, bool repeat, uint64_t key, uint16_t* outPulses) {
    uint32_t plain = ((uint32_t)counter & 0xFFFFU) |
                     (((uint32_t)button & 0x0FU) << 16) |
                     (((uint32_t)serial & 0x03FFU) << 20);

    uint32_t hop = keeloq_encrypt(plain, key);

    KeeloqFrame66 frame;
    keeloq_pack_frame66(hop, serial, button, vlow, repeat, &frame);
    return keeloq_frame_to_pulses(&frame, outPulses);
}

uint16_t keeloq_build_packet(uint32_t serial, uint16_t counter, uint8_t button, uint64_t key, uint16_t* outPulses) {
    return keeloq_build_packet_ex(serial, counter, button, true, false, key, outPulses);
}
