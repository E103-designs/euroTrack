#include <cassert>
#include <fstream>
#include <iostream>
#include <array>
#include "Arduino.h"
#include "../Eurotrack_V4_2_SingleFile.ino"

void fixture() {
  settings = Settings{};
  settingsStore = SettingsStore{};
  loadMeter();
  eventHead = eventTail = 0;
  eventOverflow = false;
  externalPresent = false;
  externalOwnsOutput = false;
  lastEdgeUs = externalLastMs = 0;
  edgeKnown = false;
  measure = 1;
  beat = 1;
  sixteenth = 0;
  measuresUntilGate = 0;
  positionStarted = false;
  pendingReset = queueFault = false;
  measureGateHigh = internalGateHigh = false;
  runGateMode = runSignalHigh = runTransportHigh = false;
  runEpoch = activeRunEpoch = 0;
  measPending = false;
  measDueUs = measWidthUs = rawTickIntervalUs = 0;
  outputStep = 0;
  internalEnabled = internalScheduled = false;
  internalNextUs = internalRemainder = 0;
  clockSource = NO_CLOCK;
  tempoKnown = false;
  previousTickUs = filteredInterval = 0;
  bpm = 0;
  timerState = STOPPED;
  timerStartMs = timerAccumulatedMs = shownSeconds = 0;
  meterButton = Button{};
  timerButton = Button{};
  comboActive = comboHandled = suppressButtons = false;
  timerHoldHandled = menuActive = meterSelection = false;
  menuIndex = 0;
  savePending = false;
  displayDirty = frameSending = false;
  oledReady = true;
  displayFault = false;
  fakeUs = 10000000;
  pins[BUTTON_PIN] = HIGH;
  pins[TIMER_BUTTON_PIN] = LOW;
  pins[RESET_INPUT] = LOW;
  pins[CLOCK_INPUT] = LOW;
  Wire.status = 0;
  Wire.timeout = false;
  Wire.transactionUs = 0;
  Wire.midTransaction = nullptr;
  pinWrites.clear();
  Wire.transactions.clear();
  display.setTextWrap(false);
}
void advance(uint64_t us, uint32_t step = 250) {
  const uint64_t end = fakeUs + us;
  while (fakeUs < end) {
    fakeUs += std::min<uint64_t>(step, end - fakeUs);
    loop();
  }
}
void clockNow(bool process = true) {
  pins[CLOCK_INPUT] = HIGH;
  clockChangeISR();
  pins[CLOCK_INPUT] = LOW;
  clockChangeISR();
  if (process) {
    serviceInputs();
    serviceGates();
  }
}
void edge(uint32_t period = 125000) {
  fakeUs += period;
  serviceGates();
  clockNow();
  serviceClock();
}
void runLevel(bool high, bool process = true) {
  pins[RESET_INPUT] = high ? HIGH : LOW;
  resetISR();
  if (process) {
    serviceInputs();
    serviceGates();
    serviceTimer();
  }
}
void enableRun() {
  settings.resetMode = RESET_RUN_GATE;
  configureResetInput();
}
void finish(SettingsStore &store) {
  for (int n = 0; n < 30 && store.busy(); ++n)
    store.service();
  assert(!store.busy());
}
bool equal(const Settings &a, const Settings &b) {
  return a.meter == b.meter && a.gateEvery == b.gateEvery && a.gateMs == b.gateMs &&
         a.bpmStep == b.bpmStep && a.resetMode == b.resetMode && a.timerMode == b.timerMode &&
         a.displayView == b.displayView && a.outputMode == b.outputMode &&
         a.swingStep == b.swingStep;
}
void settle() {
  serviceButtons();
  fakeUs += 26000;
  serviceButtons();
}
void meterClick() {
  pins[BUTTON_PIN] = LOW;
  settle();
  pins[BUTTON_PIN] = HIGH;
  settle();
}
void timerClick() {
  pins[TIMER_BUTTON_PIN] = HIGH;
  settle();
  pins[TIMER_BUTTON_PIN] = LOW;
  settle();
}
std::vector<uint64_t> highs() {
  std::vector<uint64_t> values;
  for (const auto &write : pinWrites)
    if (write.pin == MEASURE_TRIG_OUTPUT && write.value == HIGH)
      values.push_back(write.us);
  return values;
}
void dump(const std::string &name, bool menu = false) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  if (menu)
    drawMenu();
  else
    drawMain();
  std::ofstream out("preview/" + name + ".pgm", std::ios::binary);
  out << "P5\n128 64\n255\n";
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 128; ++x) {
      char value = (display.pixels[x + (y / 8) * 128] & (1 << (y % 8))) ? char(255) : char(0);
      out.write(&value, 1);
    }
}
void testLegacy() {
  for (uint8_t meterIndex = 0; meterIndex < 7; ++meterIndex) {
    fixture();
    settings.meter = meterIndex;
    loadMeter();
    edge();
    assert(measure == 1 && beat == 1 && sixteenth == 0);
    for (uint8_t i = 1; i < meter.total; ++i)
      edge();
    assert(measure == 1);
    edge();
    assert(measure == 2 && beat == 1 && sixteenth == 0 && bpm == 120);
  }
  for (uint8_t every : {1, 2, 4, 8, 16}) {
    fixture();
    settings.gateEvery = every;
    edge();
    for (int bar = 2; bar <= 34; ++bar) {
      for (int i = 0; i < 16; ++i)
        edge();
      assert(measure == bar && measureGateHigh == (((bar - 1) % every) == 0));
    }
  }
  fixture();
  for (int i = 0; i < 4; ++i)
    edge();
  enqueueInput(micros() + 125000, INPUT_CLOCK);
  enqueueInput(micros() + 126000, INPUT_RESET);
  serviceInputs();
  assert(beat == 2 && pendingReset);
  edge(250000);
  assert(measure == 1 && beat == 1 && sixteenth == 0 && !pendingReset);
  fixture();
  settings.resetMode = RESET_BAR_TIMER;
  toggleTimer();
  edge();
  runLevel(true);
  assert(timerState == RUNNING);
  edge();
  assert(timerState == STOPPED);
  for (uint8_t length : {5, 10, 20, 50}) {
    fixture();
    settings.gateMs = length;
    fakeUs = 0xFFFFFF00ULL;
    fireMeasureGate();
    fakeUs += uint32_t(length) * 1000 - 1;
    serviceGates();
    assert(measureGateHigh);
    ++fakeUs;
    serviceGates();
    assert(!measureGateHigh);
  }
  fixture();
  edge();
  edge(500);
  assert(sixteenth == 0);
  edge(1000);
  assert(sixteenth == 1);
  for (int i = 0; i < 20; ++i)
    enqueueInput(micros(), INPUT_CLOCK);
  serviceInputs();
  assert(queueFault && pendingReset);
  edge();
  assert(measure == 1 && beat == 1);
  fixture();
  timerClick();
  assert(timerState == RUNNING);
  advance(2100000);
  assert(shownSeconds == 2);
  timerClick();
  assert(timerState == PAUSED);
  timerClick();
  assert(timerState == RUNNING);
  pins[TIMER_BUTTON_PIN] = HIGH;
  settle();
  fakeUs += 1000000;
  serviceButtons();
  assert(timerState == STOPPED);
  pins[TIMER_BUTTON_PIN] = LOW;
  settle();
  assert(timerState == STOPPED);
  fixture();
  pins[BUTTON_PIN] = LOW;
  pins[TIMER_BUTTON_PIN] = HIGH;
  settle();
  fakeUs += 1000000;
  serviceButtons();
  assert(menuActive && timerState == STOPPED && settings.meter == 0);
  pins[BUTTON_PIN] = HIGH;
  pins[TIMER_BUTTON_PIN] = LOW;
  settle();
  for (uint8_t i = 1; i < MENU_COUNT; ++i) {
    meterClick();
    assert(menuIndex == i);
  }
  meterClick();
  assert(menuIndex == 0);
  menuIndex = MENU_DISPLAY_VIEW;
  timerClick();
  assert(settings.displayView == VIEW_TIMER && timerState == STOPPED);
  fixture();
  internalEnabled = true;
  timerState = RUNNING;
  settings.bpmStep = 17;
  serviceClock();
  const uint32_t first = internalNextUs;
  for (int i = 0; i < 1000; ++i) {
    fakeUs = internalNextUs;
    serviceClock();
    serviceGates();
  }
  assert(uint32_t(internalNextUs - first) == uint32_t(1000ULL * 15000000 / 135));
  // A first clock after an idle longer than one micros() cycle is not debounced away.
  fixture();
  edge();
  fakeUs += 0x100000000ULL + 500;
  serviceClock();
  clockNow();
  assert(sixteenth == 1);
  // Existing THRU priority remains intact.
  fixture();
  internalEnabled = true;
  timerState = RUNNING;
  serviceClock();
  pins[CLOCK_INPUT] = HIGH;
  fakeUs += 1000;
  clockChangeISR();
  serviceInputs();
  fakeUs += 10000;
  serviceGates();
  assert(pins[CLOCK_THRU_OUTPUT] == HIGH);
}
void testRunTransport() {
  fixture();
  enableRun();
  edge();
  assert(!positionStarted && timerState == STOPPED);
  const uint64_t start = fakeUs;
  runLevel(true);
  assert(timerState == RUNNING && shownSeconds == 0);
  fakeUs += 2000;
  clockNow();
  assert(positionStarted && beat == 1 && measure == 1 && measureGateHigh);
  for (int i = 0; i < 20; ++i)
    edge();
  const auto stoppedBar = measure;
  const auto stoppedBeat = beat;
  const auto stoppedSub = sixteenth;
  const uint64_t stop = fakeUs + 23000;
  fakeUs = stop;
  runLevel(false);
  const uint32_t duration = uint32_t((stop - start) / 1000);
  assert(timerState == PAUSED && timerAccumulatedMs == duration && !measureGateHigh &&
         !measPending);
  assert(pins[MEASURE_TRIG_OUTPUT] == LOW);
  for (int i = 0; i < 20; ++i)
    edge();
  assert(measure == stoppedBar && beat == stoppedBeat && sixteenth == stoppedSub);
  serviceTimer();
  assert(shownSeconds == duration / 1000);
  timerClick();
  assert(timerState == PAUSED); // Jack owns transport.
  runLevel(true);
  assert(timerState == RUNNING && shownSeconds == 0 && measure == 1 && beat == 1 &&
         !positionStarted);
  clockNow();
  assert(positionStarted && sixteenth == 0 && measureGateHigh);
  const auto origin = timerStartMs;
  runLevel(true);
  assert(timerStartMs == origin); // Spurious same-level ISR.

  // RUN timing uses the captured edge, including when main-loop handling is delayed.
  fixture();
  enableRun();
  runLevel(true, false);
  const uint32_t riseMs = millis();
  fakeUs += 2500000;
  runLevel(false, false);
  const uint32_t fallMs = millis();
  fakeUs += 100000;
  serviceInputs();
  serviceTimer();
  assert(timerAccumulatedMs == fallMs - riseMs && shownSeconds == 2 && timerState == PAUSED);
  // Long periods cross micros wrap, then millis wrap, independently.
  for (uint64_t origin : {5000000000ULL, (uint64_t(0xFFFFFFFF) - 1000) * 1000}) {
    fixture();
    fakeUs = origin;
    enableRun();
    runLevel(true);
    advance(3000000);
    runLevel(false);
    assert(timerAccumulatedMs == 3000 && shownSeconds == 3);
  }
  // HIGH already present when firmware boots is honored without a new rising edge.
  fixture();
  EEPROM = EEPROMStub{};
  Settings saved;
  saved.resetMode = RESET_RUN_GATE;
  SettingsStore writer;
  writer.save(saved);
  finish(writer);
  pins[RESET_INPUT] = HIGH;
  setup();
  assert(runGateMode && runTransportHigh && timerState == RUNNING);
  // Selecting the new menu role also reads the live level.
  fixture();
  settings.resetMode = RESET_BAR_TIMER;
  pins[RESET_INPUT] = HIGH;
  menuIndex = MENU_RESET_MODE;
  changeMenuValue();
  assert(settings.resetMode == RESET_RUN_GATE && runTransportHigh && timerState == RUNNING);
  // Internal clock obeys the same physical RUN gate.
  fixture();
  internalEnabled = true;
  enableRun();
  serviceClock();
  assert(!internalGateHigh);
  runLevel(true);
  serviceClock();
  assert(internalGateHigh && measureGateHigh);
  runLevel(false, false);
  assert(pins[MEASURE_TRIG_OUTPUT] == LOW && pins[CLOCK_THRU_OUTPUT] == LOW);
  serviceInputs();
  serviceClock();
  assert(!internalScheduled && !measureGateHigh);
  // Overflow fails closed until a fresh LOW/HIGH run cycle.
  fixture();
  enableRun();
  runLevel(true);
  clockNow();
  for (int i = 0; i < 20; ++i)
    enqueueInput(micros(), INPUT_CLOCK);
  serviceInputs();
  assert(queueFault && !runTransportHigh && timerState == STOPPED);
  edge();
  assert(!measureGateHigh);
  runLevel(false);
  runLevel(true);
  assert(!queueFault && runTransportHigh);
  edge();
  assert(measure == 1 && beat == 1);
}
void testRhythms() {
  // RUN is authoritative for every output mode, including pending swing hits.
  for (uint8_t mode = MEAS_BARS; mode < MEAS_MODE_COUNT; ++mode) {
    fixture();
    settings.outputMode = mode;
    enableRun();
    runLevel(true);
    clockNow();
    for (int i = 0; i < 5; ++i)
      edge();
    runLevel(false);
    const auto count = highs().size();
    const auto oldBar = measure;
    const auto oldBeat = beat;
    const auto oldSub = sixteenth;
    for (int i = 0; i < 8; ++i)
      edge();
    assert(highs().size() == count && measure == oldBar && beat == oldBeat && sixteenth == oldSub);
    runLevel(true);
    edge();
    assert(highs().size() == count + 1 && measure == 1 && beat == 1 && outputStep <= 1);
  }
  const std::array<std::vector<uint8_t>, 4> steps = {
      {{0, 3, 4, 6, 8, 11, 12, 14}, {0, 6, 12, 20, 26}, {0, 6, 12, 20, 24}, {0, 6, 12}}};
  for (uint8_t mode = MEAS_CLOCK; mode < MEAS_MODE_COUNT; ++mode) {
    for (uint8_t swing = 0; swing <= 5; ++swing) {
      fixture();
      settings.outputMode = mode;
      settings.swingStep = swing;
      const uint64_t base = fakeUs;
      const uint64_t period = 125000;
      for (int i = 0; i < 64; ++i) {
        clockNow();
        advance(period);
      }
      const auto actual = highs();
      std::vector<uint64_t> expected;
      for (uint8_t i = 0; i < 64; ++i) {
        bool hit = true;
        uint64_t delay = 0;
        if (mode == MEAS_SWING && (i & 1))
          delay = period * swing / 10;
        if (mode >= MEAS_SAMBA) {
          const auto &pattern = steps[mode - MEAS_SAMBA];
          const uint8_t length = (mode == MEAS_SAMBA || mode == MEAS_TRESILLO) ? 16 : 32;
          hit = std::find(pattern.begin(), pattern.end(), i % length) != pattern.end();
        }
        if (hit)
          expected.push_back(base + i * period + delay);
      }
      assert(actual == expected);
    }
  }
  // Max swing and long pulses retain a genuine LOW gap at high BPM.
  fixture();
  settings.outputMode = MEAS_SWING;
  settings.swingStep = 5;
  settings.gateMs = 50;
  const uint64_t base = fakeUs;
  for (int i = 0; i < 16; ++i) {
    clockNow();
    advance(60000);
  }
  bool high = false;
  uint64_t rise = 0, lastFall = 0;
  for (const auto &w : pinWrites)
    if (w.pin == MEASURE_TRIG_OUTPUT) {
      if (w.value == HIGH) {
        assert(!high);
        if (lastFall)
          assert(w.us > lastFall);
        high = true;
        rise = w.us;
      } else if (high) {
        assert(w.us - rise <= 15000);
        high = false;
        lastFall = w.us;
      }
    }
  assert(!high && highs().front() == base);
  // Delayed swing cannot survive RUN LOW, including a LOW/HIGH pair before loop().
  fixture();
  settings.outputMode = MEAS_SWING;
  enableRun();
  runLevel(true);
  clockNow();
  advance(125000);
  clockNow();
  assert(measPending);
  const auto count = highs().size();
  fakeUs += 1000;
  runLevel(false, false);
  assert(pins[MEASURE_TRIG_OUTPUT] == LOW);
  fakeUs += 1000;
  runLevel(true, false);
  fakeUs += 30000;
  serviceGates();
  assert(highs().size() == count);
  serviceInputs();
  assert(!measPending && outputStep == 0);
  clockNow();
  assert(highs().size() == count + 1);
  // A falling RUN edge during OLED I2C silences MEAS inside the interrupt callback.
  fixture();
  enableRun();
  runLevel(true);
  clockNow();
  assert(measureGateHigh);
  Wire.transactionUs = 1000;
  Wire.midTransaction = []() {
    runLevel(false, false);
    assert(pins[MEASURE_TRIG_OUTPUT] == LOW);
  };
  Wire.beginTransmission(OLED_ADDRESS);
  oledTransactionOK();
  serviceInputs();
  assert(timerState == PAUSED && !measureGateHigh);
  // Stale delayed events are discarded rather than emitted in a recovery burst.
  fixture();
  settings.outputMode = MEAS_SWING;
  clockNow();
  advance(125000);
  clockNow();
  assert(measPending);
  fakeUs += 100000;
  const auto oldCount = highs().size();
  serviceGates();
  assert(!measPending && queueFault && highs().size() == oldCount);
  // Mode/swing changes and RESET erase pending events and restart pattern phase.
  fixture();
  settings.outputMode = MEAS_SWING;
  clockNow();
  advance(125000);
  clockNow();
  menuIndex = MENU_OUTPUT_MODE;
  changeMenuValue();
  assert(!measPending && outputStep == 0 && settings.outputMode == MEAS_SAMBA);
  edge();
  assert(outputStep == 1);
  runLevel(true);
  edge();
  assert(outputStep == 1 && measure == 1 && beat == 1);
  // Manual pause cancels pending swing from the internal clock as well.
  fixture();
  settings.outputMode = MEAS_SWING;
  internalEnabled = true;
  toggleTimer();
  serviceClock();
  advance(125000);
  assert(measPending);
  toggleTimer();
  assert(timerState == PAUSED && !measPending && !measureGateHigh);
  // Micros wrap in a delayed swing event.
  fixture();
  settings.outputMode = MEAS_SWING;
  fakeUs = 0xFFFF0000ULL;
  clockNow();
  advance(125000);
  clockNow();
  const uint64_t dueTime = fakeUs + 25000;
  advance(25000);
  assert(highs().back() == dueTime);
}
void testSettings() {
  fixture();
  EEPROM = EEPROMStub{};
  Settings defaults;
  SettingsStore empty;
  empty.load(defaults);
  assert(defaults.outputMode == MEAS_BARS && defaults.swingStep == 2 && empty.needsMigration());
  empty.save(defaults);
  finish(empty);
  const auto committed = EEPROM.bytes;
  Settings changed = defaults;
  changed.resetMode = RESET_RUN_GATE;
  changed.outputMode = MEAS_BOSSA;
  changed.swingStep = 5;
  changed.displayView = VIEW_TIMER;
  changed.meter = 6;
  changed.timerMode = 1;
  for (int cut = 0; cut < 14; ++cut) {
    EEPROM.bytes = committed;
    SettingsStore writer;
    Settings value;
    writer.load(value);
    writer.save(changed);
    for (int i = 0; i < cut; ++i)
      writer.service();
    SettingsStore reboot;
    reboot.load(value);
    assert(equal(value, defaults) || equal(value, changed));
  }
  EEPROM.bytes = committed;
  SettingsStore writer;
  Settings value;
  writer.load(value);
  writer.save(changed);
  finish(writer);
  const auto both = EEPROM.bytes;
  for (int byte = 0; byte < 11; ++byte)
    for (int bit = 0; bit < 8; ++bit) {
      EEPROM.bytes = both;
      EEPROM.bytes[SettingsStore::SLOT1 + byte] ^= 1 << bit;
      SettingsStore boot;
      boot.load(value);
      assert(equal(value, defaults));
    }
  for (uint8_t schema : {1, 2}) {
    EEPROM = EEPROMStub{};
    uint8_t record[11] = {0xE4, schema, 255, 6, 8, 20, 22, 1, uint8_t(schema == 1 ? 1 : 3),
                          0,    0xA5};
    record[9] = SettingsStore::crc(record, 9);
    for (int i = 0; i < 11; ++i)
      EEPROM.bytes[32 + i] = record[i];
    const auto legacy = EEPROM.bytes;
    for (int cut = 0; cut < 14; ++cut) {
      EEPROM.bytes = legacy;
      SettingsStore migrate;
      migrate.load(value);
      assert(value.meter == 6 && value.gateEvery == 8 && value.gateMs == 20 &&
             value.bpmStep == 22 && value.resetMode == 1 && value.timerMode == 1 &&
             value.displayView == (schema == 1 ? VIEW_BAR_BEAT : VIEW_TIMER) &&
             value.outputMode == MEAS_BARS && value.swingStep == 2 && migrate.needsMigration());
      const auto before = value;
      migrate.save(value);
      for (int i = 0; i < cut; ++i)
        migrate.service();
      SettingsStore boot;
      boot.load(value);
      assert(equal(value, before));
    }
    EEPROM.bytes = legacy;
    SettingsStore migrate;
    migrate.load(value);
    migrate.save(value);
    finish(migrate);
    SettingsStore boot;
    boot.load(value);
    assert(!boot.needsMigration());
  }
  EEPROM.bytes = committed;
  SettingsStore sequence;
  sequence.load(value);
  for (int i = 0; i < 300; ++i) {
    value.outputMode = (value.outputMode + 1) % MEAS_MODE_COUNT;
    sequence.save(value);
    finish(sequence);
  }
  Settings roundTrip;
  SettingsStore boot;
  boot.load(roundTrip);
  assert(equal(value, roundTrip));
  const auto writes = EEPROM.writes;
  boot.save(roundTrip);
  assert(!boot.busy() && EEPROM.writes == writes);
  value.swingStep = 6;
  assert(!SettingsStore::validSettings(value));
  value = defaults;
  value.outputMode = MEAS_MODE_COUNT;
  assert(!SettingsStore::validSettings(value));
  value = defaults;
  value.resetMode = 3;
  assert(!SettingsStore::validSettings(value));
}
void testOLEDAndRender() {
  fixture();
  displayDirty = true;
  lastFrameMs = millis() - 100;
  serviceDisplay();
  const auto snapshot = std::vector<uint8_t>(display.pixels, display.pixels + 1024);
  settings.displayView = VIEW_TIMER;
  dirty();
  for (int i = 0; i < 64; ++i)
    serviceDisplay();
  assert(!frameSending && std::equal(snapshot.begin(), snapshot.end(), display.pixels));
  fakeUs += 60000;
  serviceDisplay();
  Wire.status = 2;
  serviceDisplay();
  assert(!oledReady && displayFault);
  fixture();
  enableRun();
  dump("run_stopped_bar");
  runLevel(true);
  clockNow();
  shownSeconds = 754;
  measure = 128;
  beat = 3;
  bpm = 120;
  dump("run_running_bar");
  settings.displayView = VIEW_TIMER;
  dump("run_running_timer");
  runLevel(false);
  shownSeconds = 754;
  dump("run_stopped_timer");
  fixture();
  menuActive = true;
  for (uint8_t i = 0; i < MENU_COUNT; ++i) {
    menuIndex = i;
    dump("menu_" + std::to_string(i + 1), true);
  }
  menuIndex = MENU_RESET_MODE;
  settings.resetMode = RESET_RUN_GATE;
  dump("menu_run_gate", true);
  menuIndex = MENU_OUTPUT_MODE;
  for (uint8_t i = 0; i < MEAS_MODE_COUNT; ++i) {
    settings.outputMode = i;
    dump("menu_output_" + std::to_string(i), true);
  }
  for (uint8_t view = 0; view < 2; ++view) {
    settings.displayView = view;
    settings.meter = 3;
    loadMeter();
    measure = 9999;
    beat = 5;
    timerState = PAUSED;
    shownSeconds = 359999;
    savePending = true;
    meterSelection = true;
    dump(view ? "maximum_timer" : "maximum_bar");
  }
}
int main() {
  setup();
  testLegacy();
  testRunTransport();
  testRhythms();
  testSettings();
  testOLEDAndRender();
  std::cout
      << "PASS: legacy counters/buttons/reset/clock, slave RUN edges/boot/rollovers/overflow, "
         "all 7 MEAS modes x 6 swing values, gate widths, ISR stop during I2C, no stale bursts, "
         "schema 1/2 migration and power cuts, CRC/sequence, views/menu/OLED.\n";
}
