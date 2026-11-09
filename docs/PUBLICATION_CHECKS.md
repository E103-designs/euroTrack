# V4.2 publication checks

Verified on 9 November 2026 against the repository copy being published.

| Check | Result |
| --- | --- |
| V4.2 Nano AVR build | Pass: 22,214 bytes flash; 617 bytes static SRAM |
| Application HEX | Byte-identical to the validated 8 November V4.2 binary |
| V4.2 host suite | Pass with AddressSanitizer/UndefinedBehaviorSanitizer |
| Historical V4.1 modular and single-file suites | Both pass with ASan/UBSan |
| README and user/development-guide links | Resolved locally |
| Firmware, test and binary SHA-256 | Match the V4.2 build manifest |
| Display gallery | Actual firmware renders; both layouts, STOP/RUN, menus and presets |

The published source is [Eurotrack_V4_2_SingleFile.ino](../firmware/Eurotrack_V4_2_SingleFile/Eurotrack_V4_2_SingleFile.ino). Only the host runner's temporary-directory/compiler selection was made portable for publication; firmware behavior and application HEX are unchanged.

The OLED allocates another 1,024 bytes, leaving about 407 bytes before allocator/stack overhead. No firmware was uploaded to hardware during publication. See the [V4.2 validation report](../firmware/Eurotrack_V4_2_SingleFile/VALIDATION.md) for software scenarios and outstanding bench checks.

Application HEX SHA-256: `68fb5a3bac775d0850bf0237b2bbeda3f046737743024f7be56cd8c1a61fcc31`.

Toolchain: Arduino AVR Boards 1.8.6, Adafruit SSD1306 2.5.17, Adafruit GFX 1.12.6, Adafruit BusIO 1.17.4. The only compiler warnings are unused `tag` parameters in the Arduino core `new.cpp`.

## Historical V4.1 checks


| Check | Result |
| --- | --- |
| Modular AVR build | Pass: 20,066 bytes flash; 594 bytes static SRAM |
| Single-file AVR build | Pass: same flash and static SRAM |
| Application HEX comparison | Byte-identical across both distributions |
| Host regression suites | Pass for both distributions with ASan/UBSan |
| OLED reference frames | 17 software-rendered frames per distribution |

Toolchain: Arduino AVR Boards 1.8.6; Adafruit SSD1306 2.5.17; Adafruit GFX 1.12.6; Adafruit BusIO 1.17.4. The only recorded compiler warnings are unused `tag` parameters in Arduino core `new.cpp`.

Application HEX SHA-256: `7a8b63d6d970dee4425611e2e94aa37fc66ac12207166c1189477cb918b387bc`.

The OLED allocates a further 1,024-byte framebuffer. Approximately 430 bytes remain before stack and allocator overhead. Hardware qualification includes worst-case stack, clock jitter, pulse widths, power interruption, input levels and OLED bus fault behavior. See [the detailed report](../firmware/Eurotrack_V4_1/VALIDATION.md).

Reproduce the host checks with `GFX_DIR=/path/to/Adafruit_GFX_Library sh tools/test_host.sh`. These use the actual sketches and Adafruit GFX with simulated GPIO, EEPROM and transport adapters.
