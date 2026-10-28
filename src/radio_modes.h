#pragma once

#if defined(ARDUINO) || defined(ESP32)
#include <Arduino.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif

// =========================================================================
// RUNTIME MODULATION CONTROL & HARDWARE PRESETS
// =========================================================================

enum RfModulation {
    MOD_OOK_ASK = 0,
    MOD_2FSK    = 1,
    MOD_GFSK    = 2
};

struct RfPresetConfig {
    const char*   name;
    RfModulation  modulation;
    float         freqMhz;
    float         rxBwKhz;       // e.g. 650.0 for OOK, 270.0 for 2-FSK, 135.0 for GFSK
    float         dataRateKbps;  // e.g. 4.8, 17.24, 19.2
    float         deviationKhz;  // e.g. 0.0 for OOK, 47.6 for 2-FSK, 38.0 for GFSK
    uint16_t      glitchFilterUs;// e.g. 110 for OOK, 20 for FSK/GFSK
};

// Safe radio state machine controls
bool radio_modes_init();
bool radio_safe_sidle();
bool radio_recalibrate_synth();

// Runtime modulation & preset switching
bool radio_set_preset(RfModulation mod, float freqMhz, int paLevel);
void radio_set_rx_bandwidth(float bwKhz);
void radio_set_glitch_filter(uint16_t minUs);
uint16_t radio_get_glitch_filter();

RfModulation radio_get_current_modulation();
float radio_get_current_frequency();
int radio_get_current_pa();
