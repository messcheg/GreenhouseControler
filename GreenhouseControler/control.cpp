#include "control.hpp"

#include <Arduino.h>
#include <time.h>

#include "schedule.hpp"

#warning "control.cpp IS being compiled"
// -----------------------------------------------------------------------------
// Internal state (copied from monolith semantics)
// -----------------------------------------------------------------------------

static int CONTROL_PIN = -1;

static PinAction pinStatus = PIN_OFF;
static ControlMode currentMode = MODE_OFF;

static time_t oneTimeTimer = 0;
static int minutesToTime = -1;

static time_t valveOffTime = 0;

// -----------------------------------------------------------------------------
// Hardware control (PRIVATE)
// -----------------------------------------------------------------------------

static void setControlPin(PinAction action, bool force) {
  if (force || pinStatus != action) {
    pinStatus = action;
    digitalWrite(
      CONTROL_PIN,
      pinStatus == PIN_ON ? HIGH : LOW
    );
  }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

void initControl(int controlPin) {
  CONTROL_PIN = controlPin;
  pinMode(CONTROL_PIN, OUTPUT);
  setControlPin(PIN_OFF, true);
}

// -----------------------------------------------------------------------------
// Manual override (EXPLICIT, no GPIO here)
// -----------------------------------------------------------------------------

void setManualOverride(int minutes) {
  oneTimeTimer = time(nullptr);
  minutesToTime = minutes;
}

void clearManualOverride() {
  minutesToTime = -1;
}

// -----------------------------------------------------------------------------
// Manual action computation (pure logic)
// -----------------------------------------------------------------------------

static PinAction actionAccordingToManual() {
  if (minutesToTime < 0) return PIN_OFF;

  time_t now = time(nullptr);
  if (now > oneTimeTimer + 60 * minutesToTime) {
    return PIN_OFF;
  }
  return PIN_ON;
}

// -----------------------------------------------------------------------------
// Main control arbitration (CALLED EVERY LOOP)
// -----------------------------------------------------------------------------

void checkIrrigationStatus() {
  time_t now = time(nullptr);

  PinAction scheduleAction = actionAccordingToSchedule();
  PinAction manualAction   = actionAccordingToManual();

  bool scheduleOn = (scheduleAction == PIN_ON);
  bool manualOn   = (manualAction   == PIN_ON);

  // Final output decision
  PinAction result =
    (scheduleOn || manualOn) ? PIN_ON : PIN_OFF;

  // Mode determination (matches monolith exactly)
  if (scheduleOn && manualOn) {
    currentMode = MODE_AUTO_AND_MANUAL;
  } else if (scheduleOn) {
    currentMode = MODE_AUTO;
  } else if (manualOn) {
    currentMode = MODE_MANUAL;
  } else {
    currentMode = MODE_OFF;
  }

  // Valve OFF time (manual dominates visibility)
  valveOffTime = 0;
  if (manualOn) {
    valveOffTime = oneTimeTimer + minutesToTime * 60;
  }

  // SINGLE GPIO DECISION POINT
  setControlPin(result, false);
}

// -----------------------------------------------------------------------------
// Accessors (read‑only)
// -----------------------------------------------------------------------------

PinAction getPinStatus() {
  return pinStatus;
}

ControlMode getControlMode() {
  return currentMode;
}

time_t getValveOffTime() {
  return valveOffTime;
}
