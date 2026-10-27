# euroTrack V4.1 user guide

## Clock and position

CLKIN expects **4 PPQN**, one rising edge per sixteenth note. BPM is expressed in quarter notes, including compound meters. At 120 BPM the input is 8 Hz; in 4/4 a bar is 16 clock edges. The first accepted edge after startup or a pending reset denotes bar 1 / beat 1 / subdivision 0 and emits a MEAS pulse.

The displayed beat is a musical group, not always an individual denominator note:

| Meter | Groups in eighth-note terms | Displayed beats per bar | 4-PPQN ticks per bar |
| --- | --- | --- | --- |
| 4/4 | Four quarter notes | 4 | 16 |
| 3/4 | Three quarter notes | 3 | 12 |
| 6/8 | 3+3 eighths | 2 | 12 |
| 5/4 | Five quarter notes | 5 | 20 |
| 7/8 | 2+2+3 eighths | 3 | 14 |
| 9/8 | 3+3+3 eighths | 3 | 18 |
| 12/8 | 3+3+3+3 eighths | 4 | 24 |

Bars wrap from 9999 to 1 without restarting the phrase-marker cadence. MEAS emits at the first bar and then every selected 1/2/4/8/16 bars. Pulse lengths are requested at 5/10/20/50 ms and serviced cooperatively: actual widths and jitter require measurement.

CLKOUT copies both external clock transitions from an interrupt. Very short rising-edge intervals (under 1000 microseconds) are rejected for counting, but THRU still copies the external waveform. It does not clean up electrical glitches. RESET is ordered with clock events and takes effect on the next clock; it does not retroactively reset earlier queued events.

External clock has priority. After **1500 ms** without a valid rising edge, the display reports no clock, or the internal source resumes if enabled and the timer is RUN. Internal fallback starts with an immediate pulse; it does not predict the removed clock's phase. At 4 PPQN, this timeout is unsuitable for masters with more than 1.5 s between edges. The internal range is 50–250 BPM in 5-BPM steps; internal CLKOUT pulses target 5 ms independently of MEAS length.

## Buttons

Panel **T.S.** corresponds to METER in firmware documentation. Short actions occur on release after 25 ms debounce.

| Gesture | Action |
| --- | --- |
| T.S. short | Open meter selection; subsequent presses advance the seven meters |
| TIMER short | Start / pause / resume elapsed time |
| TIMER held 1 s | Stop and clear elapsed time; also reset position in TIME+BAR mode |
| Both held 1 s, then both released | Enter setup without executing the short actions |
| T.S. in setup | Next item, wrapping after 8 |
| TIMER in setup | Change selected item, or save/exit at item 8 |

Meter selection closes after 3 seconds. Changing the meter restarts musical position at bar 1 on the next clock. External clocks continue counting while elapsed time is stopped or paused. Internal clocks require timer RUN.

## Setup menu

| Item | Display/function | Values / meaning |
| --- | --- | --- |
| 1/8 | MEASURE OUTPUT EVERY | 1, 2, 4, 8, 16 bars |
| 2/8 | INTERNAL CLOCK | ON/OFF; always OFF after boot, not saved |
| 3/8 | INTERNAL TEMPO | 50…250 BPM, steps of 5; default 120 |
| 4/8 | MEASURE PULSE LENGTH | 5, 10, 20, 50 ms |
| 5/8 | RESET INPUT RESETS | BAR: position only; BAR+TIME: position plus stop/clear timer on the next clock |
| 6/8 | TIMER BUTTON CONTROLS | TIME: elapsed time only; TIME+BAR: start from stopped and stop also reset position |
| 7/8 | DISPLAY VIEW | BAR/BEAT or TIMER; changes presentation without resetting timing or count |
| 8/8 | STORE SETTINGS | TIMER requests SAVE/EXIT |

Persistent settings autosave **one second after the last change**, even before SAVE/EXIT. There is no cancel-and-revert menu operation. Internal clock enable is intentionally not persistent; its BPM is persistent.

## Display

**BAR/BEAT** gives large musical-position digits and a compact timer. **TIMER** enlarges HH:MM:SS while retaining bar/beat information. Both show state, clock source/BPM, meter and a beat strip. These examples are **software renders using the firmware drawing functions**, not photographs of a running OLED:

![V4.1 software-rendered display layouts](../firmware/Eurotrack_V4_1/preview/display_overview.png)

`READY`, `RUN` and `PAUSE` describe the timer. `EXT`/`INT` identify the clock source; `EXT ---` awaits a valid measured interval. `NO CLK` means no active source. `*` signals a pending or ongoing save: wait for it to disappear before powering down if the latest settings should persist. `!` flags queue loss or a severe internal-clock delay; resynchronize with RESET.

The stopwatch uses 32-bit millisecond arithmetic, with a continuous-run horizon of about 49.7 days; displayed hours wrap after 99. It is a performance timer, not a long-term data logger.

## Persistence and updates

V4.1 settings use schema 2, two CRC-protected 11-byte records at EEPROM **32–42 and 64–74**. The commit marker is written last. It reads schema 1 (V4.0) and legacy V3.9 bytes 0–5; the default imported view is BAR/BEAT. Legacy bytes are not rewritten.

Avoid erasing EEPROM if you want migration. Older firmware may not understand schema 2 and may fall back to older stored values. Changing firmware does not guarantee settings portability unless the reader implements this format.

## Troubleshooting and limits

| Symptom | Check |
| --- | --- |
| BPM/count differs from the master | Confirm 4 PPQN; check meter grouping and accepted pulse rate |
| Internal clock does not run | Enable item 2 and start TIMER; a connected external source takes priority |
| Pausing timer does not stop bars | Expected with external clock; timer and incoming musical count are separate |
| New meter is not selected on the first press | First T.S. press opens selection; subsequent presses change it |
| Missing SettingsStore.h | Use the complete modular folder, or the self-contained SingleFile sketch |
| Clock lost after a very slow source | External timeout is 1500 ms between valid rising edges |
| Blank OLED | Check address 0x3C, wiring, 128×64 model and supply; the firmware can continue headless after an OLED fault |
| `!` on display | Events were lost or internal servicing stalled; reset/re-sync and investigate load/timing |

Use only levels established safe for the assembled board. Software pin modes and the Nano's logic rail are not a verified specification for the external jacks. The [validation report](../firmware/Eurotrack_V4_1/VALIDATION.md) lists remaining hardware, stack, brownout and timing checks.
