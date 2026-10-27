#include <cassert>
#include <fstream>
#include <iostream>
#include "Arduino.h"
#include "../Eurotrack_V4_1.ino"

void idleUs(uint64_t duration, uint32_t step = 250) {
  const uint64_t end = fakeUs + duration;
  while (fakeUs < end) {
    fakeUs += std::min<uint64_t>(step, end - fakeUs);
    loop();
  }
}
void edge(uint32_t delta = 125000) {
  fakeUs += delta;
  pins[CLOCK_INPUT] = HIGH;
  clockChangeISR();
  serviceInputs();
  serviceClock();
  pins[CLOCK_INPUT] = LOW;
  clockChangeISR();
}
void clearRuntime() {
  settings = Settings{};
  loadMeter();
  settingsStore = SettingsStore{};
  resetPosition();
  pendingReset = false;
  queueFault = false;
  eventHead = eventTail = 0;
  eventOverflow = false;
  externalPresent = false;
  externalOwnsOutput = false;
  internalEnabled = false;
  internalScheduled = false;
  internalGateHigh = false;
  measureGateHigh = false;
  clockSource = NO_CLOCK;
  bpm = 0;
  tempoKnown = false;
  filteredInterval = 0;
  edgeKnown = false;
  timerState = STOPPED;
  shownSeconds = timerAccumulatedMs = 0;
  meterButton = Button{};
  timerButton = Button{};
  timerHoldHandled = false;
  comboActive = comboHandled = suppressButtons = menuActive = meterSelection = false;
  savePending = false;
  menuIndex = 0;
  frameSending = false;
  displayDirty = false;
  fakeUs = 10000000;
  pins[BUTTON_PIN] = HIGH;
  pins[TIMER_BUTTON_PIN] = LOW;
  pins[CLOCK_INPUT] = LOW;
  Wire.status = 0;
  Wire.timeout = false;
  pinWrites.clear();
}
void settleButtons() {
  serviceButtons();
  fakeUs += 26000;
  serviceButtons();
}
void meterClick() {
  pins[BUTTON_PIN] = LOW;
  settleButtons();
  pins[BUTTON_PIN] = HIGH;
  settleButtons();
}
void timerClick() {
  pins[TIMER_BUTTON_PIN] = HIGH;
  settleButtons();
  pins[TIMER_BUTTON_PIN] = LOW;
  settleButtons();
}
void dump(const std::string &name) {
  std::ofstream out("preview/" + name + ".pgm", std::ios::binary);
  out << "P5\n128 64\n255\n";
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 128; ++x) {
      char c = (display.pixels[x + (y / 8) * 128] & (1 << (y % 8))) ? char(255) : char(0);
      out.write(&c, 1);
    }
}
void renderMain(const std::string &name) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawMain();
  dump(name);
}
void finishSave(SettingsStore &store) {
  for (int i = 0; i < 30 && store.busy(); ++i)
    store.service();
  assert(!store.busy());
}
bool equal(const Settings &a, const Settings &b) {
  return a.meter == b.meter && a.gateEvery == b.gateEvery && a.gateMs == b.gateMs &&
         a.bpmStep == b.bpmStep && a.resetMode == b.resetMode && a.timerMode == b.timerMode &&
         a.displayView == b.displayView;
}
int main() {
  setup();
  clearRuntime();
  // All seven meters: first edge is beat 1, next bar exactly total sixteenths later.
  for (uint8_t i = 0; i < 7; ++i) {
    clearRuntime();
    settings.meter = i;
    loadMeter();
    edge();
    assert(measure == 1 && beat == 1 && sixteenth == 0);
    for (uint8_t n = 1; n < meter.total; ++n)
      edge();
    assert(measure == 1);
    edge();
    assert(measure == 2 && beat == 1 && sixteenth == 0);
    assert(bpm == 120);
  }
  // Every MEASURE interval emits at bars 1, 1+N, 1+2N; all lengths expire.
  for (uint8_t interval : {1, 2, 4, 8, 16}) {
    clearRuntime();
    settings.gateEvery = interval;
    edge();
    for (int bar = 2; bar <= 34; ++bar) {
      for (int n = 0; n < 16; ++n) {
        serviceGates();
        edge();
      }
      assert(measure == bar);
      assert(measureGateHigh == (((bar - 1) % interval) == 0));
    }
  }
  for (uint8_t length : {5, 10, 20, 50}) {
    clearRuntime();
    settings.gateMs = length;
    fireMeasureGate();
    fakeUs += uint32_t(length) * 1000 - 1;
    serviceGates();
    assert(measureGateHigh);
    ++fakeUs;
    serviceGates();
    assert(!measureGateHigh);
  }
  // Grouped 7/8 is 2+2+3 eighth notes: 4+4+6 sixteenths.
  clearRuntime();
  settings.meter = 4;
  loadMeter();
  edge();
  for (int i = 0; i < 4; ++i)
    edge();
  assert(beat == 2);
  for (int i = 0; i < 4; ++i)
    edge();
  assert(beat == 3);
  for (int i = 0; i < 6; ++i)
    edge();
  assert(measure == 2 && beat == 1);
  // Ordered reset is applied to the next edge, never to an older queued edge.
  clearRuntime();
  for (int i = 0; i < 4; ++i)
    edge();
  enqueueInput(micros() + 125000, false);
  enqueueInput(micros() + 130000, true);
  serviceInputs();
  assert(beat == 2 && pendingReset);
  edge(250000);
  assert(measure == 1 && beat == 1 && sixteenth == 0 && !pendingReset);
  // RESET counter+timer mode stops time only at the synchronized edge.
  clearRuntime();
  settings.resetMode = 1;
  toggleTimer();
  edge();
  resetISR();
  serviceInputs();
  assert(timerState == RUNNING);
  edge();
  assert(timerState == STOPPED && beat == 1 && measure == 1);
  // Pulse expiration across micros wrap and millis deadline wrap.
  clearRuntime();
  fakeUs = 0xFFFFFF00ULL;
  fireMeasureGate();
  fakeUs += 9999;
  serviceGates();
  assert(measureGateHigh);
  fakeUs += 1;
  serviceGates();
  assert(!measureGateHigh);
  assert(due(5, 0xFFFFFFF0));
  assert(!due(0xFFFFFFF0, 5));
  // No measure=0; gate cadence remains independent of the display wrap.
  clearRuntime();
  settings.gateEvery = 4;
  positionStarted = true;
  measure = 9999;
  beat = 4;
  sixteenth = 3;
  measuresUntilGate = 3;
  edge();
  assert(measure == 1 && measureGateHigh && measuresUntilGate == 0);
  // Noise filter and explicit queue overflow recovery.
  clearRuntime();
  edge();
  edge(500);
  assert(sixteenth == 0);
  edge(1000);
  assert(sixteenth == 1);
  for (int i = 0; i < 20; ++i)
    enqueueInput(micros() + i * 10000, false);
  serviceInputs();
  assert(queueFault && pendingReset);
  edge();
  assert(measure == 1 && beat == 1 && sixteenth == 0);
  // Timer click/hold, bounce filtering, and chord must not change meter or timer.
  clearRuntime();
  pins[TIMER_BUTTON_PIN] = HIGH;
  serviceButtons();
  fakeUs += 5000;
  pins[TIMER_BUTTON_PIN] = LOW;
  serviceButtons();
  fakeUs += 30000;
  serviceButtons();
  assert(timerState == STOPPED);
  timerClick();
  assert(timerState == RUNNING);
  fakeUs += 2100000;
  serviceTimer();
  assert(shownSeconds == 2);
  timerClick();
  assert(timerState == PAUSED);
  timerClick();
  assert(timerState == RUNNING);
  pins[TIMER_BUTTON_PIN] = HIGH;
  settleButtons();
  fakeUs += 1000000;
  serviceButtons();
  assert(timerState == STOPPED);
  pins[TIMER_BUTTON_PIN] = LOW;
  settleButtons();
  assert(timerState == STOPPED);
  pins[BUTTON_PIN] = LOW;
  pins[TIMER_BUTTON_PIN] = HIGH;
  settleButtons();
  fakeUs += 1000000;
  serviceButtons();
  assert(menuActive && settings.meter == 0 && timerState == STOPPED);
  pins[BUTTON_PIN] = HIGH;
  pins[TIMER_BUTTON_PIN] = LOW;
  settleButtons();
  assert(menuIndex == 0);
  meterClick();
  assert(menuIndex == 1);
  timerClick();
  assert(internalEnabled);
  // External takes ownership; an old internal pulse must never pull THRU low.
  clearRuntime();
  internalEnabled = true;
  timerState = RUNNING;
  serviceClock();
  assert(internalGateHigh);
  edge(1000);
  fakeUs += 6000;
  serviceGates(); // external LOW was already mirrored by helper
  pins[CLOCK_INPUT] = HIGH;
  clockChangeISR();
  fakeUs += 6000;
  serviceGates();
  assert(pins[CLOCK_THRU_OUTPUT] == HIGH);
  assert(clockSource == EXTERNAL_CLOCK);
  serviceClock();
  assert(!internalScheduled);
  fakeUs += 1600000;
  serviceInputs();
  serviceClock();
  assert(clockSource == INTERNAL_CLOCK);
  // Fractional scheduler: 135 BPM over 1000 tick periods, without cumulative rounding drift.
  clearRuntime();
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
  // Timeout has a defined threshold, and switching OFF lowers an internal pulse.
  clearRuntime();
  edge();
  fakeUs += 1499999;
  serviceClock();
  assert(clockSource == EXTERNAL_CLOCK);
  fakeUs += 1;
  serviceClock();
  assert(clockSource == NO_CLOCK);
  internalEnabled = true;
  timerState = RUNNING;
  serviceClock();
  assert(internalGateHigh);
  menuIndex = 1;
  changeMenuValue();
  assert(!internalEnabled && !internalGateHigh && pins[CLOCK_THRU_OUTPUT] == LOW);
  // Timer and external timeout survive millis wrap.
  clearRuntime();
  fakeUs = (uint64_t(0xFFFFFFFF) - 500) * 1000;
  toggleTimer();
  edge();
  idleUs(2000000, 1000);
  assert(shownSeconds == 2 && clockSource == NO_CLOCK);
  // EEPROM defaults, legacy migration, every power-cut stage, CRC fallback, seq wrap.
  EEPROM = EEPROMStub{};
  Settings a;
  SettingsStore initial;
  initial.load(a);
  assert(a.bpmStep == 14 && initial.needsMigration());
  initial.save(a);
  finishSave(initial);
  const auto committed = EEPROM.bytes;
  Settings b = a;
  b.meter = 4;
  b.gateEvery = 16;
  b.gateMs = 50;
  b.bpmStep = 40;
  b.resetMode = 1;
  b.timerMode = 1;
  b.displayView = VIEW_TIMER;
  for (int cut = 0; cut < 14; ++cut) {
    EEPROM.bytes = committed;
    SettingsStore writer;
    Settings loaded;
    writer.load(loaded);
    writer.save(b);
    for (int j = 0; j < cut; ++j)
      writer.service();
    SettingsStore reboot;
    reboot.load(loaded);
    assert(equal(loaded, a) || equal(loaded, b));
  }
  EEPROM.bytes = committed;
  SettingsStore writer;
  Settings loaded;
  writer.load(loaded);
  writer.save(b);
  finishSave(writer);
  SettingsStore reboot;
  reboot.load(loaded);
  assert(equal(loaded, b));
  const auto bothCommitted = EEPROM.bytes;
  for (int byte = 0; byte < SettingsStore::RECORD_SIZE; ++byte)
    for (int bit = 0; bit < 8; ++bit) {
      EEPROM.bytes = bothCommitted;
      EEPROM.bytes[SettingsStore::SLOT1 + byte] ^= (1 << bit);
      SettingsStore corrupted;
      corrupted.load(loaded);
      assert(equal(loaded, a));
    }
  EEPROM.bytes = committed;
  SettingsStore cycle;
  cycle.load(loaded);
  for (int i = 0; i < 300; ++i) {
    loaded.meter = (loaded.meter + 1) % 7;
    cycle.save(loaded);
    finishSave(cycle);
  }
  Settings verify;
  SettingsStore wrapped;
  wrapped.load(verify);
  assert(equal(loaded, verify));
  unsigned writes = EEPROM.writes;
  wrapped.save(verify);
  assert(!wrapped.busy() && EEPROM.writes == writes);
  EEPROM = EEPROMStub{};
  EEPROM.bytes[0] = 6;
  EEPROM.bytes[1] = 8;
  EEPROM.bytes[3] = 20;
  SettingsStore legacy;
  legacy.load(loaded);
  assert(loaded.meter == 6 && loaded.gateEvery == 8 && loaded.bpmStep == 20 && loaded.gateMs == 10);
  // Migrate real schema-1 V4.0 records; preserve old valid record at every cut.
  EEPROM = EEPROMStub{};
  const uint8_t oldPayload[9] = {0xE4, 1, 255, 6, 8, 20, 22, 1, 1};
  for (uint8_t i = 0; i < 9; ++i)
    EEPROM.bytes[SettingsStore::SLOT0 + i] = oldPayload[i];
  EEPROM.bytes[SettingsStore::SLOT0 + 9] = SettingsStore::crc(oldPayload, 9);
  EEPROM.bytes[SettingsStore::SLOT0 + 10] = 0xA5;
  const auto oldImage = EEPROM.bytes;
  for (int cut = 0; cut < 14; ++cut) {
    EEPROM.bytes = oldImage;
    SettingsStore migration;
    Settings previous;
    migration.load(previous);
    assert(previous.meter == 6 && previous.gateEvery == 8 && previous.gateMs == 20 &&
           previous.bpmStep == 22 && previous.resetMode == 1 && previous.timerMode == 1 &&
           previous.displayView == VIEW_BAR_BEAT && migration.needsMigration());
    migration.save(previous);
    assert(migration.busy()); // Even unchanged values must upgrade the schema.
    for (int i = 0; i < cut; ++i)
      migration.service();
    Settings afterCut;
    SettingsStore boot;
    boot.load(afterCut);
    assert(equal(afterCut, previous));
  }
  EEPROM.bytes = oldImage;
  SettingsStore migrate;
  migrate.load(loaded);
  migrate.save(loaded);
  finishSave(migrate);
  SettingsStore upgraded;
  upgraded.load(loaded);
  assert(!upgraded.needsMigration() && loaded.displayView == VIEW_BAR_BEAT);
  loaded.displayView = VIEW_TIMER;
  upgraded.save(loaded);
  finishSave(upgraded);
  SettingsStore viewReboot;
  viewReboot.load(verify);
  assert(equal(loaded, verify));
  loaded.displayView = 2;
  assert(!SettingsStore::validSettings(loaded));
  viewReboot.save(loaded);
  assert(!viewReboot.busy());

  // Menu view switch is visual only, and SAVE/EXIT is now the eighth item.
  clearRuntime();
  toggleTimer();
  edge();
  measure = 128;
  beat = 3;
  sixteenth = 2;
  pendingReset = true;
  internalEnabled = true;
  internalScheduled = true;
  internalNextUs = micros() + 123456;
  const auto pinCount = pinWrites.size();
  const auto timerStart = timerStartMs;
  const auto nextTick = internalNextUs;
  const auto priorTick = previousTickUs;
  menuActive = true;
  menuIndex = MENU_DISPLAY_VIEW;
  timerClick();
  assert(settings.displayView == VIEW_TIMER && savePending && menuActive);
  assert(measure == 128 && beat == 3 && sixteenth == 2 && pendingReset && timerState == RUNNING);
  assert(timerStartMs == timerStart && internalNextUs == nextTick && previousTickUs == priorTick &&
         internalEnabled && internalScheduled && clockSource == EXTERNAL_CLOCK &&
         pinWrites.size() == pinCount);
  timerClick();
  assert(settings.displayView == VIEW_BAR_BEAT);
  meterClick();
  assert(menuIndex == MENU_SAVE_EXIT);
  timerClick();
  assert(!menuActive && savePending && due(millis(), saveDueMs));
  menuActive = true;
  menuIndex = MENU_SAVE_EXIT;
  meterClick();
  assert(menuIndex == 0);

  // OLED chunk boundaries, immutable frame, and failure degradation.
  clearRuntime();
  oledReady = true;
  displayDirty = true;
  lastFrameMs = millis() - 100;
  Wire.transactions.clear();
  serviceDisplay();
  assert(frameSending && frameOffset == 0);
  auto snapshot = std::vector<uint8_t>(display.pixels, display.pixels + 1024);
  measure = 9999;
  settings.displayView = VIEW_TIMER;
  dirty();
  for (int i = 0; i < 64; ++i)
    serviceDisplay();
  assert(!frameSending && Wire.transactions.size() == 65);
  assert(std::equal(snapshot.begin(), snapshot.end(), display.pixels));
  for (size_t i = 1; i < 65; ++i)
    assert(Wire.transactions[i].size() == 17 && Wire.transactions[i][0] == 0x40);
  fakeUs += 60000;
  serviceDisplay();
  Wire.status = 2;
  serviceDisplay();
  assert(!oledReady && displayFault && !frameSending);
  // Render both production layouts in identical states, with real Adafruit GFX.
  for (uint8_t view = VIEW_BAR_BEAT; view <= VIEW_TIMER; ++view) {
    clearRuntime();
    settings.displayView = view;
    display.setTextWrap(false);
    renderMain(view ? "05_timer_ready" : "01_ready");
    timerState = RUNNING;
    shownSeconds = 754;
    measure = 128;
    beat = 3;
    positionStarted = true;
    clockSource = EXTERNAL_CLOCK;
    bpm = 120;
    renderMain(view ? "06_timer_external" : "02_external");
    settings.meter = 6;
    loadMeter();
    meterSelection = true;
    timerState = PAUSED;
    shownSeconds = 359999;
    measure = 9999;
    beat = 4;
    savePending = true;
    renderMain(view ? "07_timer_maximum" : "03_maximum");
    meterSelection = false;
    savePending = false;
    timerState = RUNNING;
    shownSeconds = 754;
    clockSource = INTERNAL_CLOCK;
    settings.bpmStep = 40;
    measure = 42;
    renderMain(view ? "08_timer_internal" : "04_internal");
  }
  for (uint8_t i = 0; i < MENU_COUNT; ++i) {
    menuIndex = i;
    display.clearDisplay();
    drawMenu();
    dump("menu_" + std::to_string(i + 1));
  }
  menuIndex = MENU_DISPLAY_VIEW;
  settings.displayView = VIEW_BAR_BEAT;
  display.clearDisplay();
  drawMenu();
  dump("menu_view_bar_beat");
  std::cout << "PASS: existing timing/input/EEPROM regressions; V4.0 schema migration at every "
               "write phase, "
               "persistent views, visual-only menu switch, eight-item navigation, immutable frame; "
               "17 rendered screens.\n";
}
