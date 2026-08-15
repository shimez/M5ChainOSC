#include "globals.h"

Chain               M5Chain;
ResponsiveWebServer server(80);
DNSServer           dnsServer;
Preferences         prefs;

bool isAPMode      = false;
bool chainBusReady = false;

int         deviceCount = 0;
int         knownCount  = 0;
ChainDevice devices[MAX_DEVICES];
KnownDevice knownDevices[MAX_KNOWN];

String  wifi_ssid     = "";
String  wifi_password = "";
String  osc_host      = "192.168.1.100";
int     osc_port      = 9000;
String  ipStr         = "";
String  hostStr       = "atoms3r-osc.local";
uint8_t displayRotation = 0;
UiLanguage uiLanguage = UI_LANG_ENGLISH;
bool       uiLanguageConfigured = false;

String lastOscName = "";
String lastOscAddr = "";
String lastOscVal  = "";
bool   hasOscFeedback = false;

uint8_t operation_status = 0;
uint8_t color_red[3]   = {255, 0, 0};
uint8_t color_blue[3]  = {0, 0, 255};
uint8_t color_green[3] = {0, 200, 0};
uint8_t color_orange[3] = {255, 96, 0};

unsigned long lastReenumMs          = 0;
int           lastKnownDeviceCount  = -1;
String        lastDeviceFingerprint = "";
unsigned long resetPressStart       = 0;
bool          resetInProgress       = false;
int           lastResetDrawPercent  = -1;

float clampf(float v, float lo, float hi) {
  if (lo > hi) {
    float t = lo;
    lo = hi;
    hi = t;
  }
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float mapClamped(float v, float inMin, float inMax, float outMin, float outMax) {
  if (fabsf(inMax - inMin) < 1e-6f) return outMin;
  float t = (v - inMin) / (inMax - inMin);
  return clampf(outMin + t * (outMax - outMin), outMin, outMax);
}

bool isPlaceholderUid(const String& uid) {
  return uid.startsWith("POS_");
}

String uidToString(const uint8_t* uid, uint8_t len) {
  String s = "";
  for (uint8_t i = 0; i < len; i++) {
    char b[3];
    sprintf(b, "%02X", uid[i]);
    s += b;
  }
  return s;
}

const char* typeToName(chain_device_type_t t) {
  switch (t) {
    case CHAIN_KEY_TYPE_CODE:      return "Key";
    case CHAIN_ENCODER_TYPE_CODE:  return "Encoder";
    case CHAIN_ANGLE_TYPE_CODE:    return "Angle";
    case CHAIN_JOYSTICK_TYPE_CODE: return "Joystick";
    case CHAIN_TOF_TYPE_CODE:      return "ToF";
    default:                       return "Unknown";
  }
}

String htmlEscape(const String& s) {
  String o = "";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;";
    else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;";
    else o += c;
  }
  return o;
}

String deviceDisplayName(const ChainDevice& d) {
  String n = d.displayName;
  n.trim();
  if (!n.length()) {
    n = d.uid;
    if (n.length() > 6) n = n.substring(n.length() - 6);
  }
  return n;
}

void normalizeSequence(SequenceConfig& s) {
  if (s.step == 0) s.step = 1;
  if (s.start <= s.end && s.step < 0) s.step = -s.step;
  if (s.start > s.end && s.step > 0) s.step = -s.step;
  s.current = s.start;
}
