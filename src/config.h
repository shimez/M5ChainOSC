#pragma once

#include <Arduino.h>
#include "M5Chain.h"

// ---------------------------------------------------------------------------
// Hardware / bus
// ---------------------------------------------------------------------------
#define RXD_PIN GPIO_NUM_6
#define TXD_PIN GPIO_NUM_5

// ---------------------------------------------------------------------------
// Limits
// ---------------------------------------------------------------------------
static const int MAX_DEVICES = 8;
static const int MAX_KNOWN   = 16;

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
static const unsigned long REENUM_INTERVAL_MS = 1000;
static const unsigned long RESET_HOLD_MS      = 10000;
static const unsigned long WIFI_TIMEOUT_MS    = 15000;

// ---------------------------------------------------------------------------
// AP mode (first-time WiFi setup)
// ---------------------------------------------------------------------------
static const char* AP_SSID     = "AtomS3R-OSC";
static const char* AP_PASSWORD = "12345678";
static const byte  DNS_PORT    = 53;

// ---------------------------------------------------------------------------
// Value / mode enums
// ---------------------------------------------------------------------------
enum ValueType : uint8_t {
  TYPE_FLOAT  = 0,
  TYPE_INT    = 1,
  TYPE_STRING = 2
};

enum KeyMode : uint8_t {
  MODE_PRESS_RELEASE = 0,
  MODE_SEQUENCE      = 1
};
