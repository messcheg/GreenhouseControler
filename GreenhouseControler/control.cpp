#include "control.hpp"
#include "platform.hpp"
#include <Arduino.h>
#include <time.h>
#include "definitions.hpp"
#include "schedule.hpp"

// -----------------------------------------------------------------------------
// Internal state 
// -----------------------------------------------------------------------------
struct PinControl
{
  int id = 0;
  int controlPin = -1;
  PinAction pinStatus = PIN_OFF;
  ControlMode currentMode = MODE_OFF;
  ManualActionSource manualActionSource = MA_NONE;
  time_t oneTimeTimer = 0;
  int minutesToTime = -1;
  time_t valveOffTime = 0;
};

static PinControl pins[controlPinCount];

static int suppressionState = SUPR_NONE;

// -----------------------------------------------------------------------------
// Hardware control (PRIVATE)
// -----------------------------------------------------------------------------

static void setControlPin(int pinId, PinAction action, bool force) {
  if (pinId >= controlPinCount) return;
  PinControl& pin = pins[pinId];
  if (pin.currentMode == MODE_FORCED_OFF) return;
  if (pin.controlPin == -1) return;
  if (force || pin.pinStatus != action) {
    pin.pinStatus = action;
    digitalWrite(
      pin.controlPin,
      pin.pinStatus == PIN_ON ? HIGH : LOW
    );
  }
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------

void initControl() {
  for (int i = 0; i < controlPinCount; i++) {
    pins[i].controlPin = controlPins[i];
    pinMode(controlPins[i], OUTPUT);
    setControlPin(i, PIN_OFF, true);
  }
}

// -----------------------------------------------------------------------------
// Manual override (EXPLICIT, no GPIO here)
// -----------------------------------------------------------------------------

void setManualOverride(int minutes, ManualActionSource source, int pinId) {
  if (pinId >= controlPinCount) return;
  PinControl& pin = pins[pinId];
  
  pin.oneTimeTimer = time(nullptr);
  pin.minutesToTime = minutes;
  pin.manualActionSource = source;
}

void clearManualOverride(int pinId) {
  if (pinId >= controlPinCount) return;
  PinControl& pin = pins[pinId];
  
  pin.minutesToTime = -1;
  pin.manualActionSource = MA_NONE;
}

void toggleManualOverride(int minutes, ManualActionSource source, int pinId) {
  if (pinId >= controlPinCount) return;
  PinControl& pin = pins[pinId];
  
  if( pin.minutesToTime == -1 ) setManualOverride(minutes, source, pinId);
  else clearManualOverride(pinId);
}

ManualActionSource getManualActionSource(int pinId){
  if (pinId >= controlPinCount) return MA_NONE;
  PinControl& pin = pins[pinId];
  
  return pin.manualActionSource;
}

void suppressOperation(bool isSuppressed, SuppressionState reason)
{
  if (isSuppressed ){
    for (int i = 0; i < controlPinCount; i++) {
      PinControl& pin = pins[i];
      
      if (pin.currentMode != MODE_FORCED_OFF) setControlPin(i, PIN_OFF, true);
      pin.currentMode = MODE_FORCED_OFF;
    }
    suppressionState |= reason;
  }
  else if((suppressionState & reason) > 0) {
    suppressionState &= ~reason;
    if (suppressionState == SUPR_NONE)
      for (int i = 0; i < controlPinCount; i++) {
      PinControl& pin = pins[i];
      if (pin.currentMode == MODE_FORCED_OFF) pin.currentMode = MODE_OFF; 
    }
  }
}

SuppressionState getOperationSuppressionState(){
  return (SuppressionState)suppressionState;
}

// -----------------------------------------------------------------------------
// Manual action computation (pure logic)
// -----------------------------------------------------------------------------

static PinAction actionAccordingToManual(int pinId) {
  if (pinId >= controlPinCount) return PIN_OFF;
  PinControl& pin = pins[pinId];
  
  if (pin.minutesToTime < 0) return PIN_OFF;

  time_t now = time(nullptr);
  if (now > pin.oneTimeTimer + 60 * pin.minutesToTime) {
    return PIN_OFF;
  }
  return PIN_ON;
}

// -----------------------------------------------------------------------------
// Main control arbitration (CALLED EVERY LOOP)
// -----------------------------------------------------------------------------

void checkIrrigationStatus() {
  bool anyPinOn = false;
  for (int i = 0; i < controlPinCount; i++)
  {
    PinControl& pin = pins[i];
    
    if (pin.currentMode == MODE_FORCED_OFF) continue;
    
    time_t now = time(nullptr);

    PinAction scheduleAction = actionAccordingToSchedule(i);
    PinAction manualAction   = actionAccordingToManual(i);

    bool scheduleOn = (scheduleAction == PIN_ON);
    bool manualOn   = (manualAction   == PIN_ON);

    // Final output decision
    PinAction result =
      (scheduleOn || manualOn) ? PIN_ON : PIN_OFF;

    // Mode determination (matches monolith exactly)
    if (scheduleOn && manualOn) {
      pin.currentMode = MODE_AUTO_AND_MANUAL;
      anyPinOn = true;
    } else if (scheduleOn) {
      pin.currentMode = MODE_AUTO;
      anyPinOn = true;
    } else if (manualOn) {
      pin.currentMode = MODE_MANUAL;
      anyPinOn = true;
    } else {
      pin.currentMode = MODE_OFF;
    }

    // Valve OFF time (manual dominates visibility)
    pin.valveOffTime = 0;
    if (manualOn) {
      pin.valveOffTime = pin.oneTimeTimer + pin.minutesToTime * 60;
    }

    // SINGLE GPIO DECISION POINT
    setControlPin(i, result, false);
  }
  // if any control is on --> light the led
  setLed(anyPinOn ? LED_ON : LED_OFF);
}

// -----------------------------------------------------------------------------
// Accessors (read‑only)
// -----------------------------------------------------------------------------

PinAction getPinStatus(int pinId) {
  if (pinId >= controlPinCount) return PIN_OFF;
  PinControl& pin = pins[pinId];
  return pin.pinStatus;
}

ControlMode getControlMode(int pinId) {
  if (pinId >= controlPinCount) return MODE_OFF;
  PinControl& pin = pins[pinId];
  return pin.currentMode;
}

time_t getValveOffTime(int pinId) {
  if (pinId >= controlPinCount) return MODE_OFF;
  PinControl& pin = pins[pinId];
  return pin.valveOffTime;
}
