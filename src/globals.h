#pragma once

#include <M5Unified.h>
#include "M5Chain.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOSCWiFi.h>
#include <ESPmDNS.h>

#include "config.h"
#include "types.h"

// ---------------------------------------------------------------------------
// Shared hardware / services
// ---------------------------------------------------------------------------
extern Chain       M5Chain;
extern WebServer   server;
extern DNSServer   dnsServer;
extern Preferences prefs;

// ---------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------
extern bool isAPMode;
extern bool chainBusReady;

extern int         deviceCount;
extern int         knownCount;
extern ChainDevice devices[MAX_DEVICES];
extern KnownDevice knownDevices[MAX_KNOWN];

extern String wifi_ssid;
extern String wifi_password;
extern String osc_host;
extern int    osc_port;
extern String ipStr;
extern String hostStr;
extern uint8_t displayRotation;

extern String lastOscName;
extern String lastOscAddr;
extern String lastOscVal;
extern bool   hasOscFeedback;

extern uint8_t operation_status;
extern uint8_t color_red[3];
extern uint8_t color_blue[3];
extern uint8_t color_green[3];
extern uint8_t color_orange[3];

extern unsigned long lastReenumMs;
extern int           lastKnownDeviceCount;
extern String        lastDeviceFingerprint;
extern unsigned long resetPressStart;
extern bool          resetInProgress;
extern int           lastResetDrawPercent;

// ---------------------------------------------------------------------------
// Small shared helpers
// ---------------------------------------------------------------------------
float clampf(float v, float lo, float hi);
float mapClamped(float v, float inMin, float inMax, float outMin, float outMax);

bool isPlaceholderUid(const String& uid);
String uidToString(const uint8_t* uid, uint8_t len);
const char* typeToName(chain_device_type_t t);
String htmlEscape(const String& s);
String deviceDisplayName(const ChainDevice& d);

void normalizeSequence(SequenceConfig& s);
