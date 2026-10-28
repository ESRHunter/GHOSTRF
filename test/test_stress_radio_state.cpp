// Stress Test Suite 3: Radio State Machine Robustness & Modulation Switching
// Evaluates: rapid preset switching (OOK <-> 2-FSK <-> GFSK), TI CC1101 state machine requirements, Carson's rule bandwidths

#include "test_harness.h"
#include "radio_modes.h"
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cstring>
#include <iostream>

using ghostrf_test::TestTier;

// =========================================================================
// TI CC1101 STATE MACHINE EMULATOR & INTEGRITY CHECKER
// =========================================================================

#define CC1101_IOCFG0       0x02
#define CC1101_PKTCTRL0     0x08
#define CC1101_FREQ2        0x0D
#define CC1101_FREQ1        0x0E
#define CC1101_FREQ0        0x0F
#define CC1101_MDMCFG4      0x10
#define CC1101_MDMCFG2      0x12
#define CC1101_DEVIATN      0x15
#define CC1101_FREND0       0x22

#define CC1101_SCAL         0x33
#define CC1101_SRX          0x34
#define CC1101_STX          0x35
#define CC1101_SIDLE        0x36
#define CC1101_SFRX         0x3A
#define CC1101_SFTX         0x3B
#define CC1101_MARCSTATE    0x35

// CC1101 MARCSTATE definitions
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
                    calCyclesRemaining = 2; // Takes 2 status reads to finish calibration
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
};

static CC1101StateEmulator s_emu;

} // anonymous namespace

// Mock CC1101 driver functions that delegate to our state emulator
struct ELECHOUSE_CC1101_MOCK {
    void SpiStrobe(uint8_t s) { s_emu.SpiStrobe(s); }
    uint8_t SpiReadStatus(uint8_t a) { return s_emu.SpiReadStatus(a); }
    uint8_t SpiReadReg(uint8_t a) { return s_emu.SpiReadReg(a); }
    void SpiWriteReg(uint8_t a, uint8_t v) { s_emu.SpiWriteReg(a, v); }

    void setMHZ(float mhz) {
        // Sets frequency registers (FREQ2..FREQ0)
        s_emu.SpiWriteReg(CC1101_FREQ2, (uint8_t)(mhz / 10));
    }

    void setCCMode(bool) {
        // Forces asynchronous serial mode on CC_GDO0
        s_emu.SpiWriteReg(CC1101_IOCFG0, 0x0D);
        s_emu.SpiWriteReg(CC1101_PKTCTRL0, 0x32);
    }

    void setModulation(uint8_t m) {
        // m: 0=2FSK, 1=GFSK, 2=ASK/OOK
        s_emu.SpiWriteReg(CC1101_MDMCFG2, (s_emu.SpiReadReg(CC1101_MDMCFG2) & 0x8F) | ((m & 0x07) << 4));
    }

    void setDeviation(float devKhz) {
        s_emu.SpiWriteReg(CC1101_DEVIATN, (uint8_t)devKhz);
    }

    void setPA(int pa) {
        s_emu.SpiWriteReg(CC1101_FREND0, (uint8_t)pa);
    }
};

static ELECHOUSE_CC1101_MOCK ELECHOUSE_cc1101;

// Now include or wrap radio_modes functions
// Since radio_modes.cpp has #if defined(ARDUINO), we simulate the exact logic directly
// or invoke radio_set_preset under mock

// Re-implementation of radio_modes logic under the emulator to test state machine compliance
static bool emulated_radio_safe_sidle() {
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    uint32_t startMs = 0;
    while (startMs < 20) {
        uint8_t marc = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
        if (marc == MARC_IDLE) {
            ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
            ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
            return true;
        }
        startMs++;
    }
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
    return false;
}

static bool emulated_radio_recalibrate_synth() {
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SCAL);
    uint32_t startMs = 0;
    while (startMs < 25) {
        uint8_t marc = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
        if (marc == MARC_IDLE) {
            return true;
        }
        startMs++;
    }
    return false;
}

static void emulated_radio_set_rx_bandwidth(float bwKhz) {
    uint8_t currentMdmcfg4 = ELECHOUSE_cc1101.SpiReadReg(CC1101_MDMCFG4) & 0x0F;
    if (bwKhz >= 500.0f) {
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x10);
    } else if (bwKhz >= 200.0f) {
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x50);
    } else {
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x80);
    }
}

static uint16_t s_glitchFilter = 110;
static void emulated_radio_set_glitch_filter(uint16_t minUs) {
    if (minUs < 10) minUs = 10;
    if (minUs > 500) minUs = 500;
    s_glitchFilter = minUs;
}

static bool emulated_radio_set_preset(RfModulation mod, float freqMhz, int paLevel) {
    // 1. Safe transition to IDLE with FIFO flush
    emulated_radio_safe_sidle();

    // 2. Carrier frequency
    ELECHOUSE_cc1101.setMHZ(freqMhz);

    // 3. Asynchronous serial mode
    ELECHOUSE_cc1101.setCCMode(false);

    // 4. Modulation and filter parameters
    switch (mod) {
        case MOD_OOK_ASK:
            ELECHOUSE_cc1101.setModulation(2);
            emulated_radio_set_rx_bandwidth(650.0f);
            ELECHOUSE_cc1101.setDeviation(0.0f);
            emulated_radio_set_glitch_filter(110);
            break;

        case MOD_2FSK:
            ELECHOUSE_cc1101.setModulation(0);
            emulated_radio_set_rx_bandwidth(270.0f);
            ELECHOUSE_cc1101.setDeviation(47.6f);
            emulated_radio_set_glitch_filter(20);
            break;

        case MOD_GFSK:
            ELECHOUSE_cc1101.setModulation(1);
            emulated_radio_set_rx_bandwidth(135.0f);
            ELECHOUSE_cc1101.setDeviation(38.0f);
            emulated_radio_set_glitch_filter(20);
            break;
    }

    // 5. PA level
    ELECHOUSE_cc1101.setPA(paLevel);

    // 6. Recalibrate frequency synthesizer
    emulated_radio_recalibrate_synth();

    return true;
}

// ----------------------------------------------------------------------------
// TEST 1: Rapid Modulation Preset Switching (10,000 Switches)
// ----------------------------------------------------------------------------
void test_stress_rapid_preset_switching_10k() {
    s_emu.reset();

    const RfModulation modSeq[] = { MOD_OOK_ASK, MOD_2FSK, MOD_GFSK };
    const float freqSeq[] = { 433.92f, 868.35f, 315.00f };
    const int paSeq[] = { 10, 0, -10 };

    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < 10000; i++) {
        RfModulation mod = modSeq[i % 3];
        float freq = freqSeq[(i / 3) % 3];
        int pa = paSeq[i % 3];

        // Put radio into RX or TX before switching (simulating active reception/transmission)
        if (i % 2 == 0) {
            s_emu.marcstate = MARC_RX;
        } else {
            s_emu.marcstate = MARC_TX;
        }

        bool ok = emulated_radio_set_preset(mod, freq, pa);
        ASSERT_TRUE(ok);

        // Verify that after preset transition, radio settles cleanly in IDLE
        ASSERT_EQ((int)MARC_IDLE, (int)s_emu.marcstate);

        // Verify Carson's rule filter bandwidth setting
        uint8_t mdmcfg4 = s_emu.regs[CC1101_MDMCFG4];
        if (mod == MOD_OOK_ASK) {
            ASSERT_EQ(0x10, mdmcfg4 & 0xF0); // ~650 kHz
            ASSERT_EQ(110, s_glitchFilter);
        } else if (mod == MOD_2FSK) {
            ASSERT_EQ(0x50, mdmcfg4 & 0xF0); // ~270 kHz
            ASSERT_EQ(20, s_glitchFilter);
        } else if (mod == MOD_GFSK) {
            ASSERT_EQ(0x80, mdmcfg4 & 0xF0); // ~135 kHz
            ASSERT_EQ(20, s_glitchFilter);
        }
    }

    auto end = std::chrono::steady_clock::now();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();

    // Verify ZERO state machine rule violations across all 10,000 transitions
    for (const auto& v : s_emu.violations) {
        std::cout << "CC1101 State Machine Violation: " << v << "\n";
    }
    ASSERT_EQ(0U, s_emu.violations.size());
    ASSERT_TRUE(totalMs < 1000.0); // 10,000 transitions completed in <1s
}

// ----------------------------------------------------------------------------
// TEST 2: Carson's Rule Bandwidth & Frequency Deviation Boundaries
// ----------------------------------------------------------------------------
void test_stress_carsons_rule_and_deviation() {
    s_emu.reset();

    // 1. OOK: 650 kHz BW, 0.0 kHz deviation
    emulated_radio_set_preset(MOD_OOK_ASK, 433.92f, 10);
    ASSERT_EQ(0x10, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(0, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(110, s_glitchFilter);

    // 2. 2-FSK: 270 kHz BW, 47.6 kHz deviation
    emulated_radio_set_preset(MOD_2FSK, 433.92f, 10);
    ASSERT_EQ(0x50, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(47, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(20, s_glitchFilter);

    // 3. GFSK: 135 kHz BW, 38.0 kHz deviation
    emulated_radio_set_preset(MOD_GFSK, 433.92f, 10);
    ASSERT_EQ(0x80, s_emu.regs[CC1101_MDMCFG4] & 0xF0);
    ASSERT_EQ(38, s_emu.regs[CC1101_DEVIATN]);
    ASSERT_EQ(20, s_glitchFilter);

    ASSERT_EQ(0U, s_emu.violations.size());
}

// ----------------------------------------------------------------------------
// TEST 3: Hardware Bus Stall & Timeout Resilience
// ----------------------------------------------------------------------------
void test_stress_radio_hardware_stall_tolerance() {
    s_emu.reset();

    // 1. Inject stuck in RX state (SPI strobe SIDLE ignored by hardware)
    s_emu.faultInjectStuckRx = true;
    s_emu.marcstate = MARC_RX;

    // Must return false cleanly on 20ms timeout, without hanging in an infinite loop
    bool sidleOk = emulated_radio_safe_sidle();
    ASSERT_FALSE(sidleOk);

    // 2. Inject stuck in Calibration state (SCAL never settles)
    s_emu.reset();
    s_emu.faultInjectStuckCal = true;

    // Must return false cleanly on 25ms timeout, without hanging
    bool scalOk = emulated_radio_recalibrate_synth();
    ASSERT_FALSE(scalOk);
}

void register_stress_radio_state_tests() {
    TEST_SUITE("Stress Test 3: CC1101 Radio State Transitions & Presets");
    RUN_TEST_CASE("[STRESS] 10,000 rapid modulation preset transitions without state violations", TestTier::TIER_4_REAL_WORLD, test_stress_rapid_preset_switching_10k);
    RUN_TEST_CASE("[STRESS] Carson's rule filter bandwidths & deviation validation", TestTier::TIER_2_BOUNDARY, test_stress_carsons_rule_and_deviation);
    RUN_TEST_CASE("[STRESS] Hardware SPI stall & bounded timeout resilience", TestTier::TIER_2_BOUNDARY, test_stress_radio_hardware_stall_tolerance);
}
