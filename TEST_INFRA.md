# Automated Test Infrastructure (TEST_INFRA)

## Overview & Architecture
The GHOSTRF test infrastructure provides an automated, non-hardware verification suite running on host C++17 (Linux x86_64) and PlatformIO test environments. It validates the complete mathematical and logical behavior of the Sub-GHz firmware expansion without requiring physical radio hardware (CC1101 transceivers) or target microcontrollers (ESP32-C3).

```
                            ┌────────────────────────────────────────┐
                            │    test_main.cpp (Unified Runner)      │
                            └───────────────────┬────────────────────┘
                                                │
         ┌──────────────────────────────────────┼──────────────────────────────────────┐
         │                                      │                                      │
         ▼                                      ▼                                      ▼
┌─────────────────────────┐            ┌─────────────────────────┐            ┌─────────────────────────┐
│   test_sub_format.cpp   │            │     test_keeloq.cpp     │            │   test_telemetry.cpp    │
│ - Protocol-Based .sub   │            │ - 14 Cryptographic K1..14│            │ - Oregon Scientific v2.1│
│ - Multi-line RAW Pulses │            │ - Normal Key Derivation │            │ - Oregon Scientific v3.0│
│ - Lenient Stream Parser │            │ - 66-Bit Frame Assembly │            │ - Toyota PMV-C210 TPMS  │
│ - Exact Line Serializer │            │ - 3-Zone Resync Machine │            │ - Ford Continental TPMS │
│ - Chunked I/O Streaming │            │ - Rollover & Replay Drop│            │ - Schrader TPMS         │
│ - Round-Trip Fidelity   │            │ - Single-Step Emulation │            │ - Pulse Demodulation    │
└────────────┬────────────┘            └────────────┬────────────┘            └────────────┬────────────┘
             │                                      │                                      │
             └──────────────────────────────────────┼──────────────────────────────────────┘
                                                    │
                                                    ▼
                                       ┌─────────────────────────┐
                                       │      test_harness.h     │
                                       │ - Zero-alloc assertions │
                                       │ - ANSI colored output   │
                                       │ - Tier-based reporting  │
                                       │ - Per-test micro-timers │
                                       └─────────────────────────┘
```

---

## Directory Structure & Layout
All test files reside strictly in `test/`, keeping production `src/` and `lib/` decoupled from test harness code:

```
test/
├── Arduino.h             # Lightweight Arduino standard types shim for host GCC compilation
├── test_harness.h        # Zero-dependency C++17 test harness, assertions & tier reporting
├── test_main.cpp         # Main entry point and unified test runner
├── test_sub_format.cpp   # Flipper Zero .sub parser and serializer test suite (14 tests)
├── test_keeloq.cpp       # KeeLoq NLFSR cipher, key derivation & resync state machine (11 tests)
└── test_telemetry.cpp    # Oregon Scientific weather and TPMS telemetry decoders (13 tests)
```

---

## Compilation & Execution Command
To build and execute the non-hardware E2E test suite on the host system:

```bash
g++ -std=c++17 -Wall -Wextra -Itest -Isrc \
    test/test_main.cpp \
    test/test_sub_format.cpp \
    test/test_keeloq.cpp \
    test/test_telemetry.cpp \
    src/sub_format.cpp \
    src/keeloq.cpp \
    src/keeloq_analyzer.cpp \
    src/sensor_telemetry.cpp \
    -o run_tests && ./run_tests
```

Execution completes in under 1 millisecond and outputs formatted ANSI color summaries.

---

## 4-Tier Test Suite Organization

### Tier 1: Feature Coverage (>=5 test cases per feature)
Verifies the nominal happy-path behaviors against authoritative specifications:
1. **KeeLoq Cryptographic Engine**: Evaluates all 14 standard test vectors (K1..K14) derived in `spec_miner/handoff.md` over 528 rounds of non-linear feedback encryption and decryption.
2. **Key Derivation**: Tests Microchip Normal Learning (vectors NK1, NK2, NK3) and boundary serial derivations.
3. **Frame Packing & Pulse Assembly**: Verifies 66-bit frame structure, 400 µs elementary PWM timing, 12 preamble cycles (24 pulses), 4000 µs header gap, and 132 PWM pulses (total 157 pulses).
4. **Flipper Zero Protocol Parser**: Parses standard `.sub` files for Princeton (24-bit), Came (12-bit), Nice FLO (12-bit), and KeeLoq (64-bit with manufacturer).
5. **Flipper Zero RAW Parser**: Parses multi-line signed pulses (`RAW_Data:`) into callback streams without heap allocation.
6. **Telemetry Decoders**: Decodes Oregon Scientific v2.1 (warm and freezing day packets) and automotive TPMS (Toyota Pacific PMV-C210, Ford Continental VDO, Schrader motorcycle).

### Tier 2: Boundary & Corner Cases
Verifies resilience against corrupted, malformed, or boundary inputs:
1. **Empty & Truncated Streams**: Zero-byte files, truncated headers, and incomplete packet streams fail safely with boolean false and zero memory faults.
2. **KeeLoq Counter Rollover**: Evaluates counter wrap from `0xFFFF` to `0x0000` (`(0 - 65535) & 0xFFFF = 1`), confirming sequential acceptance in Zone 1.
3. **KeeLoq Window Boundaries**: Tests exact delta boundaries at $\Delta = 16$ (Zone 1 maximum), $\Delta = 17$ (Zone 2 minimum), and $\Delta = 32768$ (immediate desync rejection).
4. **KeeLoq Discriminator Mismatch**: Packets with corrupted lower 10 bits of serial are detected and rejected prior to counter evaluation.
5. **KeeLoq Interrupted Resync**: Non-consecutive clicks during Zone 2 pending state abort the resync sequence.
6. **Negative Temperatures**: Validates Oregon Scientific sign bit (nibble 11 bit 3) and Ford TPMS offset subtraction ($T = (b_5 \& 0\text{x7F}) - 56$) down to $-10.0\text{ }^\circ\text{C}$.
7. **Corrupted Checksums & CRCs**: Single-bit corruption in Oregon Scientific swapped-nibble checksum, Toyota CRC-8, and Ford additive checksum triggers immediate validation rejection.
8. **Toyota Pressure Mismatch**: Redundant inverted pressure check ($b_4/b_5$ raw pressure vs $b_7$ inverted pressure) rejects corrupt packets prior to CRC computation.
9. **Stream Parser Tolerances**: Accepts unspaced continuous hex keys and relaxed header prefixes (`SubGhz Key File` vs `Flipper SubGhz Key File`).

### Tier 3: Cross-Feature Interactions
Exercises end-to-end data pipelines across multiple subsystems:
1. **Flipper Protocol Roundtrip**: `.sub` text $\to$ `SubFormatParser::parseFile` $\to$ `SubFormatSerializer::serializeKey` $\to$ re-parse $\to$ assert byte-for-byte fidelity across all headers, keys, and timing parameters.
2. **Flipper RAW Roundtrip**: Signed pulse stream $\to$ serialize chunked with line wraps $\to$ re-parse via streaming callback $\to$ assert exact element-by-element equality.
3. **KeeLoq Closed Loop**: Master Key + Serial $\to$ Normal Learning derivation $\to$ Assemble plaintext $\to$ Encrypt $\to$ Analyze packet $\to$ Single-step emulate ($CNT+1$) $\to$ Transmit to receiver $\to$ Verify sequential sync acceptance!
4. **Manchester Demodulation Pipeline**: Oregon Scientific packet $\to$ synthesize raw Manchester pulse intervals ($\approx 488\text{ }\mu\text{s} / 976\text{ }\mu\text{s}$) $\to$ ingest via `telemetry_process_pulses` $\to$ extract temperature, humidity, and channel with valid CRC.

### Tier 4: Real-World Scenarios
Simulates realistic field environments:
1. **Authentic Community Captures**: Replays authentic captures of Came Top432NA barrier remotes and EV1527 PIR motion sensors.
2. **Multi-Remote Garage Door System**: Simulates a receiver tracking two independent transmitters (Remote A and Remote B) with different keys, counters, and out-of-range pocket presses without cross-talk or counter corruption.
3. **Automotive 4-Wheel TPMS Network**: Simulates Front-Left, Front-Right, Rear-Left, and Rear-Right tire sensors transmitting periodic Pacific PMV-C210 packets with realistic tire pressures and temperatures.

---

## Authoritative Specification Sources
All test cases and expected outputs are ground-truthed against published specifications:
- **Microchip AN642 / AN645 / AN662**: KeeLoq encryption algorithm, Normal Learning key generation, and 3-zone code hopping receiver design.
- **Microchip HCS301 Datasheet (DS21137G)**: 66-bit frame layout, PWM timing ($T_E = 400\text{ }\mu\text{s}$), and discriminator definition.
- **Flipper Zero SubGhz Firmware (`lib/subghz/`)**: File headers, protocol key syntax, and `RAW_Data:` signed integer rules.
- **rtl_433 Reference Implementations**: Oregon Scientific v2.1 Manchester protocol (`oregon_scientific.c`), Toyota Pacific PMV-C210 (`tpms_toyota.c`), and Ford VDO (`tpms_ford.c`).
