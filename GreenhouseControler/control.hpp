#pragma once

#include <time.h>
#include <stdint.h>

// -----------------------------------------------------------------------------
// Public datatypes
// -----------------------------------------------------------------------------

enum PinAction {
  PIN_ON,
  PIN_OFF
};

enum ControlMode {
  MODE_OFF,
  MODE_FORCED_OFF,
  MODE_AUTO,
  MODE_MANUAL,
  MODE_AUTO_AND_MANUAL
};

enum SuppressionState{// combinable with bitwise operators
  SUPR_NONE = 0x0,
  SUPR_UPDATE = 0x1,  // Suppression for firmwareupdate
  SUPR_MANUAL = 0x2,   // Suppression manual
  SUPR_MANUAL_UPDATE = 0x3 // both
};

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

// Must be called once during setup()
// - Configures GPIO
// - Forces initial OFF state
void initControl(int controlPin);

// -----------------------------------------------------------------------------
// Core control logic
// -----------------------------------------------------------------------------

// Must be called repeatedly from loop()
// - Combines schedule + manual override
// - Updates GPIO
// - Updates mode and valve-off time
void checkIrrigationStatus();

// -----------------------------------------------------------------------------
// Manual override API (NO GPIO here)
// -----------------------------------------------------------------------------

void setManualOverride(int minutes);
void clearManualOverride();
void suppressOperation(bool isSuppressed, SuppressionState reason);
SuppressionState getOperationSuppressionState();

// -----------------------------------------------------------------------------
// Read-only state accessors (for API / UI)
// -----------------------------------------------------------------------------

PinAction getPinStatus();
ControlMode getControlMode();
time_t getValveOffTime();
