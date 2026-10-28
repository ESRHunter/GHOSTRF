# Contributing to GHOSTRF

Thank you for your interest in contributing to **GHOSTRF**! We welcome contributions to firmware, protocols, documentation, desktop/mobile apps, and web tooling.

---

## 🛠 Development Workflow

### 1. Embedded Firmware (ESP32-C3)
* The firmware is built with **PlatformIO**:
  ```bash
  pio run -e esp32-c3-supermini
  ```
* Flashing connected hardware:
  ```bash
  pio run -t upload
  ```
* Non-blocking architecture rule: Do NOT use `delay()` or busy-waiting `while()` loops inside `src/`. All operations must use the non-blocking state machine or cooperative timers.

### 2. Host C++ Test Suite (Zero Hardware Required)
* Before submitting a pull request, run the host test suite:
  ```bash
  g++ -std=c++17 -Wall -Wextra -Itest -Isrc \
    test/test_main.cpp test/test_sub_format.cpp test/test_keeloq.cpp test/test_telemetry.cpp \
    src/sub_format.cpp src/keeloq.cpp src/keeloq_analyzer.cpp src/sensor_telemetry.cpp \
    -o run_tests && ./run_tests && rm -f run_tests
  ```

### 3. Documentation & Web Assets Integrity
* Verify all markdown links and web assets:
  ```bash
  python3 test/verify_docs_and_web.py
  ```
  All 55 checks must pass with exit code `0`.

---

## 📜 Pull Request Guidelines

1. **Keep it atomic:** One feature or fix per pull request.
2. **Bilingual documentation:** If updating specifications or firmware modes, update both `README.md` and `README_RU.md`.
3. **Brutalist web aesthetics:** If updating `web/`, maintain `#000000`/`#FFFFFF` high contrast and `border-radius: 0px`.

---

## ⚖ License
By contributing to GHOSTRF, you agree that your contributions will be licensed under the **GNU General Public License v3.0 (GPL-3.0)**.
