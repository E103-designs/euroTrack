// Eurotrack V4.1 — engineering candidate, Arduino Nano / ATmega328P, 16 MHz.
// CLOCK = 4 PPQN (sixteenths). Single-file distribution; see README.md before flashing hardware.
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
// SettingsStore is embedded below: no separate local header is required.

enum DisplayView : uint8_t { VIEW_BAR_BEAT = 0, VIEW_TIMER = 1 };

struct Settings {
  uint8_t meter = 0, gateEvery = 1, gateMs = 10, bpmStep = 14, resetMode = 0, timerMode = 0;
  uint8_t displayView = VIEW_BAR_BEAT;
};
// Two committed records. A power cut during a new write leaves the old record intact.
// Bytes 0..5 remain untouched for safe migration from V3.9.
class SettingsStore {
public:
  static constexpr uint8_t RECORD_SIZE = 11;
  static constexpr uint16_t SLOT0 = 32, SLOT1 = 64;
  static uint8_t crc(const uint8_t *data, uint8_t size) {
    uint8_t result = 0;
    while (size--) {
      result ^= *data++;
      for (uint8_t i = 0; i < 8; ++i)
        result = (result & 0x80) ? uint8_t((result << 1) ^ 0x07) : uint8_t(result << 1);
    }
    return result;
  }
  static bool validSettings(const Settings &s) {
    return s.meter < 7 &&
           (s.gateEvery == 1 || s.gateEvery == 2 || s.gateEvery == 4 || s.gateEvery == 8 ||
            s.gateEvery == 16) &&
           (s.gateMs == 5 || s.gateMs == 10 || s.gateMs == 20 || s.gateMs == 50) &&
           s.bpmStep <= 40 && s.resetMode <= 1 && s.timerMode <= 1 && s.displayView <= VIEW_TIMER;
  }
  void load(Settings &s) {
    uint8_t a[RECORD_SIZE], b[RECORD_SIZE];
    readRecord(SLOT0, a);
    readRecord(SLOT1, b);
    bool va = validRecord(a), vb = validRecord(b);
    if (va || vb) {
      bool useB = vb && (!va || int8_t(uint8_t(b[2] - a[2])) > 0);
      const uint8_t *p = useB ? b : a;
      activeSlot = useB ? SLOT1 : SLOT0;
      sequence = p[2];
      decode(p, s);
      migration = p[1] == 1; // Upgrade V4.0 records without losing musical settings.
      return;
    }
    // Validate each legacy field independently; erased EEPROM gets defaults.
    Settings legacy;
    uint8_t v = EEPROM.read(0);
    if (v < 7)
      legacy.meter = v;
    v = EEPROM.read(1);
    if (v == 1 || v == 2 || v == 4 || v == 8 || v == 16)
      legacy.gateEvery = v;
    v = EEPROM.read(2);
    if (v == 5 || v == 10 || v == 20 || v == 50)
      legacy.gateMs = v;
    v = EEPROM.read(3);
    if (v <= 40)
      legacy.bpmStep = v;
    v = EEPROM.read(4);
    if (v <= 1)
      legacy.resetMode = v;
    v = EEPROM.read(5);
    if (v <= 1)
      legacy.timerMode = v;
    s = legacy;
    migration = true;
  }
  bool needsMigration() const {
    return migration;
  }
  bool busy() const {
    return writing;
  }
  void save(const Settings &s) {
    if (writing || !validSettings(s))
      return;
    uint8_t existing[RECORD_SIZE];
    if (activeSlot && !migration) {
      readRecord(activeSlot, existing);
      Settings old;
      decode(existing, old);
      if (old.meter == s.meter && old.gateEvery == s.gateEvery && old.gateMs == s.gateMs &&
          old.bpmStep == s.bpmStep && old.resetMode == s.resetMode &&
          old.timerMode == s.timerMode && old.displayView == s.displayView)
        return;
    }
    record[0] = 0xE4;
    record[1] = 2; // Schema 2 packs the view beside the timer mode; same record footprint.
    record[2] = sequence + 1;
    record[3] = s.meter;
    record[4] = s.gateEvery;
    record[5] = s.gateMs;
    record[6] = s.bpmStep;
    record[7] = s.resetMode;
    record[8] = s.timerMode | (s.displayView << 1);
    record[9] = crc(record, 9);
    record[10] = 0xA5;
    targetSlot = activeSlot == SLOT0 ? SLOT1 : SLOT0;
    stage = 0;
    writing = true;
  }
  void service() {
    if (!writing || hardwareBusy())
      return;
    if (stage == RECORD_SIZE + 1) {
      activeSlot = targetSlot;
      sequence = record[2];
      migration = false;
      writing = false;
      return;
    }
    // First invalidate target commit, then payload/CRC, finally commit.
    uint8_t offset = stage == 0 ? 10 : stage - 1;
    uint8_t value = stage == 0 ? 0 : record[offset];
    uint16_t address = targetSlot + offset;
    if (EEPROM.read(address) != value)
      launchWrite(address, value);
    ++stage;
  }

private:
  uint8_t record[RECORD_SIZE] = {}, sequence = 0, stage = 0;
  uint16_t activeSlot = 0, targetSlot = 0;
  bool writing = false, migration = false;
  static void decode(const uint8_t *p, Settings &s) {
    s.meter = p[3];
    s.gateEvery = p[4];
    s.gateMs = p[5];
    s.bpmStep = p[6];
    s.resetMode = p[7];
    s.timerMode = p[1] == 2 ? p[8] & 1 : p[8];
    s.displayView = p[1] == 2 ? (p[8] >> 1) & 1 : VIEW_BAR_BEAT;
  }
  static void readRecord(uint16_t address, uint8_t *p) {
    for (uint8_t i = 0; i < RECORD_SIZE; ++i)
      p[i] = EEPROM.read(address + i);
  }
  static bool validRecord(const uint8_t *p) {
    Settings s;
    decode(p, s);
    return p[0] == 0xE4 && (p[1] == 1 || p[1] == 2) && p[8] <= (p[1] == 2 ? 3 : 1) &&
           p[10] == 0xA5 && p[9] == crc(p, 9) && validSettings(s);
  }
  static bool hardwareBusy() {
#ifdef EUROTRACK_HOST_TEST
    return false;
#else
    return EECR & _BV(EEPE);
#endif
  }
  static void launchWrite(uint16_t address, uint8_t value) {
#ifdef EUROTRACK_HOST_TEST
    EEPROM.update(address, value);
#else
    // No blocking EEPROM.update(): only the four-cycle programming launch is atomic.
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      EEAR = address;
      EEDR = value;
      EECR &= ~(_BV(EEPM1) | _BV(EEPM0));
      EECR |= _BV(EEMPE);
      EECR |= _BV(EEPE);
    }
#endif
  }
};
// End of embedded settings storage.

#if !defined(__AVR_ATmega328P__) && !defined(EUROTRACK_HOST_TEST)
#error "This firmware targets the classic ATmega328P Nano only."
#endif

#define TIMER_BUTTON_USES_PULLUP 0 // Preserve V3.9 PCB wiring: external pull-down.
constexpr uint8_t CLOCK_INPUT = 2, RESET_INPUT = 3, BUTTON_PIN = 4;
constexpr uint8_t CLOCK_THRU_OUTPUT = 5, MEASURE_TRIG_OUTPUT = 6, TIMER_BUTTON_PIN = 7;
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint32_t EDGE_FILTER_US = 1000, EXTERNAL_TIMEOUT_MS = 1500;
constexpr uint32_t FRAME_INTERVAL_MS = 50, BUTTON_DEBOUNCE_MS = 25, HOLD_MS = 1000;
constexpr uint8_t EVENT_CAPACITY = 16, EVENT_MASK = EVENT_CAPACITY - 1;
static_assert((EVENT_CAPACITY & EVENT_MASK) == 0, "Queue must be a power of two");
Adafruit_SSD1306 display(128, 64, &Wire, -1, 400000UL, 400000UL);

struct MeterDef {
  uint8_t num, den, beats, total;
  uint8_t pattern[5];
};
const MeterDef meterDefs[] PROGMEM = {
    {4, 4, 4, 16, {4, 4, 4, 4, 0}}, {3, 4, 3, 12, {4, 4, 4, 0, 0}}, {6, 8, 2, 12, {6, 6, 0, 0, 0}},
    {5, 4, 5, 20, {4, 4, 4, 4, 4}}, {7, 8, 3, 14, {4, 4, 6, 0, 0}}, {9, 8, 3, 18, {6, 6, 6, 0, 0}},
    {12, 8, 4, 24, {6, 6, 6, 6, 0}}};
MeterDef meter;
Settings settings;
SettingsStore settingsStore;

// Both input ISRs share an ordered queue: RESET cannot overtake older CLOCKs.
struct InputEvent {
  uint32_t us;
  uint8_t reset;
};
volatile InputEvent events[EVENT_CAPACITY];
volatile uint8_t eventHead = 0, eventTail = 0;
volatile bool eventOverflow = false, externalPresent = false;
volatile uint32_t externalLastMs = 0;
uint32_t lastEdgeUs = 0;
bool edgeKnown = false;
volatile bool externalOwnsOutput = false;

uint16_t measure = 1, bpm = 0;
uint8_t beat = 1, sixteenth = 0, measuresUntilGate = 0;
bool positionStarted = false, pendingReset = false, queueFault = false;
bool measureGateHigh = false, internalGateHigh = false;
uint32_t measureGateEndUs = 0, internalGateEndUs = 0;
bool internalEnabled = false, internalScheduled = false;
uint32_t internalNextUs = 0, internalRemainder = 0;
enum ClockSource : uint8_t { NO_CLOCK, EXTERNAL_CLOCK, INTERNAL_CLOCK };
ClockSource clockSource = NO_CLOCK;
bool tempoKnown = false;
uint32_t previousTickUs = 0, filteredInterval = 0;

enum TimerState : uint8_t { STOPPED, RUNNING, PAUSED };
TimerState timerState = STOPPED;
uint32_t timerStartMs = 0, timerAccumulatedMs = 0, shownSeconds = 0;

struct Button {
  bool raw = false, down = false, pressed = false, released = false;
  uint32_t changedMs = 0, pressedMs = 0;
  void update(bool reading, uint32_t now) {
    pressed = released = false;
    if (reading != raw) {
      raw = reading;
      changedMs = now;
    }
    if (raw != down && uint32_t(now - changedMs) >= BUTTON_DEBOUNCE_MS) {
      down = raw;
      pressed = down;
      released = !down;
      if (down)
        pressedMs = now;
    }
  }
};
Button meterButton, timerButton;
bool comboActive = false, comboHandled = false, suppressButtons = false, timerHoldHandled = false;
uint32_t comboStartMs = 0;
bool menuActive = false, meterSelection = false;
enum MenuItem : uint8_t {
  MENU_GATE_EVERY,
  MENU_INT_CLOCK,
  MENU_INT_BPM,
  MENU_GATE_LENGTH,
  MENU_RESET_MODE,
  MENU_TIMER_MODE,
  MENU_DISPLAY_VIEW,
  MENU_SAVE_EXIT,
  MENU_COUNT
};
uint8_t menuIndex = 0;
uint32_t meterSelectionMs = 0;
bool savePending = false;
uint32_t saveDueMs = 0;

bool displayDirty = true, oledReady = false, frameSending = false, displayFault = false;
uint16_t frameOffset = 0;
uint32_t lastFrameMs = 0, oledRetryMs = 0;

bool due(uint32_t now, uint32_t target) {
  return int32_t(now - target) >= 0;
}
void dirty() {
  displayDirty = true;
}
void scheduleSave() {
  savePending = true;
  saveDueMs = millis() + 1000;
  dirty();
}
void loadMeter() {
  memcpy_P(&meter, &meterDefs[settings.meter], sizeof(meter));
}

void enqueueInput(uint32_t us, bool reset) {
  uint8_t next = (eventHead + 1) & EVENT_MASK;
  if (next == eventTail) {
    eventOverflow = true;
    return;
  }
  events[eventHead].us = us;
  events[eventHead].reset = reset;
  eventHead = next;
}
void clockChangeISR() {
  const bool high = digitalRead(CLOCK_INPUT) == HIGH;
  // THRU keeps the external waveform, including short pulses; counting is filtered.
  digitalWrite(CLOCK_THRU_OUTPUT, high ? HIGH : LOW);
  if (!high)
    return;
  const uint32_t now = micros();
  if (edgeKnown && uint32_t(now - lastEdgeUs) < EDGE_FILTER_US)
    return;
  edgeKnown = true;
  lastEdgeUs = now;
  externalLastMs = millis();
  externalPresent = true;
  externalOwnsOutput = true;
  enqueueInput(now, false);
}
void resetISR() {
  enqueueInput(micros(), true);
}

void resetPosition() {
  measure = 1;
  beat = 1;
  sixteenth = 0;
  measuresUntilGate = 0;
  positionStarted = false;
  dirty();
}
void closeInternalGate() {
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (!externalOwnsOutput)
      digitalWrite(CLOCK_THRU_OUTPUT, LOW);
    internalGateHigh = false;
  }
}
void stopTimer() {
  timerState = STOPPED;
  timerAccumulatedMs = 0;
  shownSeconds = 0;
  internalScheduled = false;
  closeInternalGate();
  dirty();
}
void stopPerformance() {
  stopTimer();
  if (settings.timerMode)
    resetPosition();
}
void toggleTimer() {
  const uint32_t now = millis();
  if (timerState == RUNNING) {
    timerAccumulatedMs = now - timerStartMs;
    timerState = PAUSED;
    internalScheduled = false;
    closeInternalGate();
  } else {
    if (timerState == STOPPED && settings.timerMode)
      resetPosition();
    timerStartMs = now - timerAccumulatedMs;
    timerState = RUNNING;
    internalScheduled = false;
  }
  dirty();
}
void serviceGates() {
  const uint32_t now = micros();
  if (measureGateHigh && due(now, measureGateEndUs)) {
    digitalWrite(MEASURE_TRIG_OUTPUT, LOW);
    measureGateHigh = false;
  }
  if (internalGateHigh && due(now, internalGateEndUs))
    closeInternalGate();
}
void fireMeasureGate() {
  digitalWrite(MEASURE_TRIG_OUTPUT, HIGH);
  measureGateHigh = true;
  measureGateEndUs = micros() + uint32_t(settings.gateMs) * 1000;
}
void setClockSource(ClockSource source) {
  if (clockSource == source)
    return;
  clockSource = source;
  tempoKnown = false;
  filteredInterval = 0;
  bpm = 0;
  dirty();
}
void handleTick(uint32_t us) {
  if (tempoKnown) {
    const uint32_t interval = us - previousTickUs;
    if (interval >= EDGE_FILTER_US && interval <= EXTERNAL_TIMEOUT_MS * 1000UL) {
      // Smooth displayed tempo only; musical position follows every accepted edge.
      if (!filteredInterval)
        filteredInterval = interval;
      else
        filteredInterval = (filteredInterval * 3UL + interval) / 4;
      uint32_t calculated = (15000000UL + filteredInterval / 2) / filteredInterval;
      bpm = calculated > 999 ? 999 : calculated;
    } else {
      filteredInterval = 0;
      bpm = 0;
    }
  }
  previousTickUs = us;
  tempoKnown = true;
  if (pendingReset) {
    resetPosition();
    pendingReset = false;
    if (settings.resetMode)
      stopTimer();
  }
  // The first edge, and the first edge after RESET, is bar 1 / beat 1.
  if (!positionStarted) {
    positionStarted = true;
    fireMeasureGate();
  } else if (++sixteenth >= meter.pattern[beat - 1]) {
    sixteenth = 0;
    if (++beat > meter.beats) {
      beat = 1;
      measure = measure == 9999 ? 1 : measure + 1;
      if (++measuresUntilGate >= settings.gateEvery) {
        measuresUntilGate = 0;
        fireMeasureGate();
      }
    }
  }
  dirty();
}
void serviceInputs() {
  bool overflow;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    overflow = eventOverflow;
    eventOverflow = false;
    if (overflow)
      eventTail = eventHead;
  }
  if (overflow) {
    // Lost input history cannot be reconstructed. Recover on the next edge visibly.
    queueFault = true;
    pendingReset = true;
    tempoKnown = false;
    bpm = 0;
    dirty();
  }
  while (true) {
    InputEvent event;
    bool available = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      if (eventTail != eventHead) {
        event.us = events[eventTail].us;
        event.reset = events[eventTail].reset;
        eventTail = (eventTail + 1) & EVENT_MASK;
        available = true;
      }
    }
    if (!available)
      break;
    if (event.reset) {
      pendingReset = true;
      queueFault = false;
      dirty();
    } else {
      setClockSource(EXTERNAL_CLOCK);
      internalScheduled = false;
      handleTick(event.us);
    }
    serviceGates();
  }
}
void serviceClock() {
  bool externalActive;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Read both timestamps in the same critical section: a fresh ISR must not
    // appear to be in the future and underflow the unsigned age calculation.
    externalActive = externalPresent && uint32_t(millis() - externalLastMs) < EXTERNAL_TIMEOUT_MS;
    if (!externalActive) {
      externalPresent = false;
      externalOwnsOutput = false;
    }
  }
  if (externalActive) {
    internalScheduled = false;
    return;
  }
  if (!internalEnabled || timerState != RUNNING) {
    if (clockSource != NO_CLOCK)
      setClockSource(NO_CLOCK);
    internalScheduled = false;
    return;
  }
  setClockSource(INTERNAL_CLOCK);
  const uint32_t nowUs = micros();
  if (!internalScheduled) {
    internalNextUs = nowUs;
    internalRemainder = 0;
    internalScheduled = true;
  }
  if (!due(nowUs, internalNextUs))
    return;
  const uint16_t internalBpm = 50 + uint16_t(settings.bpmStep) * 5;
  // Fixed deadlines and fractional remainder avoid accumulated loop/rounding drift.
  const uint32_t tickUs = internalNextUs;
  internalRemainder += 15000000UL % internalBpm;
  internalNextUs += 15000000UL / internalBpm;
  if (internalRemainder >= internalBpm) {
    ++internalNextUs;
    internalRemainder -= internalBpm;
  }
  if (due(nowUs, internalNextUs)) {
    // A severe stall must not cause a burst of catch-up pulses.
    queueFault = true;
    internalNextUs = nowUs + 15000000UL / internalBpm;
  }
  bool emitted = false;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (!externalOwnsOutput) {
      digitalWrite(CLOCK_THRU_OUTPUT, HIGH);
      internalGateHigh = true;
      internalGateEndUs = nowUs + 5000;
      emitted = true;
    }
  }
  if (emitted)
    handleTick(tickUs);
}

void changeMenuValue() {
  switch (menuIndex) {
  case MENU_GATE_EVERY:
    settings.gateEvery = settings.gateEvery == 16 ? 1 : settings.gateEvery * 2;
    measuresUntilGate = 0;
    scheduleSave();
    break;
  case MENU_INT_CLOCK:
    internalEnabled = !internalEnabled;
    internalScheduled = false;
    if (!internalEnabled)
      closeInternalGate();
    break; // Deliberately OFF at boot.
  case MENU_INT_BPM:
    settings.bpmStep = (settings.bpmStep + 1) % 41;
    internalRemainder = 0;
    scheduleSave();
    break;
  case MENU_GATE_LENGTH:
    settings.gateMs = settings.gateMs == 5    ? 10
                      : settings.gateMs == 10 ? 20
                      : settings.gateMs == 20 ? 50
                                              : 5;
    scheduleSave();
    break;
  case MENU_RESET_MODE:
    settings.resetMode ^= 1;
    scheduleSave();
    break;
  case MENU_TIMER_MODE:
    settings.timerMode ^= 1;
    scheduleSave();
    break;
  case MENU_DISPLAY_VIEW:
    settings.displayView ^= 1;
    scheduleSave();
    break;
  case MENU_SAVE_EXIT:
    savePending = true;
    saveDueMs = millis();
    menuActive = false;
    meterSelection = false;
    break;
  }
  dirty();
}
void serviceButtons() {
  const uint32_t now = millis();
  meterButton.update(digitalRead(BUTTON_PIN) == LOW, now);
  timerButton.update(digitalRead(TIMER_BUTTON_PIN) == (TIMER_BUTTON_USES_PULLUP ? LOW : HIGH), now);
  if (meterButton.down && timerButton.down) {
    if (!comboActive) {
      comboActive = true;
      comboStartMs = now;
    }
    suppressButtons = true;
    if (!comboHandled && uint32_t(now - comboStartMs) >= HOLD_MS) {
      menuActive = true;
      menuIndex = 0;
      comboHandled = true;
      meterSelection = false;
      dirty();
    }
    return;
  }
  if (suppressButtons) {
    if (!meterButton.down && !timerButton.down) {
      suppressButtons = false;
      comboActive = false;
      comboHandled = false;
      timerHoldHandled = false;
    }
    return;
  }
  if (menuActive) {
    if (meterButton.released) {
      menuIndex = (menuIndex + 1) % MENU_COUNT;
      dirty();
    }
    if (timerButton.released)
      changeMenuValue();
    return;
  }
  if (meterButton.released) {
    if (meterSelection) {
      settings.meter = (settings.meter + 1) % 7;
      loadMeter();
      resetPosition();
      scheduleSave();
    }
    meterSelection = true;
    meterSelectionMs = now;
    dirty();
  }
  if (timerButton.pressed)
    timerHoldHandled = false;
  if (timerButton.down && !timerHoldHandled && uint32_t(now - timerButton.pressedMs) >= HOLD_MS) {
    stopPerformance();
    timerHoldHandled = true;
  }
  if (timerButton.released && !timerHoldHandled)
    toggleTimer();
}
void serviceTimer() {
  const uint32_t elapsed =
      timerState == RUNNING ? uint32_t(millis() - timerStartMs) : timerAccumulatedMs;
  const uint32_t seconds = elapsed / 1000;
  if (seconds != shownSeconds) {
    shownSeconds = seconds;
    dirty();
  }
  if (meterSelection && uint32_t(millis() - meterSelectionMs) >= 3000) {
    meterSelection = false;
    dirty();
  }
}
void serviceSettings() {
  settingsStore.service();
  if (savePending && due(millis(), saveDueMs) && !settingsStore.busy()) {
    settingsStore.save(settings);
    savePending = false;
    dirty();
  }
  static bool wasBusy = false;
  bool busy = settingsStore.busy();
  if (busy != wasBusy) {
    wasBusy = busy;
    dirty();
  }
}

void print2(uint8_t value) {
  if (value < 10)
    display.print('0');
  display.print(value);
}
void drawBeats() {
  uint8_t elapsed = 0;
  for (uint8_t i = 0; i < meter.beats; ++i) {
    const uint8_t x = uint16_t(128) * elapsed / meter.total;
    elapsed += meter.pattern[i];
    const uint8_t end = uint16_t(128) * elapsed / meter.total;
    const bool active = positionStarted && i == beat - 1;
    if (active)
      display.fillRect(x, 54, end - x - 2, 10, WHITE);
    else
      display.drawRect(x, 54, end - x - 2, 10, WHITE);
    display.setTextSize(1);
    display.setTextColor(active ? BLACK : WHITE);
    display.setCursor(x + (end - x - 2) / 2 - 3, 55);
    display.print(i + 1);
  }
  display.setTextColor(WHITE);
}
// Shared content keeps every status and value available in both layouts.
void drawElapsedTime(uint8_t x, uint8_t y, uint8_t size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  print2((shownSeconds / 3600) % 100);
  display.print(':');
  print2((shownSeconds / 60) % 60);
  display.print(':');
  print2(shownSeconds % 60);
}
void drawTimerState(uint8_t x, uint8_t y) {
  display.setTextSize(1);
  display.setCursor(x, y);
  if (timerState == RUNNING)
    display.print(F("RUN"));
  else if (timerState == PAUSED)
    display.print(F("PAUSE"));
  else
    display.print(F("READY"));
}
void drawClockLabel(uint8_t x, uint8_t y) {
  display.setTextSize(1);
  display.setCursor(x, y);
  if (clockSource == INTERNAL_CLOCK) {
    display.print(F("INT "));
    display.print(50 + uint16_t(settings.bpmStep) * 5);
  } else if (clockSource == EXTERNAL_CLOCK) {
    display.print(F("EXT "));
    if (bpm)
      display.print(bpm);
    else
      display.print(F("---"));
  } else
    display.print(F("NO CLK"));
}
void drawMeterBadge() {
  display.setTextSize(1);
  if (meterSelection) {
    display.fillRect(96, 0, 32, 11, WHITE);
    display.setTextColor(BLACK);
  } else
    display.drawRect(96, 0, 32, 11, WHITE);
  display.setCursor(meter.num >= 10 ? 99 : 102, 2);
  display.print(meter.num);
  display.print('/');
  display.print(meter.den);
  display.setTextColor(WHITE);
}
void drawBarBeatView() {
  drawElapsedTime(0, 1, 1);
  drawTimerState(56, 1);
  drawClockLabel(44, 15);
  display.setCursor(0, 15);
  display.print(F("BAR"));
  display.setCursor(104, 15);
  display.print(F("BEAT"));
  display.drawFastVLine(94, 25, 24, WHITE);
  display.setTextSize(3);
  display.setCursor(0, 25);
  display.print(measure);
  display.setCursor(106, 25);
  display.print(beat);
}
void drawTimerView() {
  drawTimerState(0, 1);
  drawClockLabel(44, 1);
  // Same 2x timer font as V3.9, always keeping all eight HH:MM:SS characters.
  drawElapsedTime(16, 16, 2);
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print(F("BAR"));
  display.setCursor(80, 40);
  display.print(F("BEAT"));
  display.drawFastVLine(76, 36, 15, WHITE);
  display.setTextSize(2);
  display.setCursor(24, 36);
  display.print(measure);
  display.setCursor(110, 36);
  display.print(beat);
}
void drawMain() {
  display.setTextSize(1);
  display.setCursor(88, 1);
  if (queueFault)
    display.print('!');
  else if (savePending || settingsStore.busy())
    display.print('*');
  drawMeterBadge();
  display.drawFastHLine(0, 11, 128, WHITE);
  if (settings.displayView == VIEW_TIMER)
    drawTimerView();
  else
    drawBarBeatView();
  drawBeats();
}
void drawMenu() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("SETUP"));
  display.setCursor(108, 0);
  display.print(menuIndex + 1);
  display.print('/');
  display.print(uint8_t(MENU_COUNT));
  display.drawFastHLine(0, 11, 128, WHITE);
  display.setCursor(0, 16);
  switch (menuIndex) {
  case MENU_GATE_EVERY:
    display.print(F("MEASURE OUTPUT EVERY"));
    break;
  case MENU_INT_CLOCK:
    display.print(F("INTERNAL CLOCK"));
    break;
  case MENU_INT_BPM:
    display.print(F("INTERNAL TEMPO"));
    break;
  case MENU_GATE_LENGTH:
    display.print(F("MEASURE PULSE LENGTH"));
    break;
  case MENU_RESET_MODE:
    display.print(F("RESET INPUT RESETS"));
    break;
  case MENU_TIMER_MODE:
    display.print(F("TIMER BUTTON CONTROLS"));
    break;
  case MENU_DISPLAY_VIEW:
    display.print(F("DISPLAY VIEW"));
    break;
  case MENU_SAVE_EXIT:
    display.print(F("STORE SETTINGS"));
    break;
  }
  display.setTextSize(2);
  display.setCursor(0, 31);
  switch (menuIndex) {
  case MENU_GATE_EVERY:
    display.print(settings.gateEvery);
    display.print(F(" BARS"));
    break;
  case MENU_INT_CLOCK:
    if (internalEnabled)
      display.print(F("ON"));
    else
      display.print(F("OFF"));
    break;
  case MENU_INT_BPM:
    display.print(50 + uint16_t(settings.bpmStep) * 5);
    display.print(F(" BPM"));
    break;
  case MENU_GATE_LENGTH:
    display.print(settings.gateMs);
    display.print(F(" ms"));
    break;
  case MENU_RESET_MODE:
    if (settings.resetMode)
      display.print(F("BAR+TIME"));
    else
      display.print(F("BAR"));
    break;
  case MENU_TIMER_MODE:
    if (settings.timerMode)
      display.print(F("TIME+BAR"));
    else
      display.print(F("TIME"));
    break;
  case MENU_DISPLAY_VIEW:
    if (settings.displayView == VIEW_TIMER)
      display.print(F("TIMER"));
    else
      display.print(F("BAR/BEAT"));
    break;
  case MENU_SAVE_EXIT:
    display.print(F("SAVE/EXIT"));
    break;
  }
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print(F("M:NEXT   T:"));
  display.print(menuIndex == MENU_SAVE_EXIT ? F("SAVE") : F("CHANGE"));
}
// Small I2C transactions keep gates, clock, buttons and RESET serviced between chunks.
// One framebuffer only: never redraw it while a frame is being transmitted.
bool oledTransactionOK() {
  const uint8_t status = Wire.endTransmission();
  const bool timedOut = Wire.getWireTimeoutFlag();
  Wire.clearWireTimeoutFlag();
  if (status == 0 && !timedOut)
    return true;
  oledReady = false;
  frameSending = false;
  displayFault = true;
  oledRetryMs = millis() + 2000;
  return false;
}
void initializeOLED() {
  Wire.beginTransmission(OLED_ADDRESS);
  if (!oledTransactionOK())
    return;
  // Wire is already initialized and timeout already configured.
  oledReady = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS, false, false);
  if (Wire.getWireTimeoutFlag()) {
    Wire.clearWireTimeoutFlag();
    oledReady = false;
  }
  displayFault = !oledReady;
  oledRetryMs = millis() + 2000;
  display.setTextWrap(false);
  dirty();
}
void serviceDisplay() {
  if (!oledReady) {
    // Init has several transactions; recover only when musical timing is idle.
    if (timerState != RUNNING && clockSource == NO_CLOCK && due(millis(), oledRetryMs))
      initializeOLED();
    return;
  }
  if (!frameSending) {
    if (!displayDirty || uint32_t(millis() - lastFrameMs) < FRAME_INTERVAL_MS)
      return;
    display.clearDisplay();
    display.setTextColor(WHITE);
    if (menuActive)
      drawMenu();
    else
      drawMain();
    displayDirty = false;
    lastFrameMs = millis();
    frameOffset = 0;
    frameSending = true;
    Wire.beginTransmission(OLED_ADDRESS);
    Wire.write(uint8_t(0));
    Wire.write(SSD1306_PAGEADDR);
    Wire.write(uint8_t(0));
    Wire.write(uint8_t(7));
    Wire.write(SSD1306_COLUMNADDR);
    Wire.write(uint8_t(0));
    Wire.write(uint8_t(127));
    oledTransactionOK();
    return;
  }
  Wire.beginTransmission(OLED_ADDRESS);
  Wire.write(uint8_t(0x40));
  Wire.write(display.getBuffer() + frameOffset, 16);
  if (!oledTransactionOK())
    return;
  frameOffset += 16;
  if (frameOffset == 1024)
    frameSending = false;
}
void setup() {
  // Establish safe output levels before EEPROM, heap allocation or I2C activity.
  digitalWrite(CLOCK_THRU_OUTPUT, LOW);
  pinMode(CLOCK_THRU_OUTPUT, OUTPUT);
  digitalWrite(MEASURE_TRIG_OUTPUT, LOW);
  pinMode(MEASURE_TRIG_OUTPUT, OUTPUT);
  pinMode(CLOCK_INPUT, INPUT);
  pinMode(RESET_INPUT, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TIMER_BUTTON_PIN, TIMER_BUTTON_USES_PULLUP ? INPUT_PULLUP : INPUT);
  settingsStore.load(settings);
  loadMeter();
  Wire.begin();
  Wire.setClock(400000UL);
  Wire.setWireTimeout(1500, true);
  initializeOLED();
  attachInterrupt(digitalPinToInterrupt(CLOCK_INPUT), clockChangeISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RESET_INPUT), resetISR, RISING);
  if (settingsStore.needsMigration())
    scheduleSave();
}
void loop() {
  serviceGates();
  serviceInputs();
  serviceClock();
  serviceButtons();
  serviceTimer();
  serviceSettings();
  serviceGates();
  serviceDisplay();
}
