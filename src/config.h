#pragma once

#include <Arduino.h>
#include "M5Chain.h"

static const char* APP_VERSION = "1.5.2";

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
static const int MAX_KEY_OSC_MESSAGES = 8;
static const size_t MAX_OSC_ADDRESS_BYTES = 192;
static const size_t MAX_OSC_VALUE_BYTES = 128;
static const size_t MAX_DEVICE_NAME_BYTES = 64;
static const size_t MAX_DEVICE_CONFIG_BYTES = 3800;
static const uint16_t SETTINGS_SCHEMA_VERSION = 2;
static const char* SETTINGS_FORMAT_NAME = "M5ChainOSC-settings";

// Enable only while diagnosing NVS persistence.
#define M5CHAINOSC_STORAGE_DEBUG 0

// Enable only while diagnosing repeated Web UI reload performance.
#define M5CHAINOSC_WEB_PERF_DEBUG 0

// Enable only while measuring internal RAM and PSRAM usage.
#define M5CHAINOSC_MEMORY_DEBUG 0

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

enum UiLanguage : uint8_t {
  UI_LANG_ENGLISH  = 0,
  UI_LANG_JAPANESE = 1
};
