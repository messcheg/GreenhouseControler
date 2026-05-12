#pragma once

#include <time.h>
#include <Arduino.h>

// Must be called once after Wi‑Fi is up
void setupTime();

// Must be called periodically from loop()
void checkSavedTime();

// Utility for API / UI
String getCurrentTimeISO8601();