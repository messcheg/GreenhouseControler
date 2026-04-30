
#include "schedule.hpp"

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

// -----------------------------------------------------------------------------
// Internal storage
// -----------------------------------------------------------------------------

static TimeSlot schedule[MAX_SLOTS];
static int scheduleCount = 0;

static bool scheduleDirty = false;
static unsigned long lastChangeMillis = 0;

#define VERSION_MAGIC 0x53434830  // 'SCH0'
// the magic is'SCH0' -> changed from 'SCHD' because removed 'hasRun' property from schedule


// -----------------------------------------------------------------------------
// Utilities (copied verbatim from monolithic code)
// -----------------------------------------------------------------------------

static bool isLater(const TimeSlot& a, const TimeSlot& b) {
  if (a.hour != b.hour) {
    return a.hour > b.hour;
  }
  return a.minute > b.minute;
}

static bool isSlotActiveAtGivenDay(struct tm* t, TimeSlot& slot) {
  // No date restriction
  if (slot.startMonth == 0 || slot.endMonth == 0) {
    return true;
  }

  int month = t->tm_mon + 1;
  int day   = t->tm_mday;

  auto afterOrEqual = [&](int m, int d) {
    return (month > m) || (month == m && day >= d);
  };

  auto beforeOrEqual = [&](int m, int d) {
    return (month < m) || (month == m && day <= d);
  };

  bool normalRange =
    (slot.startMonth < slot.endMonth) ||
    (slot.startMonth == slot.endMonth &&
     slot.startDay <= slot.endDay);

  if (normalRange) {
    // Inclusive active window: start ≤ today ≤ end
    return afterOrEqual(slot.startMonth, slot.startDay) &&
           beforeOrEqual(slot.endMonth, slot.endDay);
  } else {
    // Inverted range: active outside the exclusion gap
    return afterOrEqual(slot.startMonth, slot.startDay) ||
           beforeOrEqual(slot.endMonth, slot.endDay);
  }
}

// -----------------------------------------------------------------------------
// Sorting (IMPORTANT: this preserves schedule invariants)
// -----------------------------------------------------------------------------

static void sortSchedule() {
  // Bubble sort, identical to original sketch
  for (int i = 0; i < scheduleCount - 1; i++) {
    for (int j = 0; j < scheduleCount - i - 1; j++) {
      if (isLater(schedule[j], schedule[j + 1])) {
        TimeSlot temp = schedule[j];
        schedule[j] = schedule[j + 1];
        schedule[j + 1] = temp;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void initSchedule() {
  if (!LittleFS.begin()) {
    LittleFS.format();
    LittleFS.begin();
  }

  if (!LittleFS.exists("/schedule.dat")) {
    scheduleCount = 0;
    return;
  }

  File f = LittleFS.open("/schedule.dat", "r");
  if (!f) {
    scheduleCount = 0;
    return;
  }

  uint32_t magic;
  f.read((uint8_t*)&magic, sizeof(magic));
  if (magic != VERSION_MAGIC) {
    f.close();
    scheduleCount = 0;
    return;
  }

  f.read((uint8_t*)&scheduleCount, sizeof(scheduleCount));

  if (scheduleCount > MAX_SLOTS) {
    f.close();
    scheduleCount = 0;
    return;
  }

  f.read((uint8_t*)schedule, sizeof(TimeSlot) * scheduleCount);
  f.close();

  // ✅ IMPORTANT:
  // We sort AFTER loading to guarantee invariant,
  // even if older firmware saved unsorted data.
  sortSchedule();
}

PinAction actionAccordingToSchedule() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  // Time not valid yet
  if (t->tm_year < (2020 - 1900)) {
    return PIN_OFF;
  }

  TimeSlot* best = nullptr;

  // Scan today
  for (int i = 0; i < scheduleCount; i++) {
    auto& slot = schedule[i];

    if (!slot.active) continue;
    if (!isSlotActiveAtGivenDay(t, slot)) continue;

    bool timeReached =
      (t->tm_hour > slot.hour) ||
      (t->tm_hour == slot.hour && t->tm_min >= slot.minute);

    if (timeReached &&
        (best == nullptr || isLater(slot, *best))) {
      best = &slot;
    }
  }

  // Yesterday fallback (CRITICAL BEHAVIOR)
  if (best == nullptr) {
    time_t yesterday = now - 86400;
    struct tm* tYesterday = localtime(&yesterday);

    for (int i = scheduleCount - 1; i >= 0; i--) {
      auto& slot = schedule[i];
      if (slot.active &&
          isSlotActiveAtGivenDay(tYesterday, slot)) {
        return slot.action;
      }
    }
  }

  return best ? best->action : PIN_OFF;
}

// -----------------------------------------------------------------------------
// Schedule mutation (sorting ALWAYS happens here)
// -----------------------------------------------------------------------------

void addSlot(const TimeSlot& slot) {
  if (scheduleCount >= MAX_SLOTS) return;

  schedule[scheduleCount++] = slot;

  sortSchedule();          // invariant enforced
  scheduleDirty = true;
  lastChangeMillis = millis();
}

void deleteSlot(int id) {
  if (id < 0 || id >= scheduleCount) return;

  for (int i = id; i < scheduleCount - 1; i++) {
    schedule[i] = schedule[i + 1];
  }

  scheduleCount--;

  // Already sorted, but invariant preserved explicitly
  // sortSchedule();
  scheduleDirty = true;
  lastChangeMillis = millis();
}

void setSlotActive(int id, bool active) {
  if (id < 0 || id >= scheduleCount) return;

  schedule[id].active = active;
  scheduleDirty = true;
  lastChangeMillis = millis();
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

void checkSchedulePersistency() {
  if (!scheduleDirty) return;
  if (millis() - lastChangeMillis < 1000) return;

  File f = LittleFS.open("/schedule.dat", "w");
  if (!f) return;

  uint32_t magic = VERSION_MAGIC;
  f.write((uint8_t*)&magic, sizeof(magic));
  f.write((uint8_t*)&scheduleCount, sizeof(scheduleCount));
  f.write((uint8_t*)schedule, sizeof(TimeSlot) * scheduleCount);
  f.close();

  scheduleDirty = false;
}

// -----------------------------------------------------------------------------
// Accessors
// -----------------------------------------------------------------------------

int getScheduleCount() {
  return scheduleCount;
}

TimeSlot getSlot(int id) {
  return schedule[id];
}
