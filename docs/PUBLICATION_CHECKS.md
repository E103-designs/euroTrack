# Publication checks — 27 October 2026

## Current verification

Both complete source distributions were recompiled with Arduino AVR Boards 1.8.6, FQBN `arduino:avr:nano:cpu=atmega328`, warnings all; Adafruit SSD1306 2.5.17, GFX 1.12.6 and BusIO 1.17.4.

| Distribution | AVR build | Flash / 30720 | Static SRAM / 2048 | Host suite ASan/UBSan |
| --- | --- | --- | --- | --- |
| Modular V4.1 | PASS | 20066 | 594 | PASS |
| SingleFile V4.1 | PASS | 20066 | 594 | PASS |

The static RAM figure excludes the OLED's 1024-byte heap framebuffer and allocator overhead; about 430 bytes remain before allocator/stack overhead. Stack headroom still requires measurement on the Nano. The only compiler warnings are the known unused `tag` parameters in Arduino core `new.cpp`; none are reported from the euroTrack sketches.

Both fresh application HEX files are byte-identical to each other and to the existing verified distribution: SHA-256 `7a8b63d6d970dee4425611e2e94aa37fc66ac12207166c1189477cb918b387bc`. They are included in each firmware's `release/` folder, without a bootloader image. Upload instructions use the source sketch and the bootloader appropriate to the actual Nano.

The existing regression suite was run against each source form through the portable root runner, using the real Adafruit GFX library. Both passed timing/input/EEPROM regression checks, migration at every write phase, view persistence, visual-only layout switching, eight-item menu navigation and immutable frame transmission; each run emitted 17 software-rendered frames. These are host tests, not AVR execution or analog simulation.

## Source provenance

- Modular `.ino`: `47a7f86ddfc260bf3a4ed4110f16d434189f7c581f92ee093aa456f03baa72ac`.
- `SettingsStore.h`: `28fc7fa583a839f753c2ee971439880f949c537a27ca9ca47931f6d7ef7fb9fd`.
- SingleFile `.ino`: `b977d83cf8efdaf191a64f7330362802caaae7e5706df603a0ea81b7fd3fe7db`.

The SingleFile source is copied from the versioned ZIP and matches its original manifest. The loose local copy differs only in introductory comments; original project files were left unchanged. No firmware behavior or settings layout was changed for publication. The new root test runner points the existing test harness at each sketch without editing either source.

The four supplied photos show the actual assembled product. Published OLED previews come from software rendering, not a physical display photograph. No current schematic, native CAD or revision-matched manufacturing package was promoted from old assets.

No firmware was uploaded to a physical module, and no new electrical/timing/stack measurements were performed. The detailed inherited report in `firmware/Eurotrack_V4_1/VALIDATION.md` lists the remaining bench tests.
