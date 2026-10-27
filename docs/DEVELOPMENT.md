# Build and modify euroTrack

## Reproducible baseline

V4.1, dated 20 October 2026: classic Arduino Nano **ATmega328P, 16 MHz**, SSD1306 **128×64 I2C at 0x3C**, clock **4 PPQN**. SingleFile embeds the exact original SettingsStore class; the modular form keeps it in a header. The existing manifests record identical application HEX for both distributions. [Publication checks](PUBLICATION_CHECKS.md) record the current verification.

| Dependency | Version |
| --- | --- |
| Arduino AVR Boards | 1.8.6 |
| Adafruit SSD1306 | 2.5.17 |
| Adafruit GFX Library | 1.12.6 |
| Adafruit BusIO | 1.17.4 |

Wire, EEPROM, avr/pgmspace and util/atomic come from the AVR core. Install third-party libraries separately; their licenses are unchanged.

From repository root, with Arduino CLI installed:

```sh
arduino-cli core update-index
arduino-cli core install arduino:avr@1.8.6
arduino-cli lib install "Adafruit SSD1306@2.5.17" "Adafruit GFX Library@1.12.6" "Adafruit BusIO@1.17.4"
arduino-cli compile --warnings all --fqbn arduino:avr:nano:cpu=atmega328 --build-path /tmp/eurotrack-v41-build firmware/Eurotrack_V4_1
arduino-cli compile --warnings all --fqbn arduino:avr:nano:cpu=atmega328 --build-path /tmp/eurotrack-single-build firmware/Eurotrack_V4_1_SingleFile
arduino-cli board list
# Replace PORT with the confirmed Nano serial port; upload one sketch only:
arduino-cli upload --port PORT --fqbn arduino:avr:nano:cpu=atmega328 firmware/Eurotrack_V4_1_SingleFile
```

Use `cpu=atmega328old` only if your Nano needs the old bootloader. In Arduino IDE keep each sketch inside its matching directory. The modular `.ino` and `SettingsStore.h` must stay together. Verify USB/external-power arrangements on the actual hardware before connecting them together. Do not erase EEPROM if settings migration should be preserved.

## Source map

| Area | Entry points / data | What to edit |
| --- | --- | --- |
| Meter and grouping | `MeterDef`, `meterDefs`, `loadMeter`, `handleTick` | Meter numbers, group lengths in sixteenth-note ticks and bar length |
| Input events | `clockChangeISR`, `resetISR`, `enqueueInput`, `serviceInputs` | Edge filtering and ordered CLOCK/RESET handling |
| Clock | `serviceClock`, `setClockSource`, `internalNextUs`, `internalRemainder` | Tempo, phase deadlines, external priority/fallback |
| Outputs | `serviceGates`, `fireMeasureGate`, `closeInternalGate` | MEAS pulse lengths and internal CLKOUT pulses |
| Buttons/menu | `Button::update`, `serviceButtons`, `changeMenuValue`, `MenuItem` | Debounce, gestures, eight menu options |
| Timer | `toggleTimer`, `stopTimer`, `stopPerformance`, `serviceTimer` | Stopwatch behavior and position coupling |
| UI | `drawBarBeatView`, `drawTimerView`, `drawMain`, `drawMenu`, `drawBeats` | Layout, text sizes, grouping strip |
| OLED transport | `initializeOLED`, `oledTransactionOK`, `serviceDisplay` | Chunked transfer, fault handling and retries |
| Persistence | `Settings`, `SettingsStore`, `scheduleSave`, `serviceSettings` | Validation, CRC, migration, non-blocking EEPROM writes |

Meter values in PROGMEM are copied into `meter` before use. When adding meters update validation (`s.meter < 7`), selection wrap, table size and regression tests together. The UI and test suite assume a maximum of five grouped beats: changing that requires coordinated layout/data changes.

## Pin map from firmware

| Signal | Nano pin | Mode |
| --- | --- | --- |
| CLKIN | D2 | INPUT, CHANGE interrupt |
| RESET | D3 | INPUT, RISING interrupt |
| T.S. / METER button | D4 | INPUT_PULLUP, active LOW |
| CLKOUT / CLOCK_THRU_OUTPUT | D5 | OUTPUT |
| MEAS / MEASURE_TRIG_OUTPUT | D6 | OUTPUT |
| TIMER button | D7 | INPUT, active HIGH, external pulldown assumed |
| OLED SDA / SCL | A4 / A5 | I2C, configured 400 kHz |

`TIMER_BUTTON_USES_PULLUP` is **0** to preserve the supplied wiring. Setting it to 1 also changes the expected switch wiring toward GND. A firmware pin table does not validate PCB traces, input conditioning or safe jack levels. Use the corrected hardware schematic when uploaded.

## Memory and timing constraints

The recorded application uses **20,066 bytes flash** and **594 bytes static SRAM**. The OLED allocates an additional **1024 bytes on the heap**. With 2048 bytes total, about **430 bytes remain before allocator overhead and stack**. Do not quote the Arduino static RAM percentage as total runtime occupancy. Avoid another framebuffer, large stack arrays, heap churn and deep call chains. Measure stack headroom on the actual Nano after changes.

OLED data is sent in 16-byte chunks with input/gate services between them; the in-flight frame is immutable. Do not replace this path with a blocking full-frame `display.display()` without re-evaluating timing. The requested refresh limit is 20 Hz when dirty. Wire has a 1500-us timeout; faults suspend OLED transfers and retries are deferred until idle. Test actual bus failures on hardware.

Keep ISRs short: they preserve output transitions and enqueue CLOCK/RESET into a 16-entry queue with 15 usable slots. Shared ISR/loop state requires atomic access. Preserve event ordering and the explicit overflow recovery/`!` indicator. Keep unsigned rollover-safe timing. Internal deadlines include fractional remainder to avoid accumulated rounding drift; they still run cooperatively and do not guarantee measured zero jitter.

## EEPROM format

Two 11-byte schema-2 records occupy **32–42 and 64–74**. Each has magic/version/sequence, settings, CRC-8 and a final commit marker. Schema 1 is imported; legacy V3.9 bytes **0–5** remain untouched. `displayView` shares a byte with `timerMode`. The asynchronous writer starts one byte at a time after checking EEPE and uses the required EEMPE/EEPE atomic launch sequence.

For a format change, add explicit migration and power-cut/fallback tests; never silently reuse legacy addresses or remove CRC/commit behavior. Internal-clock enable is intentionally not saved. Autosave is delayed one second after the last change; SAVE/EXIT requests a save, not a transaction rollback.

## Tests

The existing C++17 suite includes the actual firmware and **real Adafruit GFX**; Arduino/GPIO/EEPROM/Wire/SSD1306 transport are adapters. It is not an AVR/analog simulation. Run from repository root:

```sh
GFX_DIR="/path/to/Adafruit_GFX_Library" sh tools/test_host.sh
```

The runner checks both source distributions under AddressSanitizer and UndefinedBehaviorSanitizer. It does not flash the Nano. `GFX_DIR` must point to the real installed GFX library; `CXX` can override clang++. Generated test frames go into a temporary directory. The original test and render scripts remain beside the modular source for inspection.

For visual UI changes, `tests/firmware_tests.cpp` writes PGM frames and `tests/render_previews.py` uses Pillow to make PNG previews. Published previews are software renders, not physical OLED measurements. The inherited README references the published overview image.

Retest all seven meters, first edge and RESET ordering, MEAS cadences/lengths, wrap at 9999, timer/clock transitions, fractional periods, queue overflow, button chord/debounce, OLED fault/immutable-frame behavior, display-view persistence, CRC fallback and interrupted migrations. Then rebuild both AVR sketches, compare application HEX and record memory figures. Bench-test clock jitter, pulse widths, input levels, display faults, power interruption and worst-case stack separately.

## Keeping SingleFile aligned

Make functional changes in the modular sketch/header, then replace its `#include "SettingsStore.h"` with the complete header contents (without the header-only `#pragma once`) to regenerate the single-file distribution. Keep the class before its first use. Rebuild and run both forms through the same regression harness. Compare application HEX, update version/date/manifests and user documentation together. Never add the alternate form as a second `.ino` tab in the same Arduino sketch.
