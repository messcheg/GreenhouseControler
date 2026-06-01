#pragma once
#include <Arduino.h>

#define DEF_APNAME "greenhouse"
#define DEF_APPASS "Tomatos#123"
#define DEF_IP "192.168.1.100"
#define DEF_SUBNETMASK "255.255.255.0"
#define DEF_GATEWAY "192.168.1.1"
#define DEF_HOSTNAME "greenhouse"

#define DEF_MANUAL_DURATION 10

const unsigned long timeBeforeSafemodeFallbackMillis = 60000; // after one minute of trying to connect to the wifi-network, the system will enter AP mode.

const int CONTROL_PIN = D2;
const int DEFAULT_PIN = D1;