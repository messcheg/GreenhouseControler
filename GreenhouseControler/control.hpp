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

enum ManualActionSource{
  MA_NONE,
  MA_SWITCH,
  MA_WEB
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
void initControl();

// -----------------------------------------------------------------------------
// Core control logic
// -----------------------------------------------------------------------------

// Must be called repeatedly from loop()
// - Combines schedule + manual override
// - Updates GPIO
// - Updates mode and valve-off time
void checkIrrigationStatus();

// -----------------------------------------------------------------------------
// Manual override API
// -----------------------------------------------------------------------------

void setManualOverride(int minutes, ManualActionSource source, int pinId=0);
void clearManualOverride(int pinId=0);
void toggleManualOverride(int minutes, ManualActionSource source, int pinId=0);
ManualActionSource getManualActionSource(int pinId=0);

void suppressOperation(bool isSuppressed, SuppressionState reason);
SuppressionState getOperationSuppressionState();

// -----------------------------------------------------------------------------
// Read-only state accessors (for API / UI)
// -----------------------------------------------------------------------------

PinAction getPinStatus(int pinId=0);
ControlMode getControlMode(int pinId=0);
time_t getValveOffTime(int pinId=0);
