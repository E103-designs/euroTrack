# Contributing to euroTrack

Fork this repository, create a focused branch and open a pull request. Start with [DEVELOPMENT.md](docs/DEVELOPMENT.md). State the exact problem, behavior before/after, firmware/PCB revision and MCU, dependency versions, build commands, memory use and tests. Distinguish software tests from measured hardware results.

Keep the modular and single-file distributions equivalent and document any control, timing or EEPROM format change. Preserve notices and identify modified versions. Do not commit credentials, IDE preferences or unrelated/generated build files.

For hardware changes include editable CAD, PDF, matching BOM/Gerbers and an explicit pinout/conditioning diff. Supply measurement conditions for electrical claims. Corrected hardware sources belong in `hardware/schematics/`; the current placeholder is not a complete fabrication package.
