# Build and modify euroTrack

## Reproducible baseline

The current source is **V4.2**, dated 8 November 2026: classic Arduino Nano **ATmega328P, 16 MHz**, SSD1306 **128×64 I2C at 0x3C**, clock **4 PPQN**. Its complete persistence implementation is embedded in `firmware/Eurotrack_V4_2_SingleFile/Eurotrack_V4_2_SingleFile.ino`. Historical V4.1 modular/single-file sources remain for reference. [Publication checks](PUBLICATION_CHECKS.md) separate versions and recorded results.

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
arduino-cli compile --warnings all --fqbn arduino:avr:nano:cpu=atmega328 --build-path /tmp/eurotrack-v42-build firmware/Eurotrack_V4_2_SingleFile
arduino-cli board list
# Replace PORT with the confirmed Nano serial port; upload one sketch only:
arduino-cli upload --port PORT --fqbn arduino:avr:nano:cpu=atmega328 firmware/Eurotrack_V4_2_SingleFile
```

Use `cpu=atmega328old` only if your Nano needs the old bootloader. In Arduino IDE keep each sketch inside its matching directory. The modular `.ino` and `SettingsStore.h` must stay together. Verify USB/external-power arrangements on the actual hardware before connecting them together. Do not erase EEPROM if settings migration should be preserved.

## Source map

| Area | Entry points / data | What to edit |
| --- | --- | --- |
| Meter and grouping | `MeterDef`, `meterDefs`, `loadMeter`, `handleTick` | Meter numbers, group lengths in sixteenth-note ticks and bar length |
| Input events | `clockChangeISR`, `resetISR`, `enqueueInput`, `serviceInputs`, `applyRunEvent`, `configureResetInput` | Edge filtering, ordered CLOCK/RESET/RUN handling and slaved transport |
| Clock | `serviceClock`, `setClockSource`, `internalNextUs`, `internalRemainder` | Tempo, phase deadlines, external priority/fallback |
| Outputs | `processMeasTick`, `rhythmMasks`, `serviceGates`, `fireMeasureGate`, `closeInternalGate` | MEAS modes, delayed swing, presets and pulse widths |
| Buttons/menu | `Button::update`, `serviceButtons`, `changeMenuValue`, `MenuItem` | Debounce, gestures, ten menu options |
| Timer | `toggleTimer`, `stopTimer`, `stopPerformance`, `serviceTimer` | Stopwatch behavior and position coupling |
| UI | `drawBarBeatView`, `drawTimerView`, `drawMain`, `drawMenu`, `drawBeats` | Layout, text sizes, grouping strip |
| OLED transport | `initializeOLED`, `oledTransactionOK`, `serviceDisplay` | Chunked transfer, fault handling and retries |
| Persistence | `Settings`, `SettingsStore`, `scheduleSave`, `serviceSettings` | Validation, CRC, migration, non-blocking EEPROM writes |

Meter values in PROGMEM are copied into `meter` before use. When adding meters update validation (`s.meter < 7`), selection wrap, table size and regression tests together. The UI and test suite assume a maximum of five grouped beats: changing that requires coordinated layout/data changes.

## Pin map from firmware

| Signal | Nano pin | Mode |
| --- | --- | --- |
| CLKIN | D2 | INPUT, CHANGE interrupt |
| RESET / RUN | D3 | INPUT, CHANGE interrupt; role selected in setup |
| T.S. / METER button | D4 | INPUT_PULLUP, active LOW |
| CLKOUT / CLOCK_THRU_OUTPUT | D5 | OUTPUT |
| MEAS / MEASURE_TRIG_OUTPUT | D6 | OUTPUT |
| TIMER button | D7 | INPUT, active HIGH, external pulldown assumed |
| OLED SDA / SCL | A4 / A5 | I2C, configured 400 kHz |

`TIMER_BUTTON_USES_PULLUP` is **0** to preserve the supplied wiring. Setting it to 1 also changes the expected switch wiring toward GND. A firmware pin table does not validate PCB traces, input conditioning or safe jack levels. Use the [updated schematic PDF](../hardware/schematics/README.md) and confirm correspondence with the assembled PCB.

## Memory and timing constraints

The recorded application uses **22,214 bytes flash** and **617 bytes static SRAM**. The OLED allocates an additional **1024 bytes on the heap**. With 2048 bytes total, about **407 bytes remain before allocator overhead and stack**. Do not quote the Arduino static RAM percentage as total runtime occupancy. Avoid another framebuffer, large stack arrays, heap churn and deep call chains. Measure stack headroom on the actual Nano after changes.

OLED data is sent in 16-byte chunks with input/gate services between them; the in-flight frame is immutable. Do not replace this path with a blocking full-frame `display.display()` without re-evaluating timing. The requested refresh limit is 20 Hz when dirty. Wire has a 1500-us timeout; faults suspend OLED transfers and retries are deferred until idle. Test actual bus failures on hardware.

Keep ISRs short: they preserve output transitions and enqueue CLOCK/RESET/RUN into a 16-entry queue with 15 usable slots. Shared ISR/loop state requires atomic access. Preserve event ordering and the explicit overflow recovery/`!` indicator. Keep unsigned rollover-safe timing. Internal deadlines include fractional remainder to avoid accumulated rounding drift; they still run cooperatively and do not guarantee measured zero jitter.

## EEPROM format

Two 11-byte schema-3 records occupy **32–42 and 64–74**. Each has magic/version/sequence, settings, CRC-8 and a final commit marker. Schemas 1 and 2 are imported; legacy V3.9 bytes **0–5** remain untouched. `displayView` shares a byte with `timerMode`. Schema 3 packs reset mode, MEAS mode and swing index into the previously reserved bits of byte 7; the record size/addresses stay unchanged. The asynchronous writer starts one byte at a time after checking EEPE and uses the required EEMPE/EEPE atomic launch sequence.

For a format change, add explicit migration and power-cut/fallback tests; never silently reuse legacy addresses or remove CRC/commit behavior. Internal-clock enable is intentionally not saved. Autosave is delayed one second after the last change; SAVE/EXIT requests a save, not a transaction rollback.

## Tests

The current C++17 suite includes the actual V4.2 single-file firmware and **real Adafruit GFX**; GPIO, timing, EEPROM and Wire are host adapters. It is not an AVR/analog simulation. Run the current suite from repository root:

```sh
GFX_DIR="/path/to/Adafruit_GFX_Library" sh firmware/Eurotrack_V4_2_SingleFile/tests/run_tests.sh
```

The runner uses AddressSanitizer/UndefinedBehaviorSanitizer. It does not flash the Nano. `GFX_DIR` points to the real installed GFX library; `CXX` can override clang++. PGM frames are written to `firmware/Eurotrack_V4_2_SingleFile/preview/` and ignored by Git. To run historical V4.1 distributions plus V4.2, use `sh tools/test_host.sh` with the same variables.

For visual changes, run `tests/render_previews.py` with Pillow after the current host suite. Published previews are actual software renders, not physical OLED measurements. The [display gallery](DISPLAY.md) describes their states.

Retest seven meters and legacy resets, RUN HIGH/LOW including delayed handling and boot HIGH, every MEAS mode and swing percentage, stale-pulse cancellation, EEPROM migration/power cuts, rollover, buttons and immutable OLED frames. Build the Nano source and record memory. Bench-test clock jitter, jack waveforms/levels, display faults, power interruption and worst-case stack separately.

## Source distributions

V4.2 is maintained directly in the single-file sketch, including the settings class. Its tests include that exact file. Keep the implementation before its first use, preserve migration and commit semantics, and update documentation/manifests with functional changes. Do not backport V4.2 behavior into the archived V4.1 source silently.

For the two historical V4.1 forms, the modular sketch/header and its embedded copy remain equivalent to one another. Never add an alternate complete sketch as a second `.ino` tab in the same Arduino sketch.
