#include "battery.h"
#include "config.h"
#include "driver/gpio.h"

static uint16_t currentMv = 0;
static uint16_t lastRawMv = 0;
static uint8_t currentPct = 0;
static unsigned long lastBatteryRead = 0;
static char batteryIcon[8] = "[USB]";

void battery_init() {
    gpio_reset_pin((gpio_num_t)PIN_BATTERY_ADC);
    gpio_set_pull_mode((gpio_num_t)PIN_BATTERY_ADC, GPIO_FLOATING);
    pinMode(PIN_BATTERY_ADC, INPUT);
    analogSetAttenuation(ADC_11db); // До 2.5 - 3.1 В на входе ADC
    battery_update();
}

static uint8_t calc_lipo_percentage(uint16_t mv) {
    if (mv >= 4180) return 100;
    if (mv >= 4050) return 90 + (uint8_t)((mv - 4050) * 10 / 130);
    if (mv >= 3950) return 75 + (uint8_t)((mv - 3950) * 15 / 100);
    if (mv >= 3850) return 55 + (uint8_t)((mv - 3850) * 20 / 100);
    if (mv >= 3750) return 35 + (uint8_t)((mv - 3750) * 20 / 100);
    if (mv >= 3650) return 15 + (uint8_t)((mv - 3650) * 20 / 100);
    if (mv >= 3500) return 5 + (uint8_t)((mv - 3500) * 10 / 150);
    if (mv >= 3350) return 1 + (uint8_t)((mv - 3350) * 4 / 150);
    return 0;
}

void battery_update() {
    unsigned long now = millis();
    if (lastBatteryRead != 0 && (now - lastBatteryRead < 1000)) {
        return;
    }
    lastBatteryRead = now;

    // Считываем 16 выборок для сглаживания шумов ADC
    uint32_t sumMv = 0;
    for (int i = 0; i < 16; i++) {
        sumMv += analogReadMilliVolts(PIN_BATTERY_ADC);
        delayMicroseconds(50);
    }
    lastRawMv = sumMv / 16;

    // С учетом делителя 1:2
    uint16_t measuredMv = (uint16_t)(lastRawMv * BATTERY_DIVIDER_RATIO);

    // Экспоненциальное сглаживание
    if (currentMv == 0) {
        currentMv = measuredMv;
    } else {
        currentMv = (currentMv * 7 + measuredMv * 3) / 10;
    }

    if (currentMv < 2000) {
        // Делитель не подключен или плата питается только от USB без батареи
        currentPct = 100;
        snprintf(batteryIcon, sizeof(batteryIcon), "[USB]");
    } else if (currentMv > 4380) {
        // Перегрузка АЦП / отсутствие делителя напряжения
        currentPct = 100;
        snprintf(batteryIcon, sizeof(batteryIcon), "[OV!]");
    } else {
        currentPct = calc_lipo_percentage(currentMv);
        if (currentMv >= 4220) {
            snprintf(batteryIcon, sizeof(batteryIcon), "[CHG]");
        } else if (currentPct > 70) {
            snprintf(batteryIcon, sizeof(batteryIcon), "[===]");
        } else if (currentPct > 35) {
            snprintf(batteryIcon, sizeof(batteryIcon), "[== ]");
        } else if (currentPct > 15) {
            snprintf(batteryIcon, sizeof(batteryIcon), "[=  ]");
        } else {
            snprintf(batteryIcon, sizeof(batteryIcon), "[!  ]");
        }
    }
}

uint16_t battery_get_millivolts() {
    return currentMv;
}

uint16_t battery_get_raw_adc_mv() {
    return lastRawMv;
}

uint8_t battery_get_percentage() {
    return currentPct;
}

bool battery_is_present() {
    return (currentMv >= 2000);
}

bool battery_is_charging() {
    return (currentMv >= 4220 && currentMv <= 4380);
}

bool battery_is_overvoltage() {
    return (currentMv > 4380);
}

const char* battery_get_icon() {
    return batteryIcon;
}
