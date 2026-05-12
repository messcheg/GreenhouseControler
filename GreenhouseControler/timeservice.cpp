#include "timeservice.hpp"

#include <LittleFS.h>
#include <sys/time.h>

#warning "timeservice.cpp is being compiled"
// -----------------------------------------------------------------------------
// Internal data
// -----------------------------------------------------------------------------

struct TimeBackup {
  uint32_t unixTime;
  uint32_t millisAtSave;
};

static time_t lastSavedTime = 0;

// -----------------------------------------------------------------------------
// Utilities
// -----------------------------------------------------------------------------

String getCurrentTimeISO8601() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
  return String(buffer);
}

static void setSystemTime(time_t restoredTime) {
  struct timeval tv;
  tv.tv_sec = restoredTime;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
}

// -----------------------------------------------------------------------------
// Persistence
// -----------------------------------------------------------------------------

static void saveTimeToFS(time_t now) {
  TimeBackup backup;
  backup.unixTime = now;
  backup.millisAtSave = millis();

  File f = LittleFS.open("/time.dat", "w");
  if (!f) return;

  f.write((uint8_t*)&backup, sizeof(backup));
  f.close();
}

static bool restoreTimeFromFS(time_t& restoredTime) {
  if (!LittleFS.exists("/time.dat")) return false;

  File f = LittleFS.open("/time.dat", "r");
  if (!f || f.size() != sizeof(TimeBackup)) {
    if (f) f.close();
    return false;
  }

  TimeBackup backup;
  f.read((uint8_t*)&backup, sizeof(backup));
  f.close();

  if (backup.unixTime < 1609459200UL) { // Jan 1, 2021
    return false;
  }

  uint32_t nowMillis = millis();
  uint32_t elapsedMillis =
    (nowMillis >= backup.millisAtSave)
      ? (nowMillis - backup.millisAtSave)
      : (UINT32_MAX - backup.millisAtSave + nowMillis + 1);

  restoredTime = backup.unixTime + (elapsedMillis / 1000);
  return true;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void setupTime() {

  // CET / CEST
  const long GMT_OFFSET_SEC = 3600;
  const int DAYLIGHT_OFFSET_SEC = 3600;

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov"
  );

  time_t now = time(nullptr);
  unsigned long start = millis();

  // Wait up to 15 seconds for NTP
  while (now < 100000 && millis() - start < 15000) {
    delay(500);
    now = time(nullptr);
  }

  if (now >= 100000) {
    saveTimeToFS(now);
    lastSavedTime = now;
    return;
  }

  // Fallback #1: filesystem
  time_t restored;
  if (restoreTimeFromFS(restored)) {
    restored += 25; // compensate startup delay
    setSystemTime(restored);
    lastSavedTime = restored;
    return;
  }

  // Fallback #2: build time
  struct tm tmBuild{};
  char monthStr[4];

  sscanf(__DATE__, "%3s %d %d",
         monthStr,
         &tmBuild.tm_mday,
         &tmBuild.tm_year);

  sscanf(__TIME__, "%d:%d:%d",
         &tmBuild.tm_hour,
         &tmBuild.tm_min,
         &tmBuild.tm_sec);

  tmBuild.tm_year -= 1900;

  const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  tmBuild.tm_mon = (strstr(months, monthStr) - months) / 3;

  time_t buildTime = mktime(&tmBuild);
  buildTime += 30; // compensate upload/startup delay
  setSystemTime(buildTime);

  lastSavedTime = buildTime;
}

void checkSavedTime() {
  time_t now = time(nullptr);
  if (now > lastSavedTime + 300) {
    saveTimeToFS(now);
    lastSavedTime = now;
  }
}