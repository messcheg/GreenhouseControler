#pragma once

#include <stdint.h>
#include <time.h>
#include "control.hpp"


#define MAX_SLOTS 20

// -----------------------------------------------------------------------------
// Data structure (must match persisted layout exactly)
// -----------------------------------------------------------------------------

struct TimeSlot {
  uint8_t hour;
  uint8_t minute;
  PinAction action;

  uint8_t startMonth;
  uint8_t startDay;
  uint8_t endMonth;
  uint8_t endDay;

  bool active;
};

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

// Must be called once during setup()
// - Mounts LittleFS
// - Loads schedule from flash
// - Enforces sorted invariant
void initSchedule();

// -----------------------------------------------------------------------------
// Core scheduling logic
// -----------------------------------------------------------------------------

// Computes the desired output state at the current time
// - State-based (not edge-triggered)
// - Includes yesterday fallback
// - Seasonal logic preserved
PinAction actionAccordingToSchedule();

// -----------------------------------------------------------------------------
// Schedule mutation (sorting happens internally)
// -----------------------------------------------------------------------------

void addSlot(const TimeSlot& slot);
void deleteSlot(int id);
void setSlotActive(int id, bool active);

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

// Call periodically from loop()
// Saves schedule to flash after debounce delay
void checkSchedulePersistency();

// -----------------------------------------------------------------------------
// Accessors (read-only)
// -----------------------------------------------------------------------------

int getScheduleCount();
TimeSlot getSlot(int id);