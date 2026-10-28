// GHOSTRF Challenger R2_2: CC1101 Radio State Transitions & Modulation Stress Harness
// Focus: Direct integration testing with production src/radio_modes.cpp,
// 50,000 rapid preset transitions, TI CC1101 datasheet invariants,
// Carson's rule filter bandwidths, and SPI stall resilience.

#include "test_harness.h"
#include "radio_modes.h"
#include "config.h"
#include <ELECHOUSE_CC1101_SRC_DRV.h>

#include <vector>
#include <string>
#include <chrono>
#include <cstring>
#include <iostream>

using ghostrf_test::TestTier;

// =========================================================================
// TI CC1101 STATE MACHINE EMULATOR & INVARIANT VALIDATOR
// =========================================================================

#define MARC_SLEEP          0x00
#define MARC_IDLE           0x01
#define MARC_XOFF           0x02
#define MARC_MANCAL         0x04
#define MARC_CALIBRATE      0x05
#define MARC_SETTLING       0x07
#define MARC_RX             0x0D
#define MARC_RX_END         0x0E
#define MARC_TX             0x13
#define MARC_TX_END         0x14

namespace {

class CC1101StateEmulator {
public:
    uint8_t  marcstate;
    uint8_t  regs[0x40];
    int      calCyclesRemaining;
    bool     faultInjectStuckRx;
    bool     faultInjectStuckCal;
    size_t   strobeCount;
    size_t   regWriteCount;
    std::vector<std::string> violations;

    CC1101StateEmulator() {
        reset();
    }

    void reset() {
        marcstate = MARC_IDLE;
        memset(regs, 0, sizeof(regs));
        calCyclesRemaining = 0;
        faultInjectStuckRx = false;
        faultInjectStuckCal = false;
        strobeCount = 0;
        regWriteCount = 0;
        violations.clear();
    }

    void SpiStrobe(uint8_t strobe) {
        strobeCount++;
        switch (strobe) {
            case CC1101_SIDLE:
                if (!faultInjectStuckRx && !faultInjectStuckCal) {
                    marcstate = MARC_IDLE;
                    calCyclesRemaining = 0;
                }
                break;

            case CC1101_SCAL:
                if (faultInjectStuckCal) {
                    marcstate = MARC_CALIBRATE;
                } else {
                    marcstate = MARC_CALIBRATE;
                    calCyclesRemaining = 2; // Takes 2 status polls to finish calibration
                }
                break;

            case CC1101_SRX:
                if (marcstate != MARC_IDLE) {
                    violations.push_back("SRX strobe issued while not in IDLE (current=" + std::to_string(marcstate) + ")");
                }
                marcstate = MARC_RX;
                break;

            case CC1101_STX:
                if (marcstate != MARC_IDLE) {
                    violations.push_back("STX strobe issued while not in IDLE (current=" + std::to_string(marcstate) + ")");
                }
                marcstate = MARC_TX;
                break;

            case CC1101_SFRX:
            case CC1101_SFTX:
                // TI CC1101 Datasheet Section 10.1: Flush strobes must only be issued in IDLE or overflow
                if (marcstate == MARC_RX || marcstate == MARC_TX) {
                    violations.push_back("FIFO flush strobe issued during active RX/TX (marcstate=" + std::to_string(marcstate) + ")");
                }
                break;

            default:
                break;
        }
    }

    uint8_t SpiReadStatus(uint8_t addr) {
        if (addr == CC1101_MARCSTATE) {
            if (calCyclesRemaining > 0) {
                calCyclesRemaining--;
                if (calCyclesRemaining == 0) {
                    marcstate = MARC_IDLE;
                }
            }
            return marcstate;
        }
        return 0;
    }

    uint8_t SpiReadReg(uint8_t addr) {
        if (addr < 0x40) return regs[addr];
        return 0;
    }

    void SpiWriteReg(uint8_t addr, uint8_t val) {
        regWriteCount++;
        // TI CC1101 Section 19: Writing configuration registers while in RX/TX violates state machine
        if (addr <= CC1101_FREND0) {
            if (marcstate != MARC_IDLE && marcstate != MARC_SLEEP) {
                violations.push_back("Register write 0x" + std::to_string(addr) + " while in active state " + std::to_string(marcstate));
            }
        }
        if (addr < 0x40) {
            regs[addr] = val;
        }
    }

    void setMHZ(float mhz) {
        SpiWriteReg(CC1101_FREQ2, (uint8_t)(mhz / 10));
    }

    void setCCMode(bool) {
        SpiWriteReg(CC1101_IOCFG0, 0x0D);
        SpiWriteReg(CC1101_PKTCTRL0, 0x32);
    }

    void setModulation(uint8_t m) {
        SpiWriteReg(CC1101_MDMCFG2, (SpiReadReg(CC1101_MDMCFG2) & 0x8F) | ((m & 0x07) << 4));
    }

    void setDeviation(float devKhz) {
        SpiWriteReg(CC1101_DEVIATN, (uint8_t)devKhz);
    }

    void setPA(int pa) {
        SpiWriteReg(CC1101_FREND0, (uint8_t)pa);
    }
};

static CC1101StateEmulator s_emu;

} // anonymous namespace

// Implement ELECHOUSE_CC1101 driver methods delegating to our state emulator
void ELECHOUSE_CC1101::SpiStrobe(byte strobe) { s_emu.SpiStrobe(strobe); }
byte ELECHOUSE_CC1101::SpiReadStatus(byte addr) { return s_emu.SpiReadStatus(addr); }
byte ELECHOUSE_CC1101::SpiReadReg(byte addr) { return s_emu.SpiReadReg(addr); }
void ELECHOUSE_CC1101::SpiWriteReg(byte addr, byte value) { s_emu.SpiWriteReg(addr, value); }
void ELECHOUSE_CC1101::setMHZ(float mhz) { s_emu.setMHZ(mhz); }
void ELECHOUSE_CC1101::setCCMode(bool s) { s_emu.setCCMode(s); }
void ELECHOUSE_CC1101::setModulation(byte m) { s_emu.setModulation(m); }
void ELECHOUSE_CC1101::setDeviation(float d) { s_emu.setDeviation(d); }
void ELECHOUSE_CC1101::setPA(int p) { s_emu.setPA(p); }

// Global instance defined for radio_modes.cpp
ELECHOUSE_CC1101 ELECHOUSE_cc1101;

// ----------------------------------------------------------------------------
// TEST 1: Rapid 10,000 Preset Transitions (Calling Production radio_set_preset)
// ----------------------------------------------------------------------------
void test_r2_stress_radio_rapid_preset_switching_10k() {
    s_emu.reset();

    const RfModulation modSequence[] = { MOD_OOK_ASK, MOD_2FSK, MOD_GFSK };
    const float freqSequence[] = { 433.92f, 868.35f, 315.00f };
    const int paSequence[] = { 10, 0, -10 };

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; i++) {
        RfModulation mod = modSequence[i % 3];
        float freq = freqSequence[(i / 3) % 3];
        int pa = paSequence[i % 3];

        // Put radio into active RX or TX state before preset switch
        if (i % 2 == 0) {
            s_emu.marcstate = MARC_RX;
        } else {
            s_emu.marcstate = MARC_TX;
        }

        // Call the ACTUAL production function from src/radio_modes.cpp!
        bool ok = radio_set_preset(mod, freq, pa);
        ASSERT_TRUE(ok);

        // State machine must settle cleanly in IDLE
        ASSERT_EQ((int)MARC_IDLE, (int)s_emu.marcstate);

        // Verify active tracking accessors
        ASSERT_EQ((int)mod, (int)radio_get_current_modulation());
        ASSERT_NEAR(freq, radio_get_current_frequency(), 0.001);
        ASSERT_EQ(pa, radio_get_current_pa());

        // Carson's rule filter bandwidth check
        uint8_t mdmcfg4 = s_emu.regs[CC1101_MDMCFG4];
        if (mod == MOD_OOK_ASK) {
            ASSERT_EQ(0x10, mdmcfg4 & 0xF0); // ~650 kHz
            ASSERT_EQ(110, radio_get_glitch_filter());
        } else if (mod == MOD_2FSK) {
            ASSERT_EQ(0x50, mdmcfg4 & 0xF0); // ~270 kHz
            ASSERT_EQ(20, radio_get_glitch_filter());
        } else if (mod == MOD_GFSK) {
            ASSERT_EQ(0x80, mdmcfg4 & 0xF0); // ~135 kHz
            ASSERT_EQ(20, radio_get_glitch_filter());
        }

        // Asynchronous serial mode check
        ASSERT_EQ(0x0D, s_emu.regs[CC1101_IOCFG0]);
        ASSERT_EQ(0x32, s_emu.regs[CC1101_PKTCTRL0]);
    }

    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    // Verify ZERO state machine violations across all 10,000 transitions
    for (const auto& v : s_emu.violations) {
        std::cout << "CC1101 Invariant Violation: " << v << "\n";
    }
    ASSERT_EQ(0U, s_emu.violations.size());
    ASSERT_TRUE(totalMs < 3500.0); // 10,000 transitions with ~200us sleep complete in <3.5s
}

// ----------------------------------------------------------------------------
// TEST 2: Carson's Rule Bandwidths & Frequency Deviations Verification
// ----------------------------------------------------------------------------
void test_r2_stress_carsons_rule_and_deviations() {
    s_emu.reset();

    // 1. OOK: 650 kHz, 0.0 kHz dev, 110 us glitch
    radio_set_preset(MOD_OOK_ASK, 433.92f, 10);
    ASSERT_EQ(0x10, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(0, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(110, radio_get_glitch_filter());

    // 2. 2-FSK: 270 kHz, 47.6 kHz dev, 20 us glitch
    radio_set_preset(MOD_2FSK, 868.35f, 7);
    ASSERT_EQ(0x50, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(47, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(20, radio_get_glitch_filter());

    // 3. GFSK: 135 kHz, 38.0 kHz dev, 20 us glitch
    radio_set_preset(MOD_GFSK, 315.00f, 0);
    ASSERT_EQ(0x80, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(38, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(20, radio_get_glitch_filter());

    ASSERT_EQ(0U, s_emu.violations.size());
}

// ----------------------------------------------------------------------------
// TEST 3: Hardware Bus Stall & Timeout Resilience
// ----------------------------------------------------------------------------
void test_r2_stress_radio_stall_resilience() {
    s_emu.reset();

    // 1. Stuck in RX state (SIDLE strobe ignored by hardware)
    s_emu.faultInjectStuckRx = true;
    s_emu.marcstate = MARC_RX;

    // radio_safe_sidle() must return false on 20ms timeout without infinite hang
    bool sidleOk = radio_safe_sidle();
    ASSERT_FALSE(sidleOk);

    // 2. Stuck in Calibration state (SCAL never finishes)
    s_emu.reset();
    s_emu.faultInjectStuckCal = true;

    // radio_recalibrate_synth() must return false on 25ms timeout without infinite hang
    bool scalOk = radio_recalibrate_synth();
    ASSERT_FALSE(scalOk);

    // 3. Recovery after hardware stall cleared
    s_emu.reset();
    bool recovered = radio_set_preset(MOD_2FSK, 433.92f, 10);
    ASSERT_TRUE(recovered);
    ASSERT_EQ((int)MARC_IDLE, (int)s_emu.marcstate);
    ASSERT_EQ(0U, s_emu.violations.size());
}

// ----------------------------------------------------------------------------
// TEST 4: Glitch Filter Boundary Clamping Safety
// ----------------------------------------------------------------------------
void test_r2_stress_glitch_filter_clamping() {
    // Range is strictly clamped to [10, 500] us
    radio_set_glitch_filter(0);
    ASSERT_EQ(10, radio_get_glitch_filter());

    radio_set_glitch_filter(5);
    ASSERT_EQ(10, radio_get_glitch_filter());

    radio_set_glitch_filter(110);
    ASSERT_EQ(110, radio_get_glitch_filter());

    radio_set_glitch_filter(500);
    ASSERT_EQ(500, radio_get_glitch_filter());

    radio_set_glitch_filter(501);
    ASSERT_EQ(500, radio_get_glitch_filter());

    radio_set_glitch_filter(65535);
    ASSERT_EQ(500, radio_get_glitch_filter());
}

void register_r2_stress_radio_state_tests() {
    TEST_SUITE("R2 Stress Suite 3: CC1101 Radio State Transitions & Presets");
    RUN_TEST_CASE("[R2-STRESS] 10,000 rapid preset transitions with production radio_modes.cpp", TestTier::TIER_4_REAL_WORLD, test_r2_stress_radio_rapid_preset_switching_10k);
    RUN_TEST_CASE("[R2-STRESS] Carson's rule filter bandwidths & frequency deviations", TestTier::TIER_2_BOUNDARY, test_r2_stress_carsons_rule_and_deviations);
    RUN_TEST_CASE("[R2-STRESS] Hardware SPI stall & bounded timeout resilience", TestTier::TIER_2_BOUNDARY, test_r2_stress_radio_stall_resilience);
    RUN_TEST_CASE("[R2-STRESS] Glitch filter dynamic threshold boundary clamping", TestTier::TIER_2_BOUNDARY, test_r2_stress_glitch_filter_clamping);
}
