
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
// Utilities
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
  std::sort(schedule, schedule + scheduleCount, [](const TimeSlot& a, const TimeSlot& b) {return isLater(b, a);});
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

  // IMPORTANT:
  // We sort AFTER loading to guarantee invariant,
  // even if older firmware saved unsorted data.
  sortSchedule();
}

PinAction actionAccordingToSchedule() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  // Time not valid yet, we don't accept times before 2020
  // time with fallbacks ensures that the time will be set after 2020
  // so if the time isn't 2020 yet, initialisation has failed. 
  // and the schedule should be defensive on spilling water.
  if (t->tm_year < (2020 - 1900)) {
    return PIN_OFF;
  }

  TimeSlot* previousSlot = nullptr;
  TimeSlot* nextSlot = nullptr;
  
  // Scan today based on the fact that the slots are ordered
  int slotCtr = 0;
  bool ready = false;
  while (!ready && slotCtr < scheduleCount) {
    auto& slot = schedule[slotCtr];

    if (slot.active && isSlotActiveAtGivenDay(t, slot)) 
    {
      bool toLate = (t->tm_hour < slot.hour) || (t->tm_hour == slot.hour && t->tm_min < slot.minute);

      if (toLate){
        nextSlot = &slot;
        ready = true;
      }
      else { 
        if (previousSlot == nullptr) previousSlot = &slot;
        else if (isLater(slot, *previousSlot)) previousSlot = &slot;
      }
    }
    slotCtr++;
  }

  // Yesterday fallback 
  // since this device is configured with local time, we know 
  // that there is different behaviour during change of daytime savings
  // fallback, if the previous and the next slot both are "on" we assume that 
  // we forgot an "off" and we'll turn the water off.
  // we don't want to water forever, so we will only investigate the previous and next day  
  if (previousSlot == nullptr) {
    time_t yesterday = now - 86400;
    struct tm tYesterday = *localtime(&yesterday);
    
    slotCtr = scheduleCount - 1;
    ready = false;
    while(!ready && slotCtr >= 0) {
      auto& slot = schedule[slotCtr];
      if (slot.active &&
          isSlotActiveAtGivenDay(&tYesterday, slot)) {
        previousSlot = &slot;
        ready = true;
      }
      slotCtr--;
    }
  }
  
  if (nextSlot == nullptr) {
    time_t tomorrow = now + 86400;
    struct tm tTomorrow = *localtime(&tomorrow);
    
    slotCtr = 0;
    ready = false;
    while(!ready && slotCtr < scheduleCount) {
      auto& slot = schedule[slotCtr];
      if (slot.active &&
          isSlotActiveAtGivenDay(&tTomorrow, slot)) {
        nextSlot = &slot;
        ready = true;
      }
      slotCtr++;
    }
  }
  // Situations:
  // previous | next  | result
  // ---------+-------+----------
  //  On      | Off   |  On
  //  On      | On    |  Off  (failsafe)
  //  On      | null  |  Off  (failsafe)
  //  Off     | Off   |  Off
  //  Off     | On    |  Off
  //  Off     | null  |  Off
  //  null    | On    |  Off
  //  null    | Off   |  Off
  //  null    | null  |  Off
  if ( previousSlot == nullptr || nextSlot == nullptr) return PIN_OFF;
  if ( nextSlot->action == PIN_ON) return PIN_OFF;
  return previousSlot->action;
}

// -----------------------------------------------------------------------------
// Schedule mutation (sorting ALWAYS happens here)
// -----------------------------------------------------------------------------
void setScheduleDirty()
{
  scheduleDirty = true;        // (1) The order of these two assignments
  lastChangeMillis = millis(); // (2) is essential to preserve a correct state (combined with (3) and (4))
}

void addSlot(const TimeSlot& slot) {
  if (scheduleCount < MAX_SLOTS) {
    schedule[scheduleCount++] = slot;
    sortSchedule();          // invariant enforced
    setScheduleDirty();
  }
 }

void deleteSlot(int id) {
  if (id < 0 || id >= scheduleCount) return;

  for (int i = id; i < scheduleCount - 1; i++) {
    schedule[i] = schedule[i + 1];
  }

  scheduleCount--;

  // Already sorted, but invariant preserved explicitly
  // sortSchedule();
  setScheduleDirty();
}

void setSlotActive(int id, bool active) {
  if (id < 0 || id >= scheduleCount) return;

  schedule[id].active = active;
  setScheduleDirty();
}

void updateSlot(int id, const TimeSlot& slot) {
  if (id < 0 || id >= scheduleCount) return;
  schedule[id] = slot;
  sortSchedule();
  setScheduleDirty();
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

void checkSchedulePersistency() {
  if (!scheduleDirty) return;
  if (millis() - lastChangeMillis < 1000) return;
  int changeId = 0;                      // this is a failsafe to preserve the saved state 
  while (changeId != lastChangeMillis){  // (3a) here we can check if a new change was done during our safe (combined with (1) and(2))
    changeId = lastChangeMillis;         // (3b) this has to be stored before the state is saved
    File f = LittleFS.open("/schedule.dat", "w");
    if (!f) return;

    uint32_t magic = VERSION_MAGIC;
    f.write((uint8_t*)&magic, sizeof(magic));
    f.write((uint8_t*)&scheduleCount, sizeof(scheduleCount));
    f.write((uint8_t*)schedule, sizeof(TimeSlot) * scheduleCount);
    f.close();

    scheduleDirty = false;  // (4) if another setDirty was performaed during the save, lastChangesMillis will be different.
  }
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
