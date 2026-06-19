#pragma once

#define VERSION_MAGIC_V0 0x53434830  // 'SCH0'
// the magic is'SCH0' -> changed from 'SCHD' because removed 'hasRun' property from schedule

struct TimeSlot_V0 {
  uint8_t hour;
  uint8_t minute;
  PinAction action;

  uint8_t startMonth;
  uint8_t startDay;
  uint8_t endMonth;
  uint8_t endDay;

  bool active;
};
