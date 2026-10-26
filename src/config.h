#pragma once
#include <Arduino.h>

// =========================================================================
// АППАРАТНАЯ КОНФИГУРАЦИЯ (ESP32-C3 SuperMini + OLED SSD1306 + CC1101)
// =========================================================================

// Дисплей OLED 0.96" I2C
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       64
#define OLED_RESET          -1
#define OLED_ADDR           0x3C

#define PIN_OLED_SDA        8
#define PIN_OLED_SCL        9

// Кнопки управления (INPUT_PULLUP к GND)
#define PIN_BTN_UP          0
#define PIN_BTN_ACTION      1
#define PIN_BTN_DOWN        10

// Радиомодуль CC1101 (SPI + GDO0)
#define CC_SCK              4
#define CC_MISO             5
#define CC_MOSI             6
#define CC_CS               7
#define CC_GDO0             3

// Мониторинг аккумулятора (Li-Po 3.7V через делитель 1:2 на вход АЦП ADC1_CH2)
#define PIN_BATTERY_ADC     2
#define BATTERY_DIVIDER_RATIO 2.0f

// =========================================================================
// ТАЙМИНГИ И ЛИМИТЫ
// =========================================================================
#define DEBOUNCE_TIME_MS    30
#define LONG_PRESS_TIME_MS  750

#define MAX_SAVED_SIGNALS   20
#define MAX_RAW_PULSES      192
#define SCREEN_TIMEOUT_MS   60000

#define NUM_FREQ_PRESETS    7
#define NUM_PA_LEVELS       4
#define NUM_SQUELCH_LEVELS  4
#define SPECTRUM_CHANNELS   32
#define NUM_SPECTRUM_BANDS  3

// Пресеты частот передачи/приема
const float FREQ_PRESETS[NUM_FREQ_PRESETS] = {
    433.920, 433.075, 433.420, 434.420, 434.775, 315.000, 868.350
};

const char* const FREQ_NAMES[NUM_FREQ_PRESETS] = {
    "433.92", "433.07", "433.42", "434.42", "434.77", "315.00", "868.35"
};

// Частоты для быстрого автопоиска (Частотомер)
#define NUM_FAST_SCAN_FREQS 7
const float FAST_SCAN_FREQS[NUM_FAST_SCAN_FREQS] = {
    433.920, 433.075, 433.420, 434.420, 434.775, 315.000, 868.350
};

// Уровни мощности передачи PA
const int PA_LEVELS[NUM_PA_LEVELS] = {-30, -10, 0, 10};
const char* const PA_NAMES[NUM_PA_LEVELS] = {"-30dB", "-10dB", "0dB", "+10dB"};

// Уровни чувствительности шумоподавителя RAW (Squelch)
const int SQUELCH_LEVELS[NUM_SQUELCH_LEVELS] = {-85, -78, -72, -65};
const char* const SQUELCH_NAMES[NUM_SQUELCH_LEVELS] = {"-85dB", "-78dB", "-72dB", "-65dB"};
