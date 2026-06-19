#pragma once
#include <Arduino.h>

// #define PROFILE_GREENHOUSE
#define PROFILE_SWIMMINGPOND

#ifdef PROFILE_GREENHOUSE
  #define DEF_APNAME "greenhouse"
  #define DEF_APPASS "Tomatos#123"
  #define DEF_IP "192.168.1.100"
  #define DEF_SUBNETMASK "255.255.255.0"
  #define DEF_GATEWAY "192.168.1.1"
  #define DEF_HOSTNAME "geenhouse"
  #define DEF_DISPLAYNAME "Greenhouse" 

  #define DEF_MANUAL_DURATION 10

  const unsigned long timeBeforeSafemodeFallbackMillis = 60000; // after one minute of trying to connect to the wifi-network, the system will enter AP mode.

  const int DEFAULT_PIN = D1; // this is the pin for the button
  static const int controlPins[] = {D2};
  static const char* controlPinNames[] = {"valve"};
  const int controlPinCount = 1;
#endif 

#ifdef PROFILE_SWIMMINGPOND
  #define DEF_APNAME "swimmingpond"
  #define DEF_APPASS "Water#123"
  #define DEF_IP "192.168.1.100"
  #define DEF_SUBNETMASK "255.255.255.0"
  #define DEF_GATEWAY "192.168.1.1"
  #define DEF_HOSTNAME "swimmingpond"
  #define DEF_DISPLAYNAME "Swimmingpond" 

  #define DEF_MANUAL_DURATION 10

  const unsigned long timeBeforeSafemodeFallbackMillis = 60000; // after one minute of trying to connect to the wifi-network, the system will enter AP mode.

  const int DEFAULT_PIN = D1; // this is the pin for the button
  static const int controlPins[] = {D2,D5};
  static const char* controlPinNames[] = {"water", "air"};
  const int controlPinCount = 2;
#endif 