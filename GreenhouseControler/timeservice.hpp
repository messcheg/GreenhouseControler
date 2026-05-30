#pragma once

#include <time.h>
#include <Arduino.h>

const static int acceptedTimeDelay = 7200;

// Must be called once after Wi‑Fi is up
void setupTime();

// Must be called periodically from loop()
void checkSavedTime();

// Utility for API / UI
String getCurrentTimeISO8601();

enum TimeSource
{
   NTP_SYNC,
   FROM_STORAGE,
   COMPILE_TIME
};

TimeSource GetcurrentTimeSource();