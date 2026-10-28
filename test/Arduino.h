#pragma once
// Lightweight Arduino compatibility shim for host compilation of GHOSTRF units.

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include "cli_mock_env.h"

typedef uint8_t byte;
typedef bool boolean;

#ifndef HIGH
#define HIGH 0x1
#endif

#ifndef LOW
#define LOW 0x0
#endif

#ifndef INPUT
#define INPUT 0x0
#endif

#ifndef OUTPUT
#define OUTPUT 0x1
#endif

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#endif

#ifndef F
#define F(s) (s)
#endif

inline unsigned long millis() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration_cast<milliseconds>(steady_clock::now() - start).count();
}

inline unsigned long micros() {
    using namespace std::chrono;
    static auto start = steady_clock::now();
    return duration_cast<microseconds>(steady_clock::now() - start).count();
}

inline void delay(unsigned long ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

inline void delayMicroseconds(unsigned int us) {
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void noInterrupts() {}
inline void interrupts() {}
inline void yield() {}
