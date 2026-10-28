```
╔═══════════════════════════════════════════════════════════════════════════════════════════════╗
║    ____ _   _  ___  ____ _____ ____  _____   [ POCKET SUB-GHZ & 2.4GHZ MULTI-TOOL ]           ║
║   / ___| | | |/ _ \/ ___|_   _|  _ \|  ___|  ESP32-C3 RISC-V + TI CC1101 + OLED 0.96"         ║
║  | |  _| |_| | | | \___ \ | | | |_) | |_     SUB-GHZ TRANSCEIVER & 2.4GHZ WIRELESS LAB        ║
║  | |_| |  _  | |_| |___) || | |  _ <|  _|    100% OPEN-SOURCE HARDWARE & FIRMWARE             ║
║   \____|_| |_|\___/|____/ |_| |_| \_\_|      TOTAL BUILD COST: < $10 USD                      ║
╚═══════════════════════════════════════════════════════════════════════════════════════════════╝
```

<p align="center">
  <a href="https://platformio.org/"><img src="https://img.shields.io/badge/PlatformIO-ESP32--C3-orange.svg?style=flat-square&logo=platformio" alt="PlatformIO"></a>
  <a href="https://www.espressif.com/"><img src="https://img.shields.io/badge/MCU-ESP32--C3%20160MHz%20RISC--V-red.svg?style=flat-square&logo=espressif" alt="MCU ESP32-C3"></a>
  <a href="https://www.ti.com/product/CC1101"><img src="https://img.shields.io/badge/Transceiver-TI%20CC1101%20SPI-blue.svg?style=flat-square" alt="TI CC1101"></a>
  <img src="https://img.shields.io/badge/Sub--GHz-300--928%20MHz-green.svg?style=flat-square" alt="Sub-GHz 300-928MHz">
  <img src="https://img.shields.io/badge/2.4GHz-Wi--Fi%20%2B%20BLE%205.0-lightgrey.svg?style=flat-square" alt="2.4GHz Wi-Fi and BLE 5.0">
  <a href="web/index.html"><img src="https://img.shields.io/badge/WebSerial-1--Click%20Web%20Flasher-success.svg?style=flat-square&logo=googlechrome" alt="WebSerial Flasher"></a>
  <img src="https://img.shields.io/badge/Release-v1.0.0-blueviolet.svg?style=flat-square" alt="Release v1.0.0">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-yellow.svg?style=flat-square" alt="License GPL-3.0"></a>
</p>

<p align="center">
  🌐 <strong>Language:</strong> <strong>English</strong> | <a href="README_RU.md">Русская версия (README_RU.md)</a>
</p>

---

## 🎬 Official Video Showcase

<p align="center">
  <a href="GHOSTRF_Official_Promo.mp4">
    <img src="web/ghostrf_demo.gif" alt="GHOSTRF Live Firmware & 32-Ch Spectrum Demo" width="100%">
  </a>
</p>
<p align="center">
  <video src="GHOSTRF_Official_Promo.mp4" width="100%" controls poster="web/ghostrf_user_parts_annotated.jpg">
    <a href="GHOSTRF_Official_Promo.mp4">
      <img src="web/ghostrf_user_parts_annotated.jpg" alt="GHOSTRF Official Video Showcase" width="100%">
    </a>
  </video>
</p>
<p align="center">
  <em>▶ <strong>Watch the Official Promo Video (26.5s)</strong>: Live Sub-GHz pulse capturing, KeeLoq rolling code decoding, real-time 32-channel RSSI spectrum with peak hold, 2.4GHz Wi-Fi/BLE radar, and cross-platform desktop/mobile streaming. <a href="GHOSTRF_Official_Promo.mp4"><strong>[ Download Full 1080p MP4 • 2.8 MB ]</strong></a></em>
</p>

---

## ⚡ What is GHOSTRF?

**GHOSTRF** is an ultra-compact, open-source pocket multi-tool for Sub-GHz (300–928 MHz) and 2.4 GHz RF exploration, security research, and protocol reverse engineering. Powered by an **Espressif ESP32-C3** 160 MHz 32-bit RISC-V SoC and a **Texas Instruments CC1101** SPI transceiver, GHOSTRF delivers the capabilities of dedicated commercial RF appliances inside a matchbox-sized form factor for **under $10 USD**.

Unlike single-band receivers, GHOSTRF combines raw microsecond Sub-GHz pulse capture with native 2.4 GHz Wi-Fi and Bluetooth LE analysis, on-device digital oscilloscope visualization on a 0.96" OLED display, bidirectional **Flipper Zero `.sub` file compatibility** on LittleFS flash storage, and a cross-platform desktop & mobile software suite.

### 💰 Itemized Bill of Materials (< $9.00 USD)

| Component | Technical Description | Est. Cost (USD) | Source / Notes |
|---|---|---|---|
| **ESP32-C3 SuperMini** | 160 MHz RISC-V core, 320 KB SRAM, 4 MB Flash, USB-C native CDC/JTAG, Wi-Fi 802.11b/g/n + BLE 5.0 | $2.20 | AliExpress / Amazon |
| **Texas Instruments CC1101** | Sub-1GHz RF transceiver (300–928 MHz), SPI bus, SMA antenna interface | $2.80 | Green module + spring/SMA antenna |
| **0.96" I2C Monochrome OLED** | SSD1306 controller, 128x64 resolution, 3.3V, I2C address `0x3C` | $1.40 | 4-pin I2C module (VCC, GND, SCL, SDA) |
| **TP4056 USB-C Charger** | 1S 3.7V Li-Po charger module with overcharge & over-discharge protection | $0.40 | Type-C charging port |
| **Li-Po 1S Battery** | 3.7V 400–600 mAh rechargeable pouch cell (fits 3D matchbox enclosure) | $1.80 | 1S Li-Po with protection board |
| **Tactile Buttons & Resistors** | 3x 6x6 mm tactile switches + 2x 100 kΩ 1% metal film divider resistors | $0.30 | Hardware pull-ups and battery divider |
| **Total Build Cost** | **Complete pocket Sub-GHz & 2.4GHz multi-tool** | **~$8.90 USD** | **Under $10 total build budget** |

### 📊 Capability Comparison

| Feature | GHOSTRF | Flipper Zero | HackRF One | Generic ESP8266 Tool |
|---|---|---|---|---|
| **Retail / Build Price** | **< $10 USD** | $169 USD | $300+ USD | ~$5 USD |
| **Sub-GHz Transceiver** | **TI CC1101 (300–928 MHz)** | TI CC1101 (300–928 MHz) | MAX2837 (1 MHz–6 GHz SDR) | None (2.4 GHz only) |
| **Sub-GHz Modulations** | **OOK / ASK / 2-FSK / GFSK** | OOK / ASK / 2-FSK / 4-FSK | Any (Software Demod) | None |
| **2.4 GHz Tools** | **Wi-Fi Deauth Mon + BLE Radar** | None (Requires addon board) | Wideband SDR (No native BLE stack) | 802.11b/g/n deauth only |
| **Storage & Flipper Files** | **Native LittleFS `.sub` support** | Native SD card `.sub` | Host PC required | None |
| **Display** | **0.96" OLED SSD1306 (128x64)** | 1.4" Monochrome LCD (128x64) | None (Requires Host PC) | Varies / None |
| **On-Screen Oscilloscope** | **Yes (Micro-oscilloscope)** | Yes (Sub-GHz RAW) | External (Inspectrum/GQRX) | None |
| **Desktop Suite** | **Qt 6 C++ (Linux & Windows)** | qFlipper | SDR# / GNU Radio / GQRX | Web / Raw Serial |
| **Mobile App** | **Android APK + Web BLE PWA** | Android / iOS App | None | Captive portal |
| **Web Flasher** | **1-Click WebSerial In-Browser** | Web Updater | DFU CLI | esptool.py CLI |

---

## 🔌 Hardware Specifications & Pinout

```
                         ESP32-C3 SuperMini Pinout
                               ┌─────────┐
                   5V (Power) ─┤ 1    16 ├─ 3.3V (VDD Out -> OLED & CC1101)
                 GND (Ground) ─┤ 2    15 ├─ GPIO 10 (Button DOWN)
         GPIO 0 (Button UP)   ─┤ 3    14 ├─ GPIO 9  (OLED SCL)
         GPIO 1 (Button OK)   ─┤ 4    13 ├─ GPIO 8  (OLED SDA)
         GPIO 2 (Battery ADC) ─┤ 5    12 ├─ GPIO 7  (CC1101 CSN)
         GPIO 3 (CC1101 GDO0) ─┤ 6    11 ├─ GPIO 6  (CC1101 MOSI)
         GPIO 4 (CC1101 SCK)  ─┤ 7    10 ├─ GPIO 5  (CC1101 MISO)
         GPIO 20 (UART0 TX/Ex)─┤ 8     9 ├─ GPIO 21 (UART0 RX/Ex)
                               └─────────┘
```

### Complete Pin Connection Matrix

| Component | Module Pin | ESP32-C3 Pin | Mode / Bus | Technical Function & Description |
|---|---|---|---|---|
| **Power Supply** | VCC (OLED & CC1101) | **3.3V** | VDD Output | Regulated 3.3V rail. **⚠️ DANGER: NEVER connect CC1101 to 5V! 5V will destroy the CC1101.** |
| **Power Supply** | GND (Common) | **GND** | Ground | Common system ground plane |
| **OLED Display** | **SDA** | **GPIO 8** | Hardware I2C | SSD1306 I2C Serial Data (`0x3C` I2C address) |
| **OLED Display** | **SCL** | **GPIO 9** | Hardware I2C | SSD1306 I2C Serial Clock line (up to 400 kHz) |
| **CC1101 Radio** | **SCK / CLK** | **GPIO 4** | Hardware SPI | SPI Clock line (up to 10 MHz) |
| **CC1101 Radio** | **MISO / SO** | **GPIO 5** | Hardware SPI | SPI Master In Slave Out (Data from CC1101 to ESP32) |
| **CC1101 Radio** | **MOSI / SI** | **GPIO 6** | Hardware SPI | SPI Master Out Slave In (Data from ESP32 to CC1101) |
| **CC1101 Radio** | **CSN / SS** | **GPIO 7** | Output (Active LOW) | CC1101 Chip Select line |
| **CC1101 Radio** | **GDO0** | **GPIO 3** | Digital I/O (ISR) | Edge interrupt capture for microsecond pulse demodulation & TX pulse modulation |
| **Button UP** | Key 1 -> GND | **GPIO 0** | INPUT_PULLUP | Navigation UP / Value increment / Instant Replay TX |
| **Button OK** | Key 2 -> GND | **GPIO 1** | INPUT_PULLUP | Short press: Select / Action / Save; Long press (>0.75s): Global Main Menu |
| **Button DOWN** | Key 3 -> GND | **GPIO 10** | INPUT_PULLUP | Navigation DOWN / Value decrement / Discard packet |
| **Battery Monitor**| 100k + 100k Divider | **GPIO 2** | ADC1_CH2 | 0–4.2V Li-Po voltage sensing via 16x oversampling ADC (2.0x ratio) |
| **Expansion Port**| Free Header Pins | **GPIO 20, 21** | GPIO / UART0 | Uncommitted pins reserved for external GPS, sensors, or haptic buzzer |
| **USB CDC Port** | USB-C connector | **GPIO 18, 19** | Native USB CDC | High-speed Serial CLI, display frame streaming, and WebSerial flashing |
| **Battery Charger**| TP4056 OUT+ / OUT- | **5V & GND** | Power In | USB-C 1S Li-Po charging with automatic cut-off |

### ⚡ Critical Electrical Warnings
1. **CC1101 Transceiver Voltage**: The CC1101 transceiver chip has an absolute maximum VDD rating of **3.6V**. It **MUST** be powered from the ESP32-C3 onboard 3.3V LDO regulator pin. Supplying 5V to the CC1101 will cause instantaneous dielectric breakdown and permanent destruction of the RF core.
2. **Battery ADC Voltage Divider**: The internal ESP32-C3 ADC has a maximum input limit of 3.3V (2.5V recommended with attenuation). The Li-Po battery connects to `GPIO 2` through two precision 100 kΩ resistors in series (1:2 divider ratio), safely halving a 4.2V fully charged battery down to 2.1V.

<p align="center">
  <img src="web/ghostrf_wiring_diagram.png" alt="GHOSTRF Complete Schematic and Wiring Diagram" width="85%">
</p>

---

## 🕹️ Deep Breakdown of All 9 Firmware Modes

GHOSTRF features a deterministic, non-blocking finite state machine (FSM) architecture. Any active mode can be exited instantly by **holding the OK button for > 0.75 seconds**, safely terminating radio transfers and returning to the Main Menu.

```
┌────────────────────────────────────────────────────────┐
│ [===] 4.14V      GHOSTRF v1.0.0               [BLE ON] │
├────────────────────────────────────────────────────────┤
│ > 1. Remote (TX)             6. Carrier / Jam          │
│   2. Sniffer (RX)            7. 2.4GHz Tools           │
│   3. RAW Sniffer             8. Battery Info           │
│   4. Freq Analyzer           9. Settings               │
│   5. RSSI Spectrum                                     │
└────────────────────────────────────────────────────────┘
```

### Mode 1: Remote Control (TX) & Signal Library
- **Architecture**: `src/main.cpp`, `src/storage.cpp`, `src/sub_storage.cpp`
- **Signal Capacity**: Stores up to 20 fast-access signals in Flash NVS Preferences and up to 64 structured signals on the 896 KB LittleFS flash partition.
- **Interactive Action Sub-Menu**: Selecting any signal presents a dedicated operational menu:
  1. `Transmit (TX)`: Transmits the encoded radio signal with real-time on-screen confirmation `>>> TRANSMITTING <<<`.
  2. `Repeats`: Cycle transmit repetition count (1x, 3x, 5x, 10x) to ensure reliable receiver triggering.
  3. `Edit Signal`: Modify frequency, protocol, pulse timings, or randomize address bits directly on the device.
  4. `Waveform`: Inspect the microsecond digital pulse train on the embedded micro-oscilloscope before transmitting.
  5. `Delete Signal`: Permanently remove the signal and re-index storage.
- **Smart EV1527 Button Cycling**: For 24-bit learning code remotes, GHOSTRF dynamically cycles button masks (`Btn 1` -> `Btn 2` -> `Btn 3` -> `Btn 4` -> `ALL`), allowing complete multi-channel relay testing from a single captured remote.
- **Supported Encoders**: Princeton PT2262/PT2240, EV1527, Came 12/24-bit, Nice Flo 12-bit, Linear 10-bit, Somfy RTS 56-bit, DoorHan, KeeLoq HCS, and arbitrary signed microsecond RAW pulse streams.

### Mode 2: Fixed-Code Receiver & Sniffer (RX)
- **Architecture**: `src/main.cpp`, `src/radio.cpp`, `rc-switch`
- **Operation**: Continuous non-blocking monitoring of OOK/ASK modulation at 433.92 MHz (or any configured preset) featuring an active radar animation.
- **Packet Extraction**: Automatically detects and decodes incoming fixed-code transmissions, displaying:
  - **HEX Code** (e.g., `0x547A11`), **DEC Code**, bit length (e.g., 24-bit), protocol ID, and pulse timing ($\mu s$).
  - **Tri-State Decoding**: Automatically evaluates PT2262 ternary address states (`0`, `1`, `F` float), identifying physical DIP switch positions for wireless relays and garage doors.
- **Action Shortcuts**:
  - `[OK]`: Saves signal to Flash with smart automatic nomenclature (`Socket #1`, `Gate #1`, `Smart #1`).
  - `[UP]`: Instant Replay TX test — re-transmits the captured code immediately into the ether.
  - `[DOWN]`: Discard current packet and resume scanning.
  - Broadcasts structured telemetry across USB CDC Serial and Bluetooth LE Nordic UART Service in real time.

### Mode 3: RAW Edge Sniffer & KeeLoq Dynamic Decoder
- **Architecture**: `src/main.cpp`, `src/radio.cpp`, `src/keeloq.cpp`, `src/keeloq_analyzer.cpp`, `src/sub_format.cpp`
- **Microsecond ISR Edge Capture**: Direct hardware interrupt capture via the CC1101 `GDO0` pin, recording pulse widths with 1 $\mu s$ resolution.
- **Adaptive Squelch Threshold**: Real-time noise gate (-85 dBm, -78 dBm default, -72 dBm, -65 dBm) with live on-screen RSSI bar and vertical squelch marker, eliminating false triggers from atmospheric RF noise.
- **KeeLoq Dynamic Code Recognition (HCS200 / HCS300 / HCS301)**:
  - Automatically isolates KeeLoq preamble and extracts the unencrypted 28-bit Serial Number (`0x...`).
  - Decodes the 4-bit Button Code (`0..15`).
  - Decodes the 32-bit encrypted hopping code.
  - Evaluates counter synchronization and flags potential replay attacks.
- **FSK & GFSK Environmental & TPMS Telemetry**:
  - Demodulates 2-FSK and GFSK sensor transmissions at 433.92 MHz and 868.35 MHz.
  - Extracts temperature, humidity, and battery health from Oregon Scientific v2.1/v3.0 weather stations.
  - Parses tire pressure (PSI/bar) and sensor ID from automotive TPMS sensors (Toyota PMV-C210, Ford Continental VDO, Schrader).
- **On-Screen Micro-Oscilloscope**:
  - High-contrast graphical waveform rendering on the 128x64 OLED display showing exact high/low pulse duty cycles and preamble structure.
  - Direct export of captured raw pulse trains into LittleFS as standard Flipper Zero `.sub` files.

### Mode 4: Frequency Analyzer & Auto-Detection
- **Architecture**: `src/main.cpp`, `src/radio.cpp`
- **Multi-Frequency Sweep**: Rapidly sweeps 7 ISM frequency presets (433.92, 433.07, 433.42, 434.42, 434.77, 315.00, 868.35 MHz) every 90 ms.
- **Carrier Lock**: Continuously evaluates RSSI against a high-confidence threshold ($\ge -65\text{ dBm}$). When an active transmission is detected, the display locks with `[LOCKED]` and reports the exact peak RSSI in dBm.
- **Auto-Tune Handoff**: Pressing `[OK]` locks the detected frequency, updates active transceiver parameters, and immediately transitions into RAW Sniffer mode tuned to the captured transmission.

### Mode 5: 32-Channel RSSI Panoramic Spectrum Analyzer
- **Architecture**: `src/main.cpp`, `src/ui.cpp`, `src/radio.cpp`
- **Panoramic 32-Channel Bargraph**: Real-time panoramic spectrum display across three primary frequency bands:
  - **433 MHz Band**: 433.05 MHz – 434.80 MHz (sub-GHz ISM)
  - **315 MHz Band**: 314.00 MHz – 316.00 MHz (automotive / North American remotes)
  - **868 MHz Band**: 867.50 MHz – 869.50 MHz (European security / smart meters)
- **Peak Hold with Decaying Markers**: Floating horizontal peak markers maintain peak signal levels with exponential decay, catching transient frequency-hopping and burst transmitters.
- **Interactive Controls**: `[UP] / [DOWN]` switches frequency bands instantly; `[OK]` clears peak hold markers; long hold `[OK]` returns to Main Menu.

### Mode 6: CW Carrier & Jamming Generator
- **Architecture**: `src/main.cpp`, `src/radio.cpp`
- **RF Signal Synthesis**: Generates continuous RF signals across any supported frequency (300–928 MHz) with selectable output power (-30 dBm to +10 dBm).
- **3 Operating Modes**:
  1. `CW` (Continuous Wave): Pure unmodulated RF carrier for antenna alignment, SWR analysis, and receiver calibration.
  2. `Tone 1 kHz`: 1 kHz square-wave audio modulation audible on analog receivers, transceivers, and SDR waterfall displays.
  3. `Jam Noise`: High-speed pseudo-random pulse burst generator for RF receiver desensitization and resilience testing.
- **Safety Interlock**: Explicit user activation required (`[OK]` toggle) with prominent on-screen transmission warning indicators.

### Mode 7: 2.4 GHz Wi-Fi & Bluetooth LE Toolkit
- **Architecture**: `src/main.cpp`, `src/wifi_tools.cpp`
- **Hardware Native 2.4 GHz Subsystem**: Leverages the ESP32-C3 internal 2.4 GHz radio core without requiring secondary coprocessors:
  1. `Wi-Fi Scanner`: Scans 802.11b/g/n access points, presenting SSID, RSSI, channel, encryption type (Open, WEP, WPA, WPA2, WPA3), and BSSID MAC address.
  2. `Wi-Fi Channels`: 13-channel spectrum histogram illustrating channel congestion, co-channel interference, and peak RSSI distribution.
  3. `Promiscuous Sniffer & Deauth Alert Monitor`: Promiscuous 802.11 frame monitor counting Management, Data, and Probe frames, with an audible and visual alert displaying victim MAC and AP BSSID during active deauthentication attacks.
  4. `BLE Scanner / Radar`: Proximity scanner for Bluetooth Low Energy advertising packets, listing device names, MAC addresses, RSSI, and vendor tags (`Apple`, `Google Fast Pair`, `Flipper Zero`).
  5. `BLE Beacon Spammer`: Educational notification flood testing simulating Apple Action Modals, Google Fast Pair prompts, and custom GHOSTRF advertising beacons.
  6. `Wi-Fi Beacon Spammer`: Multi-SSID beacon generator for SSID crowding and receiver resilience testing.

### Mode 8: Battery Monitor & Power Management
- **Architecture**: `src/main.cpp`, `src/battery.cpp`
- **16x Oversampling Precision ADC**: Reads battery voltage via `ADC1_CH2` (GPIO 2) with hardware calibration, 16x oversampling, and an exponential moving average filter.
- **Real-Time Readout**: Displays battery voltage in millivolts (e.g., `4.14V`), graphical charge percentage (0–100%), and dynamic charging states:
  - `[CHG]`: Active charging detected via TP4056.
  - `[USB]`: USB power connected, battery fully charged.
  - `[===]`: Normal battery discharge (healthy voltage).
  - `[!  ]`: Low battery warning (< 3.45V).
  - `[OV!]`: Overvoltage warning (> 4.30V).
- **Power Saver**: Automatic display sleep after 60 seconds of inactivity to maximize field operational life, waking instantly on any button press, incoming serial packet, or BLE connection.

### Mode 9: Settings & System Information
- **Architecture**: `src/main.cpp`, `src/storage.cpp`
- **Runtime Configurations**:
  - Bluetooth LE 5.0 Nordic UART Service toggle ON / OFF.
  - Default operating frequency preset selector (433.92 MHz, 315.00 MHz, 868.35 MHz).
  - Transmit PA power output (-30 dBm, -10 dBm, 0 dBm, +10 dBm).
  - RAW squelch sensitivity (-85 dBm to -65 dBm).
  - Non-volatile memory management: Clear NVS signal bank.
- **Hardware Diagnostics Screen**: Displays CPU clock (160 MHz), Free Heap (> 230 KB), Minimum Heap watermark, Flash chip size (4 MB), LittleFS partition allocation (896 KB mounted), and CC1101 SPI bus status. All settings persist across reboots in NVS Preferences.

---

## 💾 Storage Architecture & Flipper Zero `.sub` Compatibility

GHOSTRF features a dedicated **896 KB LittleFS flash partition** (`sub_storage_init()`) formatted for direct, native compatibility with standard Flipper Zero Sub-GHz files.

### Supported File Formats

#### 1. Protocol Key Files (`Filetype: Flipper SubGhz Key File`)
```
Filetype: Flipper SubGhz Key File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok650Async
Protocol: KeeLoq
Bit: 64
Key: 00 00 00 00 28 35 12 7A
Serial: 0x02835127
Btn: 0x2
```

#### 2. Signed Microsecond RAW Files (`Filetype: Flipper SubGhz RAW File`)
```
Filetype: Flipper SubGhz RAW File
Version: 1
Frequency: 433920000
Preset: FuriHalSubGhzPresetOok270Async
Protocol: RAW
RAW_Data: 420 -840 840 -420 420 -840 840 -420 1260 -420
```

Signals captured in the field can be saved directly as `.sub` files, replayed with exact timing fidelity, or transferred to/from a PC via GHOSTRF Studio or the USB Serial CLI.

---

## 🌐 Ecosystem & Multi-Platform Software

GHOSTRF includes a complete software suite spanning desktop operating systems, Android mobile devices, and in-browser web interfaces.

### 1. GHOSTRF Studio Desktop (Qt 6 / C++)
The official desktop companion application is built on standard C++17 and Qt 6 with CMake, providing hardware control, signal analysis, and backup capabilities.

<p align="center">
  <img src="motion_render/preview_v4_website.jpg" alt="GHOSTRF Studio Desktop GUI" width="90%">
</p>

- **Standalone Portable Releases**:
  - **Linux x86_64**: [`GHOSTRF-Studio-Desktop-Linux-x64.tar.gz`](GHOSTRF-Studio-Desktop-Linux-x64.tar.gz) (Self-contained Qt 6 runtime + `run.sh` launcher).
  - **Windows 10/11 x64**: [`GHOSTRF-Studio-Desktop-Windows-x64.zip`](GHOSTRF-Studio-Desktop-Windows-x64.zip) (Standalone bundle with all Qt 6 and MinGW/MSVC DLLs).
- **Core Desktop Features**:
  - **Saved Signal Manager**: Inspect, edit, export, and import Flipper Zero `.sub` keys and JSON database backups.
  - **Digital Micro-Oscilloscope**: Interactive zoomable logic waveform viewer with microsecond timing cursors and pulse polarity inversion.
  - **Live OLED Mirror & Virtual Gamepad**: Mirror the 128x64 display over USB CDC (115200 baud) with virtual gamepad buttons.
  - **Sub-GHz Sniffer Table**: Live packet logging with one-click **Replay TX**.
  - **Full Documentation**: See [`gui/README.md`](gui/README.md).

#### Building Desktop App from Source
```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y build-essential cmake ninja-build qt6-base-dev libqt6serialport6-dev
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/gui/GHOSTRF-Studio
```

---

### 2. GHOSTRF Studio Mobile (Android & Web Bluetooth)
GHOSTRF provides dual mobile access options for wireless control via Bluetooth Low Energy 5.0 (Nordic UART Service `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`):

- **Option A: Pre-Built Android APK**
  - Download: [`web/GHOSTRF-Studio.apk`](web/GHOSTRF-Studio.apk) (Android 7.0+ / API 24+).
  - Native Kotlin application utilizing a custom `OledCanvasView` hardware-accelerated renderer for real-time 128x64 OLED mirroring.
  - Full hardware gamepad emulation (`UP`, `OK`, `DOWN`, `HOLD OK`) and instant mode switcher buttons.
  - Source code located in `android/RFHubMobile/`.
- **Option B: Zero-Install Web Bluetooth PWA**
  - Located in `android/web_ble/`.
  - Runs directly inside **Google Chrome on Android** without installing an APK.
  - Pair via Web Bluetooth API for instant display streaming and remote control.
  - Full Documentation: See [`android/README.md`](android/README.md).

---

### 3. In-Browser WebSerial 1-Click Flasher
The official web interface at [`web/index.html`](web/index.html) includes an integrated **WebSerial 1-Click Flasher** powered by ESP Web Tools.

- Requires zero command-line utilities or Python drivers.
- Works in Chromium-based browsers (Google Chrome, Microsoft Edge, Opera, Brave).
- Flashes the pre-compiled unified binary [`web/firmware_merged.bin`](web/firmware_merged.bin) directly at flash offset `0x0`.

---

## 🚀 Quick-Start: Build & Flash

### Method A: WebSerial 1-Click Flasher (No Toolchain Required)
1. Open [`web/index.html`](web/index.html) in Google Chrome or Microsoft Edge (or host via GitHub Pages).
2. Connect your ESP32-C3 SuperMini to your computer with a USB-C data cable.
3. Click **`[ INSTALL: FLASH TO ESP32-C3 ]`**.
4. Select the detected USB Serial / JTAG CDC port in the browser prompt and confirm.
5. Flashing finishes in ~30 seconds, and GHOSTRF reboots into the Main Menu.

---

### Method B: PlatformIO CLI Build & Flash
For developers and makers modifying firmware source code:

1. **Clone the repository**:
   ```bash
   git clone https://github.com/esrhunter/cc1101.git
   cd cc1101
   ```

2. **Compile the firmware**:
   ```bash
   ~/.platformio/penv/bin/pio run
   ```
   *(Or using system PlatformIO: `pio run`)*

3. **Upload to ESP32-C3**:
   ```bash
   ~/.platformio/penv/bin/pio run -t upload
   ```

4. **Open interactive Serial Monitor (115200 baud)**:
   ```bash
   ~/.platformio/penv/bin/pio run -t monitor
   ```

---

## 📁 Codebase Architecture

The GHOSTRF firmware is written in clean, non-blocking C++17 under PlatformIO with modular separation of concerns:

| Source File | Subsystem & Technical Scope |
|---|---|
| [`src/config.h`](src/config.h) | Hardware pin assignments (SPI, I2C, buttons, ADC), frequency tables, PA power levels |
| [`src/types.h`](src/types.h) | Core data models, `RFCommand`, `AppState` enum, button event definitions |
| [`src/main.cpp`](src/main.cpp) | System initialization, deterministic non-blocking state machine loop, mode dispatchers |
| [`src/radio.h`](src/radio.h) / `radio.cpp` | CC1101 SPI abstraction layer, register tables, RSSI spectrum sweeps, CW/Jam generation |
| [`src/ui.h`](src/ui.h) / `ui.cpp` | SSD1306 OLED rendering engine, 32-channel spectrum with peak hold, menu navigation |
| [`src/buttons.h`](src/buttons.h) / `buttons.cpp` | Debounced edge-detection state machine for 3-button navigation and long-press detection |
| [`src/storage.h`](src/storage.h) / `storage.cpp` | Non-volatile Flash storage manager via ESP32 NVS Preferences for settings and signals |
| [`src/sub_format.h`](src/sub_format.h) / `sub_format.cpp` | Flipper Zero `.sub` file streaming parser and serializer |
| [`src/sub_storage.h`](src/sub_storage.h) / `sub_storage.cpp` | LittleFS flash filesystem mounting and signal management |
| [`src/keeloq.h`](src/keeloq.h) / `keeloq.cpp` | KeeLoq HCS200/300/301 dynamic rolling code cryptanalysis, decryption, and validation |
| [`src/keeloq_analyzer.h`](src/keeloq_analyzer.h) / `keeloq_analyzer.cpp` | Dynamic code inspection engine, serial number extraction, button code mapping |
| [`src/sensor_telemetry.h`](src/sensor_telemetry.h) / `sensor_telemetry.cpp` | FSK/GFSK demodulation for Oregon Scientific weather stations and automotive TPMS |
| [`src/wifi_tools.h`](src/wifi_tools.h) / `wifi_tools.cpp` | Native 2.4 GHz Wi-Fi AP scanner, 13-channel spectrum, promiscuous deauth monitor, BLE radar |
| [`src/battery.h`](src/battery.h) / `battery.cpp` | Precision Li-Po ADC monitor, 16x oversampling filter, charge status evaluation |
| [`platformio.ini`](platformio.ini) | Build configuration for `esp32-c3-devkitm-1` with 4 MB Flash and `huge_app.csv` partition |

---

## 📜 License & Legal Disclaimer

- **License**: GHOSTRF is released as free and open-source software under the **[GNU General Public License v3.0](LICENSE)**.
- **Educational & Authorized Use Only**: GHOSTRF is designed strictly for educational purposes, RF security research, and authorized auditing of personal wireless hardware. Always verify local telecommunications and radio spectrum regulations before transmitting on any frequency. Transmitting jamming signals or interfering with licensed radio bands without proper authorization is illegal. The authors assume no liability for misuse of this project.
