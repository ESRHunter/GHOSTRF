#include "radio_modes.h"

#if defined(ARDUINO) || defined(ESP32)
#include "config.h"
#include <SPI.h>
#include <ELECHOUSE_CC1101_SRC_DRV.h>
#endif

// =========================================================================
// RADIO STATE TRACKING
// =========================================================================

static RfModulation s_currentModulation = MOD_OOK_ASK;
static float        s_currentFreqMhz    = 433.92f;
static int          s_currentPaLevel    = 10;
static uint16_t     s_glitchFilterUs    = 110;

bool radio_modes_init() {
#if defined(ARDUINO) || defined(ESP32)
    s_currentModulation = MOD_OOK_ASK;
    s_currentFreqMhz = 433.92f;
    s_currentPaLevel = 10;
    s_glitchFilterUs = 110;
    return radio_set_preset(MOD_OOK_ASK, 433.92f, 10);
#else
    return true;
#endif
}

bool radio_safe_sidle() {
#if defined(ARDUINO) || defined(ESP32)
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    uint32_t start = millis();
    while ((millis() - start) < 20) {
        uint8_t marc = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
        if (marc == 0x01) { // MARCSTATE_IDLE
            ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
            ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
            return true;
        }
        delayMicroseconds(50);
    }
    // Fallback flush even on timeout
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFRX);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SFTX);
    return false;
#else
    return true;
#endif
}

bool radio_recalibrate_synth() {
#if defined(ARDUINO) || defined(ESP32)
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SIDLE);
    delayMicroseconds(100);
    ELECHOUSE_cc1101.SpiStrobe(CC1101_SCAL);
    uint32_t start = millis();
    while ((millis() - start) < 25) {
        uint8_t marc = ELECHOUSE_cc1101.SpiReadStatus(CC1101_MARCSTATE) & 0x1F;
        if (marc == 0x01) { // Calibrated and back to IDLE
            return true;
        }
        delayMicroseconds(50);
    }
    return false;
#else
    return true;
#endif
}

void radio_set_rx_bandwidth(float bwKhz) {
#if defined(ARDUINO) || defined(ESP32)
    uint8_t currentMdmcfg4 = ELECHOUSE_cc1101.SpiReadReg(CC1101_MDMCFG4) & 0x0F;
    if (bwKhz >= 500.0f) {
        // Carson rule bandwidth ~650 kHz (CHANBW_E = 0, CHANBW_M = 1) -> 0x10
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x10);
    } else if (bwKhz >= 200.0f) {
        // Carson rule bandwidth ~270 kHz (CHANBW_E = 1, CHANBW_M = 1) -> 0x50
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x50);
    } else {
        // Carson rule bandwidth ~135 kHz (CHANBW_E = 2, CHANBW_M = 0) -> 0x80
        ELECHOUSE_cc1101.SpiWriteReg(CC1101_MDMCFG4, currentMdmcfg4 | 0x80);
    }
#else
    (void)bwKhz;
#endif
}

void radio_set_glitch_filter(uint16_t minUs) {
    if (minUs < 10) minUs = 10;
    if (minUs > 500) minUs = 500;
    s_glitchFilterUs = minUs;
}

uint16_t radio_get_glitch_filter() {
    return s_glitchFilterUs;
}

RfModulation radio_get_current_modulation() {
    return s_currentModulation;
}

float radio_get_current_frequency() {
    return s_currentFreqMhz;
}

int radio_get_current_pa() {
    return s_currentPaLevel;
}

bool radio_set_preset(RfModulation mod, float freqMhz, int paLevel) {
#if defined(ARDUINO) || defined(ESP32)
    // 1. Safe state transition to IDLE with FIFO flush
    radio_safe_sidle();

    // 2. Set basic carrier frequency
    ELECHOUSE_cc1101.setMHZ(freqMhz);

    // 3. Force asynchronous serial mode on CC_GDO0
    ELECHOUSE_cc1101.setCCMode(false);
#endif

    // 4. Register reconfiguration per modulation with Carson's rule filter bandwidths
    switch (mod) {
        case MOD_OOK_ASK:
#if defined(ARDUINO) || defined(ESP32)
            ELECHOUSE_cc1101.setModulation(2); // ASK/OOK
            radio_set_rx_bandwidth(650.0f);
            ELECHOUSE_cc1101.setDeviation(0.0f);
#endif
            radio_set_glitch_filter(110);
            break;

        case MOD_2FSK:
#if defined(ARDUINO) || defined(ESP32)
            ELECHOUSE_cc1101.setModulation(0); // 2-FSK
            radio_set_rx_bandwidth(270.0f);
            ELECHOUSE_cc1101.setDeviation(47.6f);
#endif
            radio_set_glitch_filter(20);
            break;

        case MOD_GFSK:
#if defined(ARDUINO) || defined(ESP32)
            ELECHOUSE_cc1101.setModulation(1); // GFSK
            radio_set_rx_bandwidth(135.0f);
            ELECHOUSE_cc1101.setDeviation(38.0f);
#endif
            radio_set_glitch_filter(20);
            break;

        default:
#if defined(ARDUINO) || defined(ESP32)
            ELECHOUSE_cc1101.setModulation(2);
            radio_set_rx_bandwidth(650.0f);
            ELECHOUSE_cc1101.setDeviation(0.0f);
#endif
            radio_set_glitch_filter(110);
            break;
    }

#if defined(ARDUINO) || defined(ESP32)
    // 5. Configure PA power level
    ELECHOUSE_cc1101.setPA(paLevel);

    // 6. Recalibrate frequency synthesizer for target band
    radio_recalibrate_synth();
#endif

    // 7. Update active tracking variables
    s_currentModulation = mod;
    s_currentFreqMhz = freqMhz;
    s_currentPaLevel = paLevel;

    return true;
}
