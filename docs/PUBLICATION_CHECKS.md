# V4.1 validation

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
