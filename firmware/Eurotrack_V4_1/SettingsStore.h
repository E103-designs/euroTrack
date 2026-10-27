#pragma once
#include <stdint.h>
#include <EEPROM.h>
#include <util/atomic.h>

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
