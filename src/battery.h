#pragma once
#include <Arduino.h>

// =========================================================================
// МОДУЛЬ МОНИТОРИНГА ЗАРЯДА АККУМУЛЯТОРА (Li-Po 3.7V / 1S)
// =========================================================================

void battery_init();
void battery_update(); // Вызывать периодически в главном цикле
uint16_t battery_get_millivolts();
uint16_t battery_get_raw_adc_mv();
uint8_t  battery_get_percentage();
bool     battery_is_present();
bool     battery_is_charging();
bool     battery_is_overvoltage();
const char* battery_get_icon();
