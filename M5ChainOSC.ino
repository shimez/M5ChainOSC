/*
 * M5 AtomS3R + Atomic ToChain Base + Chain OSC (VRChat向け)
 * - 切断時 devices[] 完全クリア / registerKnownDevice は Save 時のみ
 * - 画面回転: 現在角度ボタンに *
 * - Joystick: X/Y 軸それぞれ Invert
 */

#include <M5Unified.h>
#include "M5Chain.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoOSCWiFi.h>
#include <ESPmDNS.h>

#define RXD_PIN GPIO_NUM_6
#define TXD_PIN GPIO_NUM_5

const char* AP_SSID = "AtomS3R-OSC";
const char* AP_PASSWORD = "12345678";
const byte DNS_PORT = 53;

const int MAX_DEVICES = 8;
const int MAX_KNOWN = 16;
const unsigned long REENUM_INTERVAL_MS = 1000;
const unsigned long RESET_HOLD_MS = 10000;

enum ValueType { TYPE_FLOAT = 0, TYPE_INT = 1, TYPE_STRING = 2 };
enum KeyMode { MODE_PRESS_RELEASE = 0, MODE_SEQUENCE = 1 };

struct OSCMessage {
  String address = "";
  String valueStr = "1.0";
  ValueType valueType = TYPE_FLOAT;
};

struct SequenceConfig {
  String address = "/seq";
  ValueType valueType = TYPE_FLOAT;
  float start = 0, end = 10, step = 1, current = 0;
};

struct RangeMap {
  float inMin = 0, inMax = 4095;
  float outMin = 0, outMax = 1;
  ValueType outType = TYPE_FLOAT;
};

struct EncoderOscConfig {
  String rotAddr = "/avatar/parameters/Encoder";
  bool sendIncrement = false;
  float absInMin = 0, absInMax = 20;
  float incScale = 0.05f;
  RangeMap map;
  KeyMode clickMode = MODE_PRESS_RELEASE;
  OSCMessage press, release;
  SequenceConfig clickSeq;
};

struct AngleOscConfig {
  String addr = "/avatar/parameters/Angle";
  bool use12bit = true;
  int deadband = 8;
  RangeMap map;
};

struct JoystickOscConfig {
  String xAddr = "/avatar/parameters/JoyX";
  String yAddr = "/avatar/parameters/JoyY";
  int deadband = 3;
  bool invertX = false;
  bool invertY = false;
  RangeMap map;
  KeyMode clickMode = MODE_PRESS_RELEASE;
  OSCMessage press, release;
  SequenceConfig clickSeq;
};

struct ChainDevice {
  bool active = false;
  uint16_t chainId = 0;
  chain_device_type_t type = CHAIN_UNKNOWN_TYPE_CODE;
  String uid = "", uidShort = "", displayName = "";
  KeyMode mode = MODE_PRESS_RELEASE;
  OSCMessage press, release;
  SequenceConfig seq;
  EncoderOscConfig enc;
  AngleOscConfig angle;
  JoystickOscConfig joy;
  uint8_t lastButtonStatus = 0;
  int16_t lastEncAbs = 0;
  bool encInited = false;
  int lastAngle = -99999;
  int16_t lastJoyX = 0, lastJoyY = 0;
  bool joyInited = false;
};

struct KnownDevice {
  bool used = false;
  String uid = "", displayName = "";
  chain_device_type_t type = CHAIN_UNKNOWN_TYPE_CODE;
};

Chain M5Chain;
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

bool isAPMode = false, chainBusReady = false;
int deviceCount = 0, knownCount = 0;
ChainDevice devices[MAX_DEVICES];
KnownDevice knownDevices[MAX_KNOWN];

String wifi_ssid = "", wifi_password = "";
String osc_host = "192.168.1.100";
int osc_port = 9000;
String ipStr = "", hostStr = "atoms3r-osc.local";
uint8_t displayRotation = 0;

String lastOscName = "", lastOscAddr = "", lastOscVal = "";
bool hasOscFeedback = false;

uint8_t operation_status = 0;
uint8_t color_red[3] = {255, 0, 0}, color_blue[3] = {0, 0, 255}, color_green[3] = {0, 200, 0};

unsigned long lastReenumMs = 0;
int lastKnownDeviceCount = -1;
String lastDeviceFingerprint = "";
unsigned long resetPressStart = 0;
bool resetInProgress = false;
int lastResetDrawPercent = -1;

uint32_t uidHash32(const String& uid) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < uid.length(); i++) {
    h ^= (uint8_t)uid.charAt(i);
    h *= 16777619u;
  }
  return h;
}
String deviceCfgKey(const String& uid) {
  char buf[12]; snprintf(buf, sizeof(buf), "d%08X", (unsigned)uidHash32(uid)); return String(buf);
}
String deviceNameKey(const String& uid) {
  char buf[12]; snprintf(buf, sizeof(buf), "n%08X", (unsigned)uidHash32(uid)); return String(buf);
}
String deviceCfgKeyLegacy(const String& uid) {
  String tail = uid.length() <= 12 ? uid : uid.substring(uid.length() - 12);
  return String("d_") + tail;
}
String deviceNameKeyLegacy(const String& uid) {
  String tail = uid.length() <= 12 ? uid : uid.substring(uid.length() - 12);
  return String("nm") + tail;
}
bool isPlaceholderUid(const String& uid) { return uid.startsWith("POS_"); }

float clampf(float v, float lo, float hi) {
  if (lo > hi) { float t = lo; lo = hi; hi = t; }
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
float mapClamped(float v, float inMin, float inMax, float outMin, float outMax) {
  if (fabsf(inMax - inMin) < 1e-6f) return outMin;
  float t = (v - inMin) / (inMax - inMin);
  return clampf(outMin + t * (outMax - outMin), outMin, outMax);
}

void applyDisplayRotation() {
  if (displayRotation > 3) displayRotation = 0;
  M5.Display.setRotation(displayRotation);
}

void showMessage(const char* a, const char* b = "") {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  int cx = M5.Display.width() / 2, cy = M5.Display.height() / 2;
  if (b[0] == '\0') M5.Display.drawString(a, cx, cy);
  else { M5.Display.drawString(a, cx, cy - 16); M5.Display.drawString(b, cx, cy + 16); }
}

void drawMainScreen() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextDatum(TL_DATUM);
  M5.Display.setTextSize(1);
  const int screenW = M5.Display.width(), screenH = M5.Display.height(), cols = 21;
  M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
  M5.Display.setCursor(2, 2); M5.Display.println("OSC Ready");
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setCursor(2, 16);
  String ipLine = "IP:" + ipStr; if ((int)ipLine.length() > cols) ipLine = ipLine.substring(0, cols);
  M5.Display.println(ipLine);
  M5.Display.setCursor(2, 28);
  String hostLine = "H:" + hostStr; if ((int)hostLine.length() > cols) hostLine = hostLine.substring(0, cols);
  M5.Display.println(hostLine);
  M5.Display.setCursor(2, 40); M5.Display.printf("Dev:%d", deviceCount);

  if (hasOscFeedback) {
    M5.Display.drawFastHLine(2, 52, screenW - 4, TFT_DARKGREY);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setCursor(2, 56);
    String n = lastOscName; if ((int)n.length() > cols) n = n.substring(0, cols);
    M5.Display.println(n.length() ? n : "OSC");
    const int addrStartY = 70, lineH = 12, valY = screenH - 14;
    const int maxAddrLines = max(1, (valY - 2 - addrStartY) / lineH);
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(2, addrStartY); M5.Display.print("A:");
    M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK);
    String addr = lastOscAddr;
    int firstCols = max(8, cols - 2), pos = 0, line = 0;
    while (pos < (int)addr.length() && line < maxAddrLines) {
      int take = (line == 0) ? firstCols : cols;
      int end = min(pos + take, (int)addr.length());
      String part = addr.substring(pos, end);
      if (line == 0) { M5.Display.setCursor(14, addrStartY); M5.Display.println(part); }
      else { M5.Display.setCursor(2, addrStartY + line * lineH); M5.Display.println(part); }
      pos = end; line++;
    }
    M5.Display.setTextColor(TFT_DARKGREY, TFT_BLACK);
    M5.Display.setCursor(2, valY); M5.Display.print("V:");
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    String v = lastOscVal; if ((int)v.length() > cols - 2) v = v.substring(0, cols - 2);
    M5.Display.print(v);
  } else {
    M5.Display.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Display.setCursor(2, 56);
    M5.Display.println(deviceCount > 0 ? "Waiting..." : "No Device");
  }
}

String deviceDisplayName(const ChainDevice& d) {
  String n = d.displayName; n.trim();
  if (!n.length()) { n = d.uid; if (n.length() > 6) n = n.substring(n.length() - 6); }
  return n;
}

void showOscFeedback(const String& name, const String& address, const String& value) {
  lastOscName = name; lastOscAddr = address; lastOscVal = value; hasOscFeedback = true;
  drawMainScreen();
}

void showResetProgress(unsigned long heldMs) {
  if (heldMs > RESET_HOLD_MS) heldMs = RESET_HOLD_MS;
  int percent = (int)((heldMs * 100) / RESET_HOLD_MS);
  if (percent == lastResetDrawPercent && heldMs < RESET_HOLD_MS) return;
  lastResetDrawPercent = percent;
  int cx = M5.Display.width() / 2, cy = M5.Display.height() / 2 - 6;
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.drawCircle(cx, cy, 36, TFT_DARKGREY);
  M5.Display.drawCircle(cx, cy, 28, TFT_DARKGREY);
  float deg = (heldMs * 360.0f) / (float)RESET_HOLD_MS; if (deg < 1) deg = 1;
  M5.Display.fillArc(cx, cy, 28, 36, -90, (int)(-90 + deg), TFT_RED);
  M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK); M5.Display.drawString("RESET", cx, cy - 8);
  char buf[16]; sprintf(buf, "%d%%", percent);
  M5.Display.setTextColor(TFT_YELLOW, TFT_BLACK); M5.Display.drawString(buf, cx, cy + 8);
}

String uidToString(const uint8_t* uid, uint8_t len) {
  String s = "";
  for (uint8_t i = 0; i < len; i++) { char b[3]; sprintf(b, "%02X", uid[i]); s += b; }
  return s;
}

const char* typeToName(chain_device_type_t t) {
  switch (t) {
    case CHAIN_KEY_TYPE_CODE: return "Key";
    case CHAIN_ENCODER_TYPE_CODE: return "Encoder";
    case CHAIN_ANGLE_TYPE_CODE: return "Angle";
    case CHAIN_JOYSTICK_TYPE_CODE: return "Joystick";
    default: return "Unknown";
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

void sendOSCValue(const String& address, ValueType type, float value, const String& strValue = "") {
  if (WiFi.status() != WL_CONNECTED || !address.length()) return;
  if (type == TYPE_FLOAT) OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), value);
  else if (type == TYPE_INT) OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), (int)lroundf(value));
  else OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), strValue.c_str());
}

void sendOSC(const OSCMessage& m) {
  if (m.valueType == TYPE_STRING) sendOSCValue(m.address, TYPE_STRING, 0, m.valueStr);
  else if (m.valueType == TYPE_INT) sendOSCValue(m.address, TYPE_INT, (float)m.valueStr.toInt());
  else sendOSCValue(m.address, TYPE_FLOAT, m.valueStr.toFloat());
}

void sendMappedOsc(const String& name, const String& addr, float mapped, ValueType type) {
  sendOSCValue(addr, type, mapped);
  String vs = (type == TYPE_INT) ? String((int)lroundf(mapped)) : String(mapped, 3);
  showOscFeedback(name, addr, vs);
}

void normalizeSequence(SequenceConfig& s) {
  if (s.step == 0) s.step = 1;
  if (s.start <= s.end && s.step < 0) s.step = -s.step;
  if (s.start > s.end && s.step > 0) s.step = -s.step;
  s.current = s.start;
}

void handleSequencePress(SequenceConfig& seq, const String& name) {
  float v = seq.current;
  sendOSCValue(seq.address, seq.valueType, v);
  float next = v + seq.step;
  if (seq.step >= 0) { if (next > seq.end + 1e-6f) next = seq.start; }
  else { if (next < seq.end - 1e-6f) next = seq.start; }
  seq.current = next;
  showOscFeedback(name, seq.address, seq.valueType == TYPE_INT ? String((int)v) : String(v, 2));
}

void clearKnownInMemory() {
  knownCount = 0;
  for (int i = 0; i < MAX_KNOWN; i++) {
    knownDevices[i].used = false;
    knownDevices[i].uid = "";
    knownDevices[i].displayName = "";
    knownDevices[i].type = CHAIN_UNKNOWN_TYPE_CODE;
  }
}

void saveKnownList() {
  String blob = "";
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used || !knownDevices[i].uid.length()) continue;
    if (isPlaceholderUid(knownDevices[i].uid)) continue;
    if (blob.length()) blob += ";";
    String n = knownDevices[i].displayName; n.replace(";", " "); n.replace("|", " ");
    blob += knownDevices[i].uid + "|" + n + "|" + String((int)knownDevices[i].type);
  }
  prefs.begin("known", false); prefs.putString("list", blob); prefs.end();
}

void loadKnownList() {
  clearKnownInMemory();
  prefs.begin("known", true); String blob = prefs.getString("list", ""); prefs.end();
  int start = 0;
  bool cleaned = false;
  while (start < (int)blob.length() && knownCount < MAX_KNOWN) {
    int sep = blob.indexOf(';', start); if (sep < 0) sep = blob.length();
    String item = blob.substring(start, sep);
    int bar1 = item.indexOf('|');
    String uid = (bar1 >= 0) ? item.substring(0, bar1) : item;
    String rest = (bar1 >= 0) ? item.substring(bar1 + 1) : "";
    int bar2 = rest.indexOf('|');
    String name = (bar2 >= 0) ? rest.substring(0, bar2) : rest;
    String typeStr = (bar2 >= 0) ? rest.substring(bar2 + 1) : "0";
    uid.trim(); name.trim(); typeStr.trim();
    if (uid.length() && !isPlaceholderUid(uid)) {
      knownDevices[knownCount].used = true;
      knownDevices[knownCount].uid = uid;
      knownDevices[knownCount].displayName = name;
      knownDevices[knownCount].type = (chain_device_type_t)typeStr.toInt();
      knownCount++;
    } else if (uid.length()) cleaned = true;
    start = sep + 1;
  }
  if (cleaned) saveKnownList();
}

int findKnownIndex(const String& uid) {
  for (int i = 0; i < MAX_KNOWN; i++) if (knownDevices[i].used && knownDevices[i].uid == uid) return i;
  return -1;
}
bool isUidConnected(const String& uid) {
  for (int i = 0; i < deviceCount; i++) if (devices[i].active && devices[i].uid == uid) return true;
  return false;
}

void registerKnownDevice(const String& uid, const String& displayName, chain_device_type_t type) {
  if (!uid.length() || isPlaceholderUid(uid)) return;
  int idx = findKnownIndex(uid);
  if (idx >= 0) {
    knownDevices[idx].displayName = displayName;
    if (type != CHAIN_UNKNOWN_TYPE_CODE) knownDevices[idx].type = type;
    saveKnownList();
    return;
  }
  for (int i = 0; i < MAX_KNOWN; i++) if (!knownDevices[i].used) {
    knownDevices[i].used = true;
    knownDevices[i].uid = uid;
    knownDevices[i].displayName = displayName;
    knownDevices[i].type = type;
    knownCount++;
    saveKnownList();
    return;
  }
}

void unregisterKnownDevice(const String& uid) {
  int idx = findKnownIndex(uid); if (idx < 0) return;
  knownDevices[idx].used = false;
  knownDevices[idx].uid = "";
  knownDevices[idx].displayName = "";
  knownDevices[idx].type = CHAIN_UNKNOWN_TYPE_CODE;
  knownCount = 0; for (int i = 0; i < MAX_KNOWN; i++) if (knownDevices[i].used) knownCount++;
  saveKnownList();
}

String loadDeviceNameOnly(const String& uid) {
  if (!uid.length() || isPlaceholderUid(uid)) return "";
  prefs.begin("devcfg", true);
  String name = prefs.getString(deviceNameKey(uid).c_str(), "");
  if (!name.length()) name = prefs.getString(deviceNameKeyLegacy(uid).c_str(), "");
  prefs.end();
  name.trim();
  return name;
}

void saveDeviceNameOnly(const String& uid, const String& name) {
  if (!uid.length() || isPlaceholderUid(uid)) return;
  prefs.begin("devcfg", false);
  prefs.putString(deviceNameKey(uid).c_str(), name);
  prefs.remove(deviceNameKeyLegacy(uid).c_str());
  prefs.end();
}

void applyKnownDisplayName(ChainDevice& d) {
  if (d.displayName.length() || !d.uid.length() || isPlaceholderUid(d.uid)) return;
  int ki = findKnownIndex(d.uid);
  if (ki >= 0 && knownDevices[ki].displayName.length()) d.displayName = knownDevices[ki].displayName;
}

void setDefaultDeviceMessages(ChainDevice& d) {
  d.displayName = "";
  d.mode = MODE_PRESS_RELEASE;
  d.press = {"/avatar/parameters/Key", "1.0", TYPE_FLOAT};
  d.release = {"/avatar/parameters/Key", "0.0", TYPE_FLOAT};
  d.seq = {"/avatar/parameters/KeySeq", TYPE_FLOAT, 0, 10, 1, 0};
  d.enc.rotAddr = "/avatar/parameters/Encoder";
  d.enc.sendIncrement = false;
  d.enc.absInMin = 0; d.enc.absInMax = 20; d.enc.incScale = 0.05f;
  d.enc.map = {0, 20, 0, 1, TYPE_FLOAT};
  d.enc.clickMode = MODE_PRESS_RELEASE;
  d.enc.press = {"/avatar/parameters/EncoderClick", "1.0", TYPE_FLOAT};
  d.enc.release = {"/avatar/parameters/EncoderClick", "0.0", TYPE_FLOAT};
  d.enc.clickSeq = {"/avatar/parameters/EncoderSeq", TYPE_FLOAT, 0, 10, 1, 0};
  d.angle.addr = "/avatar/parameters/Angle";
  d.angle.use12bit = true; d.angle.deadband = 8;
  d.angle.map = {0, 4095, 0, 1, TYPE_FLOAT};
  d.joy.xAddr = "/avatar/parameters/JoyX";
  d.joy.yAddr = "/avatar/parameters/JoyY";
  d.joy.deadband = 3;
  d.joy.invertX = false;
  d.joy.invertY = false;
  d.joy.map = {-127, 127, -1, 1, TYPE_FLOAT};
  d.joy.clickMode = MODE_PRESS_RELEASE;
  d.joy.press = {"/avatar/parameters/JoyClick", "1.0", TYPE_FLOAT};
  d.joy.release = {"/avatar/parameters/JoyClick", "0.0", TYPE_FLOAT};
  d.joy.clickSeq = {"/avatar/parameters/JoySeq", TYPE_FLOAT, 0, 10, 1, 0};
  d.encInited = false; d.joyInited = false; d.lastAngle = -99999; d.lastButtonStatus = 0;
}

static const char FS = '\x1f';
static void appendField(String& out, const String& v) {
  if (out.length()) out += FS;
  String t = v; t.replace(String(FS), " "); out += t;
}
static void appendField(String& out, int v) { appendField(out, String(v)); }
static void appendField(String& out, float v) { appendField(out, String(v, 6)); }
static String nextField(const String& src, int& idx) {
  if (idx >= (int)src.length()) return "";
  int end = src.indexOf(FS, idx);
  if (end < 0) end = src.length();
  String part = src.substring(idx, end);
  idx = end + 1;
  return part;
}

String serializeDeviceConfig(const ChainDevice& d) {
  String o;
  appendField(o, (int)d.type);
  appendField(o, d.displayName);
  appendField(o, (int)d.mode);
  appendField(o, d.press.address); appendField(o, d.press.valueStr); appendField(o, (int)d.press.valueType);
  appendField(o, d.release.address); appendField(o, d.release.valueStr); appendField(o, (int)d.release.valueType);
  appendField(o, d.seq.address); appendField(o, (int)d.seq.valueType);
  appendField(o, d.seq.start); appendField(o, d.seq.end); appendField(o, d.seq.step);
  appendField(o, d.enc.rotAddr); appendField(o, d.enc.sendIncrement ? 1 : 0);
  appendField(o, d.enc.absInMin); appendField(o, d.enc.absInMax); appendField(o, d.enc.incScale);
  appendField(o, d.enc.map.outMin); appendField(o, d.enc.map.outMax); appendField(o, (int)d.enc.map.outType);
  appendField(o, (int)d.enc.clickMode);
  appendField(o, d.enc.press.address); appendField(o, d.enc.press.valueStr); appendField(o, (int)d.enc.press.valueType);
  appendField(o, d.enc.release.address); appendField(o, d.enc.release.valueStr); appendField(o, (int)d.enc.release.valueType);
  appendField(o, d.enc.clickSeq.address); appendField(o, (int)d.enc.clickSeq.valueType);
  appendField(o, d.enc.clickSeq.start); appendField(o, d.enc.clickSeq.end); appendField(o, d.enc.clickSeq.step);
  appendField(o, d.angle.addr); appendField(o, d.angle.use12bit ? 1 : 0); appendField(o, d.angle.deadband);
  appendField(o, d.angle.map.outMin); appendField(o, d.angle.map.outMax); appendField(o, (int)d.angle.map.outType);
  appendField(o, d.joy.xAddr); appendField(o, d.joy.yAddr); appendField(o, d.joy.deadband);
  appendField(o, d.joy.invertX ? 1 : 0); appendField(o, d.joy.invertY ? 1 : 0);
  appendField(o, d.joy.map.outMin); appendField(o, d.joy.map.outMax); appendField(o, (int)d.joy.map.outType);
  appendField(o, (int)d.joy.clickMode);
  appendField(o, d.joy.press.address); appendField(o, d.joy.press.valueStr); appendField(o, (int)d.joy.press.valueType);
  appendField(o, d.joy.release.address); appendField(o, d.joy.release.valueStr); appendField(o, (int)d.joy.release.valueType);
  appendField(o, d.joy.clickSeq.address); appendField(o, (int)d.joy.clickSeq.valueType);
  appendField(o, d.joy.clickSeq.start); appendField(o, d.joy.clickSeq.end); appendField(o, d.joy.clickSeq.step);
  return o;
}

void applySerializedConfig(ChainDevice& d, const String& blob) {
  if (!blob.length()) return;
  int i = 0;
  auto s = [&]() { return nextField(blob, i); };
  auto asInt = [&]() { return s().toInt(); };
  auto asFloat = [&]() { return s().toFloat(); };

  String first = s();
  bool looksLikeType = first.length() > 0 && first.length() <= 4;
  for (size_t k = 0; looksLikeType && k < first.length(); k++) {
    if (first[k] < '0' || first[k] > '9') looksLikeType = false;
  }
  if (looksLikeType) {
    chain_device_type_t savedType = (chain_device_type_t)first.toInt();
    if (d.type == CHAIN_UNKNOWN_TYPE_CODE) d.type = savedType;
    d.displayName = s();
  } else {
    d.displayName = first;
  }

  d.mode = (KeyMode)asInt();
  d.press.address = s(); d.press.valueStr = s(); d.press.valueType = (ValueType)asInt();
  d.release.address = s(); d.release.valueStr = s(); d.release.valueType = (ValueType)asInt();
  d.seq.address = s(); d.seq.valueType = (ValueType)asInt();
  d.seq.start = asFloat(); d.seq.end = asFloat(); d.seq.step = asFloat();
  d.enc.rotAddr = s(); d.enc.sendIncrement = asInt() != 0;
  d.enc.absInMin = asFloat(); d.enc.absInMax = asFloat(); d.enc.incScale = asFloat();
  d.enc.map.outMin = asFloat(); d.enc.map.outMax = asFloat(); d.enc.map.outType = (ValueType)asInt();
  d.enc.clickMode = (KeyMode)asInt();
  d.enc.press.address = s(); d.enc.press.valueStr = s(); d.enc.press.valueType = (ValueType)asInt();
  d.enc.release.address = s(); d.enc.release.valueStr = s(); d.enc.release.valueType = (ValueType)asInt();
  d.enc.clickSeq.address = s(); d.enc.clickSeq.valueType = (ValueType)asInt();
  d.enc.clickSeq.start = asFloat(); d.enc.clickSeq.end = asFloat(); d.enc.clickSeq.step = asFloat();
  d.angle.addr = s(); d.angle.use12bit = asInt() != 0; d.angle.deadband = asInt();
  d.angle.map.outMin = asFloat(); d.angle.map.outMax = asFloat(); d.angle.map.outType = (ValueType)asInt();
  d.joy.xAddr = s(); d.joy.yAddr = s(); d.joy.deadband = asInt();

  // invertX/Y: 新形式。旧 blob では次が outMin(浮動) の場合がある
  // 残りフィールドを先読みせず、asInt で 0/1 なら invert、それ以外は旧形式として outMin に使う
  // 簡易: 常に2つ読む（旧データは要再Save）
  String invX = s();
  String invY = s();
  // invX が "0"/"1" なら新形式、それ以外（小数など）は旧形式で invX が outMin
  if ((invX == "0" || invX == "1") && (invY == "0" || invY == "1" || invY.length() <= 2)) {
    d.joy.invertX = invX.toInt() != 0;
    d.joy.invertY = invY.toInt() != 0;
    d.joy.map.outMin = asFloat();
  } else {
    d.joy.invertX = false;
    d.joy.invertY = false;
    d.joy.map.outMin = invX.toFloat();
    // invY が outMax の可能性
    d.joy.map.outMax = invY.toFloat();
    d.joy.map.outType = (ValueType)asInt();
    d.joy.clickMode = (KeyMode)asInt();
    d.joy.press.address = s(); d.joy.press.valueStr = s(); d.joy.press.valueType = (ValueType)asInt();
    d.joy.release.address = s(); d.joy.release.valueStr = s(); d.joy.release.valueType = (ValueType)asInt();
    d.joy.clickSeq.address = s(); d.joy.clickSeq.valueType = (ValueType)asInt();
    d.joy.clickSeq.start = asFloat(); d.joy.clickSeq.end = asFloat(); d.joy.clickSeq.step = asFloat();
    return;
  }
  d.joy.map.outMax = asFloat(); d.joy.map.outType = (ValueType)asInt();
  d.joy.clickMode = (KeyMode)asInt();
  d.joy.press.address = s(); d.joy.press.valueStr = s(); d.joy.press.valueType = (ValueType)asInt();
  d.joy.release.address = s(); d.joy.release.valueStr = s(); d.joy.release.valueType = (ValueType)asInt();
  d.joy.clickSeq.address = s(); d.joy.clickSeq.valueType = (ValueType)asInt();
  d.joy.clickSeq.start = asFloat(); d.joy.clickSeq.end = asFloat(); d.joy.clickSeq.step = asFloat();
}

void loadDeviceSettings(ChainDevice& d) {
  chain_device_type_t liveType = d.type;
  setDefaultDeviceMessages(d);
  d.type = liveType;
  if (!d.uid.length() || isPlaceholderUid(d.uid)) return;

  String dedicatedName = loadDeviceNameOnly(d.uid);
  String keyNew = deviceCfgKey(d.uid);
  String keyOld = deviceCfgKeyLegacy(d.uid);

  if (!prefs.begin("devcfg", true)) {
    if (dedicatedName.length()) d.displayName = dedicatedName;
    else applyKnownDisplayName(d);
    return;
  }

  auto readBlob = [&](const String& key) -> String {
    size_t len = prefs.getBytesLength(key.c_str());
    if (len > 0 && len < 4096) {
      char* buf = (char*)malloc(len + 1);
      if (buf) {
        size_t n = prefs.getBytes(key.c_str(), buf, len);
        buf[n] = '\0';
        String s(buf);
        free(buf);
        return s;
      }
    }
    return prefs.getString(key.c_str(), "");
  };

  String blob = readBlob(keyNew);
  if (!blob.length()) blob = readBlob(keyOld);
  prefs.end();

  if (blob.length()) {
    applySerializedConfig(d, blob);
    if (liveType != CHAIN_UNKNOWN_TYPE_CODE) d.type = liveType;
  }

  if (dedicatedName.length()) d.displayName = dedicatedName;
  else if (!d.displayName.length()) applyKnownDisplayName(d);

  normalizeSequence(d.seq);
  normalizeSequence(d.enc.clickSeq);
  normalizeSequence(d.joy.clickSeq);
  d.angle.map.inMin = 0;
  d.angle.map.inMax = d.angle.use12bit ? 4095.f : 255.f;
  d.enc.map.inMin = d.enc.absInMin;
  d.enc.map.inMax = d.enc.absInMax;
  d.joy.map.inMin = -127;
  d.joy.map.inMax = 127;
}

void saveDeviceSettings(const ChainDevice& d) {
  if (!d.uid.length() || isPlaceholderUid(d.uid)) return;
  saveDeviceNameOnly(d.uid, d.displayName);

  String key = deviceCfgKey(d.uid);
  String keyOld = deviceCfgKeyLegacy(d.uid);
  String blob = serializeDeviceConfig(d);

  auto tryWrite = [&](bool clearOldNs) -> bool {
    if (clearOldNs) {
      Preferences p2;
      if (p2.begin("keycfg", false)) { p2.clear(); p2.end(); }
    }
    if (!prefs.begin("devcfg", false)) return false;
    prefs.remove(keyOld.c_str());
    prefs.remove(key.c_str());
    size_t wrote = prefs.putBytes(key.c_str(), blob.c_str(), blob.length());
    if (wrote != blob.length()) {
      prefs.remove(key.c_str());
      wrote = prefs.putString(key.c_str(), blob);
    }
    size_t len2 = prefs.getBytesLength(key.c_str());
    bool ok = (len2 == blob.length()) ||
              (prefs.getString(key.c_str(), "").length() == (int)blob.length());
    prefs.end();
    return ok;
  };

  if (!tryWrite(false)) tryWrite(true);
  registerKnownDevice(d.uid, d.displayName, d.type);
}

void deleteDeviceSettingsByUid(const String& uid) {
  if (!uid.length()) return;
  if (!isPlaceholderUid(uid)) {
    prefs.begin("devcfg", false);
    prefs.remove(deviceCfgKey(uid).c_str());
    prefs.remove(deviceNameKey(uid).c_str());
    prefs.remove(deviceCfgKeyLegacy(uid).c_str());
    prefs.remove(deviceNameKeyLegacy(uid).c_str());
    prefs.end();
  }
  unregisterKnownDevice(uid);
  for (int i = 0; i < deviceCount; i++)
    if (devices[i].active && devices[i].uid == uid) {
      chain_device_type_t t = devices[i].type;
      setDefaultDeviceMessages(devices[i]);
      devices[i].type = t;
      break;
    }
}

void loadWifiAndOscCommon() {
  prefs.begin("wifi", true); wifi_ssid = prefs.getString("ssid", ""); wifi_password = prefs.getString("password", ""); prefs.end();
  prefs.begin("osc", true); osc_host = prefs.getString("host", "192.168.1.100"); osc_port = prefs.getInt("port", 9000); prefs.end();
  prefs.begin("ui", true); displayRotation = prefs.getUChar("rotation", 0); if (displayRotation > 3) displayRotation = 0; prefs.end();
  loadKnownList();
}
void saveDisplayRotation() {
  prefs.begin("ui", false); prefs.putUChar("rotation", displayRotation); prefs.end();
}

void resetAllSettings() {
  showResetProgress(RESET_HOLD_MS); delay(300); showMessage("RESET", "Clearing...");
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("osc", false); prefs.clear(); prefs.end();
  prefs.begin("devcfg", false); prefs.clear(); prefs.end();
  prefs.begin("keycfg", false); prefs.clear(); prefs.end();
  prefs.begin("ui", false); prefs.clear(); prefs.end();
  prefs.begin("known", false); prefs.clear(); prefs.end();
  delay(1000); showMessage("Cleared", "Reboot"); delay(800); ESP.restart();
}

void pollEncoder(ChainDevice& d) {
  int16_t absv = 0;
  if (M5Chain.getEncoderValue(d.chainId, &absv) == CHAIN_OK) {
    if (!d.encInited) { d.lastEncAbs = absv; d.encInited = true; }
    else if (absv != d.lastEncAbs) {
      int16_t delta = absv - d.lastEncAbs;
      d.lastEncAbs = absv;
      float mapped;
      if (d.enc.sendIncrement) {
        mapped = clampf((float)delta * d.enc.incScale, d.enc.map.outMin, d.enc.map.outMax);
      } else {
        float span = d.enc.absInMax - d.enc.absInMin;
        float x = (float)absv;
        if (fabsf(span) > 1e-6f) {
          x = fmodf(x - d.enc.absInMin, span);
          if (x < 0) x += span;
          x += d.enc.absInMin;
        }
        mapped = mapClamped(x, d.enc.absInMin, d.enc.absInMax, d.enc.map.outMin, d.enc.map.outMax);
      }
      sendMappedOsc(deviceDisplayName(d), d.enc.rotAddr, mapped, d.enc.map.outType);
    }
  }
  uint8_t st = 0;
  if (M5Chain.getEncoderButtonStatus(d.chainId, &st) == CHAIN_OK && st != d.lastButtonStatus) {
    if (d.enc.clickMode == MODE_SEQUENCE) {
      if (st == 1) {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_green, 3, &operation_status);
        handleSequencePress(d.enc.clickSeq, deviceDisplayName(d));
      } else M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
    } else if (st == 1) {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_red, 3, &operation_status);
      sendOSC(d.enc.press);
      showOscFeedback(deviceDisplayName(d), d.enc.press.address, d.enc.press.valueStr);
    } else {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      sendOSC(d.enc.release);
      showOscFeedback(deviceDisplayName(d), d.enc.release.address, d.enc.release.valueStr);
    }
    d.lastButtonStatus = st;
  }
}

void pollAngle(ChainDevice& d) {
  int val = -1;
  if (d.angle.use12bit) {
    uint16_t v12 = 0;
    if (M5Chain.getAngle12BitAdc(d.chainId, &v12) == CHAIN_OK) val = (int)v12;
  } else {
    uint8_t v8 = 0;
    if (M5Chain.getAngle8BitAdc(d.chainId, &v8) == CHAIN_OK) val = (int)v8;
  }
  if (val < 0) return;
  if (d.lastAngle < -99990) { d.lastAngle = val; return; }
  if (abs(val - d.lastAngle) >= max(1, d.angle.deadband)) {
    d.lastAngle = val;
    d.angle.map.inMin = 0;
    d.angle.map.inMax = d.angle.use12bit ? 4095.f : 255.f;
    float mapped = mapClamped((float)val, d.angle.map.inMin, d.angle.map.inMax, d.angle.map.outMin, d.angle.map.outMax);
    sendMappedOsc(deviceDisplayName(d), d.angle.addr, mapped, d.angle.map.outType);
  }
}

void pollJoystick(ChainDevice& d) {
  int8_t x = 0, y = 0;
  if (M5Chain.getJoystickMappedInt8Value(d.chainId, &x, &y) == CHAIN_OK) {
    if (!d.joyInited) { d.lastJoyX = x; d.lastJoyY = y; d.joyInited = true; }
    else {
      bool cx = abs((int)x - (int)d.lastJoyX) >= max(1, d.joy.deadband);
      bool cy = abs((int)y - (int)d.lastJoyY) >= max(1, d.joy.deadband);
      if (cx || cy) {
        d.lastJoyX = x; d.lastJoyY = y;
        if (cx) {
          float xin = d.joy.invertX ? -(float)x : (float)x;
          float mx = mapClamped(xin, -127, 127, d.joy.map.outMin, d.joy.map.outMax);
          sendMappedOsc(deviceDisplayName(d), d.joy.xAddr, mx, d.joy.map.outType);
        }
        if (cy) {
          float yin = d.joy.invertY ? -(float)y : (float)y;
          float my = mapClamped(yin, -127, 127, d.joy.map.outMin, d.joy.map.outMax);
          sendMappedOsc(deviceDisplayName(d), d.joy.yAddr, my, d.joy.map.outType);
        }
      }
    }
  }
  uint8_t st = 0;
  if (M5Chain.getJoystickButtonStatus(d.chainId, &st) == CHAIN_OK && st != d.lastButtonStatus) {
    if (d.joy.clickMode == MODE_SEQUENCE) {
      if (st == 1) {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_green, 3, &operation_status);
        handleSequencePress(d.joy.clickSeq, deviceDisplayName(d));
      } else M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
    } else if (st == 1) {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_red, 3, &operation_status);
      sendOSC(d.joy.press);
      showOscFeedback(deviceDisplayName(d), d.joy.press.address, d.joy.press.valueStr);
    } else {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      sendOSC(d.joy.release);
      showOscFeedback(deviceDisplayName(d), d.joy.release.address, d.joy.release.valueStr);
    }
    d.lastButtonStatus = st;
  }
}

void handleAPRoot(); void handleSaveWiFi(); void handleRoot(); void handleSave();
void handleDeleteWifi(); void handleDeleteDevice(); void handleSetRotation();

void startAPMode() {
  isAPMode = true;
  IPAddress apIP(192, 168, 4, 1);
  WiFi.mode(WIFI_AP); WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASSWORD); delay(500);
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", handleAPRoot); server.on("/save_wifi", handleSaveWiFi);
  server.onNotFound([]() { handleAPRoot(); }); server.begin();
  showMessage("AP Mode", "192.168.4.1");
}
void handleAPRoot() {
  server.send(200, "text/html",
    "<html><body><h2>WiFi Setup</h2><form method='POST' action='/save_wifi'>"
    "SSID<input name='ssid'><br>Password<input type='password' name='password'><br>"
    "<button type='submit'>Save & Restart</button></form></body></html>");
}
void handleSaveWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.arg("ssid").length()) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("password", server.arg("password"));
    prefs.end();
    server.send(200, "text/html", "<h2>Saved</h2>"); delay(1500); ESP.restart();
  }
  server.send(400, "text/plain", "Error");
}

String typeSelectHtml(const String& name, ValueType cur) {
  String s = "<select name='" + name + "'>";
  s += "<option value='0'" + String(cur == TYPE_FLOAT ? " selected" : "") + ">Float</option>";
  s += "<option value='1'" + String(cur == TYPE_INT ? " selected" : "") + ">Int</option>";
  s += "<option value='2'" + String(cur == TYPE_STRING ? " selected" : "") + ">String</option></select>";
  return s;
}
String clickModeHtml(const String& name, KeyMode cur, const String& prId, const String& sqId) {
  String s = "<div class='mode-box'><label>Click Mode</label>";
  s += "<select name='" + name + "' onchange=\"toggleClickMode('" + prId + "','" + sqId + "',this)\">";
  s += "<option value='0'" + String(cur == MODE_PRESS_RELEASE ? " selected" : "") + ">Press / Release</option>";
  s += "<option value='1'" + String(cur == MODE_SEQUENCE ? " selected" : "") + ">Sequence (press only)</option>";
  s += "</select></div>";
  return s;
}

void handleRoot() {
  String html = R"raw(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OSC Settings</title>
<style>
body{font-family:sans-serif;margin:16px;background:#f5f5f5}
.card{background:#fff;padding:16px;border-radius:10px;margin-bottom:16px;box-shadow:0 2px 5px rgba(0,0,0,.1)}
h1{font-size:1.4em}h2{margin-top:0;font-size:1.1em}
label{display:block;margin-top:10px;font-weight:bold;font-size:.9em}
input,select{width:100%;padding:8px;margin-top:4px;box-sizing:border-box}
button{width:100%;padding:12px;background:#28a745;color:#fff;border:none;border-radius:6px;font-size:16px;margin-top:8px}
.btn-danger{background:#dc3545}.btn-warning{background:#ff9800}.btn-rot{background:#6f42c1;flex:1;margin:0}
.btn-rot-cur{background:#9b59b6;font-weight:bold;box-shadow:inset 0 0 0 2px #fff}
.rot-row{display:flex;gap:8px;margin-top:10px}
.press{border-left:5px solid #dc3545;padding-left:10px;margin-top:12px}
.release{border-left:5px solid #007bff;padding-left:10px;margin-top:12px}
.seq{border-left:5px solid #20c997;padding-left:10px;margin-top:12px}
.enc{border-left:5px solid #fd7e14;padding-left:10px;margin-top:12px}
.ang{border-left:5px solid #6610f2;padding-left:10px;margin-top:12px}
.joy{border-left:5px solid #e83e8c;padding-left:10px;margin-top:12px}
.device{border-left:5px solid #6f42c1}
.uid{font-family:monospace;background:#eee;padding:6px 10px;border-radius:4px;word-break:break-all;font-size:.85em}
.note{color:#888;font-size:.9em}.meta{color:#666;font-size:.85em}
.badge{display:inline-block;padding:2px 8px;border-radius:10px;font-size:.75em;margin-right:4px}
.badge-on{background:#d4edda;color:#155724}.badge-off{background:#f8d7da;color:#721c24}
.badge-type{background:#e7e7ff;color:#333}
.mode-box{background:#f8f9fa;padding:10px;border-radius:6px;margin-top:10px}
.chk{display:flex;align-items:center;gap:8px;margin-top:8px}
.chk input{width:auto}
</style>
<script>
function toggleMode(pr,sq,sel){if(!pr||!sq)return;if(sel.value==='1'){pr.style.display='none';sq.style.display='block';}else{pr.style.display='block';sq.style.display='none';}}
function toggleClickMode(prId,sqId,sel){toggleMode(document.getElementById(prId),document.getElementById(sqId),sel);}
</script></head><body><h1>Chain OSC (VRChat)</h1>
)raw";

  html += "<div class='card'><h2>WiFi</h2><p class='meta'>IP: " + ipStr + "</p>";
  html += "<form action='/delete_wifi' method='POST' onsubmit=\"return confirm('Delete WiFi?');\">";
  html += "<button class='btn-danger' type='submit'>Delete WiFi Settings</button></form></div>";

  // 現在角度を明示 + ボタンに *
  html += "<div class='card'><h2>Display Rotation</h2>";
  html += "<p class='meta'>Current: <strong>" + String((int)displayRotation * 90) + "&deg;</strong> (index " + String(displayRotation) + ")</p>";
  html += "<div class='rot-row'>";
  for (int r = 0; r < 4; r++) {
    bool cur = ((int)displayRotation == r);
    html += "<form action='/set_rotation' method='POST' style='flex:1;margin:0'><input type='hidden' name='rotation' value='" + String(r) + "'>";
    html += "<button class='btn-rot" + String(cur ? " btn-rot-cur" : "") + "' type='submit'>";
    html += String(r * 90) + "&deg;";
    if (cur) html += " *";
    html += "</button></form>";
  }
  html += "</div></div>";

  html += "<form action='/save' method='POST'><div class='card'><h2>OSC Destination</h2>";
  html += "<label>Host IP</label><input name='host' value='" + htmlEscape(osc_host) + "'>";
  html += "<label>Port</label><input type='number' name='port' value='" + String(osc_port) + "'></div>";

  if (deviceCount == 0) html += "<div class='card'><p class='note'>No device connected.</p></div>";

  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    String idx = String(i);
    bool isSeq = devices[i].mode == MODE_SEQUENCE;
    bool encSeq = devices[i].enc.clickMode == MODE_SEQUENCE;
    bool joySeq = devices[i].joy.clickMode == MODE_SEQUENCE;
    bool ph = isPlaceholderUid(devices[i].uid);

    html += "<div class='card device'><h2>";
    html += "<span class='badge badge-type'>" + String(typeToName(devices[i].type)) + "</span>";
    html += " #" + String(devices[i].chainId);
    html += " <span class='badge badge-on'>Connected</span></h2>";
    html += "<div class='uid'>" + htmlEscape(devices[i].uid) + "</div>";
    if (ph) html += "<p class='note'>UID取得失敗（仮ID）。設定は保存されません。</p>";
    html += "<label>Device Name</label><input name='nm_" + idx + "' value='" + htmlEscape(devices[i].displayName) + "'>";
    html += "<input type='hidden' name='uid_" + idx + "' value='" + htmlEscape(devices[i].uid) + "'>";

    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      html += "<div class='mode-box'><label>Key Mode</label><select name='md_" + idx + "' onchange=\"toggleClickMode('kpr_" + idx + "','ksq_" + idx + "',this)\">";
      html += "<option value='0'" + String(!isSeq ? " selected" : "") + ">Press / Release</option>";
      html += "<option value='1'" + String(isSeq ? " selected" : "") + ">Sequence (press only)</option></select></div>";
      html += "<div id='kpr_" + idx + "' style='display:" + String(isSeq ? "none" : "block") + "'>";
      html += "<div class='press'><strong>Press</strong><label>Address</label><input name='pa_" + idx + "' value='" + htmlEscape(devices[i].press.address) + "'>";
      html += "<label>Value</label><input name='pv_" + idx + "' value='" + htmlEscape(devices[i].press.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("pt_" + idx, devices[i].press.valueType) + "</div>";
      html += "<div class='release'><strong>Release</strong><label>Address</label><input name='ra_" + idx + "' value='" + htmlEscape(devices[i].release.address) + "'>";
      html += "<label>Value</label><input name='rv_" + idx + "' value='" + htmlEscape(devices[i].release.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("rt_" + idx, devices[i].release.valueType) + "</div></div>";
      html += "<div id='ksq_" + idx + "' class='seq' style='display:" + String(isSeq ? "block" : "none") + "'><strong>Sequence</strong>";
      html += "<label>Address</label><input name='sa_" + idx + "' value='" + htmlEscape(devices[i].seq.address) + "'>";
      html += "<label>Start</label><input type='number' step='any' name='ss_" + idx + "' value='" + String(devices[i].seq.start) + "'>";
      html += "<label>End</label><input type='number' step='any' name='se_" + idx + "' value='" + String(devices[i].seq.end) + "'>";
      html += "<label>Step</label><input type='number' step='any' name='sp_" + idx + "' value='" + String(devices[i].seq.step) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("st_" + idx, devices[i].seq.valueType) + "</div>";
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      html += "<div class='enc'><strong>Encoder Rotation</strong>";
      html += "<label>Rotation Address</label><input name='er_" + idx + "' value='" + htmlEscape(devices[i].enc.rotAddr) + "'>";
      html += "<label>Mode</label><select name='ei_" + idx + "'><option value='0'" + String(!devices[i].enc.sendIncrement ? " selected" : "") + ">Absolute</option>";
      html += "<option value='1'" + String(devices[i].enc.sendIncrement ? " selected" : "") + ">Increment</option></select>";
      html += "<label>Abs In Min</label><input type='number' step='any' name='e0_" + idx + "' value='" + String(devices[i].enc.absInMin) + "'>";
      html += "<label>Abs In Max</label><input type='number' step='any' name='e1_" + idx + "' value='" + String(devices[i].enc.absInMax) + "'>";
      html += "<label>Inc Scale</label><input type='number' step='any' name='es_" + idx + "' value='" + String(devices[i].enc.incScale) + "'>";
      html += "<label>Out Min</label><input type='number' step='any' name='eo_" + idx + "' value='" + String(devices[i].enc.map.outMin) + "'>";
      html += "<label>Out Max</label><input type='number' step='any' name='eO_" + idx + "' value='" + String(devices[i].enc.map.outMax) + "'>";
      html += "<label>Out Type</label>" + typeSelectHtml("et_" + idx, devices[i].enc.map.outType) + "</div>";
      html += clickModeHtml("em_" + idx, devices[i].enc.clickMode, "epr_" + idx, "esq_" + idx);
      html += "<div id='epr_" + idx + "' style='display:" + String(encSeq ? "none" : "block") + "'>";
      html += "<div class='press'><strong>Click Press</strong><label>Address</label><input name='eb_" + idx + "' value='" + htmlEscape(devices[i].enc.press.address) + "'>";
      html += "<label>Value</label><input name='ec_" + idx + "' value='" + htmlEscape(devices[i].enc.press.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("ed_" + idx, devices[i].enc.press.valueType) + "</div>";
      html += "<div class='release'><strong>Click Release</strong><label>Address</label><input name='ef_" + idx + "' value='" + htmlEscape(devices[i].enc.release.address) + "'>";
      html += "<label>Value</label><input name='eg_" + idx + "' value='" + htmlEscape(devices[i].enc.release.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("eh_" + idx, devices[i].enc.release.valueType) + "</div></div>";
      html += "<div id='esq_" + idx + "' class='seq' style='display:" + String(encSeq ? "block" : "none") + "'><strong>Click Sequence</strong>";
      html += "<label>Address</label><input name='ek_" + idx + "' value='" + htmlEscape(devices[i].enc.clickSeq.address) + "'>";
      html += "<label>Start</label><input type='number' step='any' name='en_" + idx + "' value='" + String(devices[i].enc.clickSeq.start) + "'>";
      html += "<label>End</label><input type='number' step='any' name='e2_" + idx + "' value='" + String(devices[i].enc.clickSeq.end) + "'>";
      html += "<label>Step</label><input type='number' step='any' name='e3_" + idx + "' value='" + String(devices[i].enc.clickSeq.step) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("el_" + idx, devices[i].enc.clickSeq.valueType) + "</div>";
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      html += "<div class='ang'><strong>Angle</strong>";
      html += "<label>Address</label><input name='aa_" + idx + "' value='" + htmlEscape(devices[i].angle.addr) + "'>";
      html += "<label>Resolution</label><select name='a1_" + idx + "'><option value='1'" + String(devices[i].angle.use12bit ? " selected" : "") + ">12-bit</option>";
      html += "<option value='0'" + String(!devices[i].angle.use12bit ? " selected" : "") + ">8-bit</option></select>";
      html += "<label>Deadband</label><input type='number' name='ad_" + idx + "' value='" + String(devices[i].angle.deadband) + "'>";
      html += "<label>Out Min</label><input type='number' step='any' name='ao_" + idx + "' value='" + String(devices[i].angle.map.outMin) + "'>";
      html += "<label>Out Max</label><input type='number' step='any' name='aO_" + idx + "' value='" + String(devices[i].angle.map.outMax) + "'>";
      html += "<label>Out Type</label>" + typeSelectHtml("at_" + idx, devices[i].angle.map.outType) + "</div>";
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      html += "<div class='joy'><strong>Joystick XY</strong>";
      html += "<label>X Address</label><input name='jx_" + idx + "' value='" + htmlEscape(devices[i].joy.xAddr) + "'>";
      html += "<label>Y Address</label><input name='jy_" + idx + "' value='" + htmlEscape(devices[i].joy.yAddr) + "'>";
      html += "<label>Deadband</label><input type='number' name='jd_" + idx + "' value='" + String(devices[i].joy.deadband) + "'>";
      html += "<div class='chk'><input type='checkbox' name='jix_" + idx + "' value='1'" + String(devices[i].joy.invertX ? " checked" : "") + ">";
      html += "<span>Invert X (+/- 反転)</span></div>";
      html += "<div class='chk'><input type='checkbox' name='jiy_" + idx + "' value='1'" + String(devices[i].joy.invertY ? " checked" : "") + ">";
      html += "<span>Invert Y (+/- 反転)</span></div>";
      html += "<label>Out Min</label><input type='number' step='any' name='jo_" + idx + "' value='" + String(devices[i].joy.map.outMin) + "'>";
      html += "<label>Out Max</label><input type='number' step='any' name='jO_" + idx + "' value='" + String(devices[i].joy.map.outMax) + "'>";
      html += "<label>Out Type</label>" + typeSelectHtml("jt_" + idx, devices[i].joy.map.outType) + "</div>";
      html += clickModeHtml("jm_" + idx, devices[i].joy.clickMode, "jpr_" + idx, "jsq_" + idx);
      html += "<div id='jpr_" + idx + "' style='display:" + String(joySeq ? "none" : "block") + "'>";
      html += "<div class='press'><strong>Click Press</strong><label>Address</label><input name='jb_" + idx + "' value='" + htmlEscape(devices[i].joy.press.address) + "'>";
      html += "<label>Value</label><input name='jc_" + idx + "' value='" + htmlEscape(devices[i].joy.press.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("je_" + idx, devices[i].joy.press.valueType) + "</div>";
      html += "<div class='release'><strong>Click Release</strong><label>Address</label><input name='jf_" + idx + "' value='" + htmlEscape(devices[i].joy.release.address) + "'>";
      html += "<label>Value</label><input name='jg_" + idx + "' value='" + htmlEscape(devices[i].joy.release.valueStr) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("jh_" + idx, devices[i].joy.release.valueType) + "</div></div>";
      html += "<div id='jsq_" + idx + "' class='seq' style='display:" + String(joySeq ? "block" : "none") + "'><strong>Click Sequence</strong>";
      html += "<label>Address</label><input name='jk_" + idx + "' value='" + htmlEscape(devices[i].joy.clickSeq.address) + "'>";
      html += "<label>Start</label><input type='number' step='any' name='jn_" + idx + "' value='" + String(devices[i].joy.clickSeq.start) + "'>";
      html += "<label>End</label><input type='number' step='any' name='j2_" + idx + "' value='" + String(devices[i].joy.clickSeq.end) + "'>";
      html += "<label>Step</label><input type='number' step='any' name='j3_" + idx + "' value='" + String(devices[i].joy.clickSeq.step) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("jl_" + idx, devices[i].joy.clickSeq.valueType) + "</div>";
    } else {
      html += "<p class='note'>Type code: " + String((int)devices[i].type) + "</p>";
    }
    html += "</div>";
  }
  html += "<button type='submit'>Save All Settings</button></form>";

  html += "<div class='card'><h2>Saved Device Settings</h2>";
  html += "<p class='note'>Save したデバイスのみ表示。</p></div>";
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used || isPlaceholderUid(knownDevices[i].uid)) continue;
    bool con = isUidConnected(knownDevices[i].uid);
    String label = knownDevices[i].displayName.length() ? knownDevices[i].displayName
      : knownDevices[i].uid.substring(max(0, (int)knownDevices[i].uid.length() - 8));
    html += "<div class='card'><h2>";
    html += "<span class='badge badge-type'>" + String(typeToName(knownDevices[i].type)) + "</span> ";
    html += htmlEscape(label) + " ";
    html += con ? "<span class='badge badge-on'>Connected</span>" : "<span class='badge badge-off'>Off</span>";
    html += "</h2>";
    html += "<p class='meta'>Type: <strong>" + String(typeToName(knownDevices[i].type)) + "</strong></p>";
    html += "<div class='uid'>" + htmlEscape(knownDevices[i].uid) + "</div>";
    html += "<form action='/delete_device' method='POST' onsubmit=\"return confirm('Delete?');\">";
    html += "<input type='hidden' name='uid' value='" + htmlEscape(knownDevices[i].uid) + "'>";
    html += "<button class='btn-warning' type='submit'>Delete Settings</button></form></div>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (server.hasArg("host")) osc_host = server.arg("host");
  if (server.hasArg("port")) osc_port = server.arg("port").toInt();
  prefs.begin("osc", false); prefs.putString("host", osc_host); prefs.putInt("port", osc_port); prefs.end();

  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    String idx = String(i);
    if (!server.hasArg("uid_" + idx)) continue;
    if (server.hasArg("nm_" + idx)) {
      devices[i].displayName = server.arg("nm_" + idx);
      devices[i].displayName.trim();
    }

    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      if (server.hasArg("md_" + idx)) devices[i].mode = (KeyMode)server.arg("md_" + idx).toInt();
      if (server.hasArg("pa_" + idx)) devices[i].press.address = server.arg("pa_" + idx);
      if (server.hasArg("pv_" + idx)) devices[i].press.valueStr = server.arg("pv_" + idx);
      if (server.hasArg("pt_" + idx)) devices[i].press.valueType = (ValueType)server.arg("pt_" + idx).toInt();
      if (server.hasArg("ra_" + idx)) devices[i].release.address = server.arg("ra_" + idx);
      if (server.hasArg("rv_" + idx)) devices[i].release.valueStr = server.arg("rv_" + idx);
      if (server.hasArg("rt_" + idx)) devices[i].release.valueType = (ValueType)server.arg("rt_" + idx).toInt();
      if (server.hasArg("sa_" + idx)) devices[i].seq.address = server.arg("sa_" + idx);
      if (server.hasArg("ss_" + idx)) devices[i].seq.start = server.arg("ss_" + idx).toFloat();
      if (server.hasArg("se_" + idx)) devices[i].seq.end = server.arg("se_" + idx).toFloat();
      if (server.hasArg("sp_" + idx)) devices[i].seq.step = server.arg("sp_" + idx).toFloat();
      if (server.hasArg("st_" + idx)) devices[i].seq.valueType = (ValueType)server.arg("st_" + idx).toInt();
      normalizeSequence(devices[i].seq);
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      if (server.hasArg("er_" + idx)) devices[i].enc.rotAddr = server.arg("er_" + idx);
      if (server.hasArg("ei_" + idx)) devices[i].enc.sendIncrement = server.arg("ei_" + idx).toInt() != 0;
      if (server.hasArg("e0_" + idx)) devices[i].enc.absInMin = server.arg("e0_" + idx).toFloat();
      if (server.hasArg("e1_" + idx)) devices[i].enc.absInMax = server.arg("e1_" + idx).toFloat();
      if (server.hasArg("es_" + idx)) devices[i].enc.incScale = server.arg("es_" + idx).toFloat();
      if (server.hasArg("eo_" + idx)) devices[i].enc.map.outMin = server.arg("eo_" + idx).toFloat();
      if (server.hasArg("eO_" + idx)) devices[i].enc.map.outMax = server.arg("eO_" + idx).toFloat();
      if (server.hasArg("et_" + idx)) devices[i].enc.map.outType = (ValueType)server.arg("et_" + idx).toInt();
      if (server.hasArg("em_" + idx)) devices[i].enc.clickMode = (KeyMode)server.arg("em_" + idx).toInt();
      if (server.hasArg("eb_" + idx)) devices[i].enc.press.address = server.arg("eb_" + idx);
      if (server.hasArg("ec_" + idx)) devices[i].enc.press.valueStr = server.arg("ec_" + idx);
      if (server.hasArg("ed_" + idx)) devices[i].enc.press.valueType = (ValueType)server.arg("ed_" + idx).toInt();
      if (server.hasArg("ef_" + idx)) devices[i].enc.release.address = server.arg("ef_" + idx);
      if (server.hasArg("eg_" + idx)) devices[i].enc.release.valueStr = server.arg("eg_" + idx);
      if (server.hasArg("eh_" + idx)) devices[i].enc.release.valueType = (ValueType)server.arg("eh_" + idx).toInt();
      if (server.hasArg("ek_" + idx)) devices[i].enc.clickSeq.address = server.arg("ek_" + idx);
      if (server.hasArg("en_" + idx)) devices[i].enc.clickSeq.start = server.arg("en_" + idx).toFloat();
      if (server.hasArg("e2_" + idx)) devices[i].enc.clickSeq.end = server.arg("e2_" + idx).toFloat();
      if (server.hasArg("e3_" + idx)) devices[i].enc.clickSeq.step = server.arg("e3_" + idx).toFloat();
      if (server.hasArg("el_" + idx)) devices[i].enc.clickSeq.valueType = (ValueType)server.arg("el_" + idx).toInt();
      normalizeSequence(devices[i].enc.clickSeq);
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      if (server.hasArg("aa_" + idx)) devices[i].angle.addr = server.arg("aa_" + idx);
      if (server.hasArg("a1_" + idx)) devices[i].angle.use12bit = server.arg("a1_" + idx).toInt() != 0;
      if (server.hasArg("ad_" + idx)) devices[i].angle.deadband = server.arg("ad_" + idx).toInt();
      if (server.hasArg("ao_" + idx)) devices[i].angle.map.outMin = server.arg("ao_" + idx).toFloat();
      if (server.hasArg("aO_" + idx)) devices[i].angle.map.outMax = server.arg("aO_" + idx).toFloat();
      if (server.hasArg("at_" + idx)) devices[i].angle.map.outType = (ValueType)server.arg("at_" + idx).toInt();
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      if (server.hasArg("jx_" + idx)) devices[i].joy.xAddr = server.arg("jx_" + idx);
      if (server.hasArg("jy_" + idx)) devices[i].joy.yAddr = server.arg("jy_" + idx);
      if (server.hasArg("jd_" + idx)) devices[i].joy.deadband = server.arg("jd_" + idx).toInt();
      // checkbox は checked のときだけ送信される
      devices[i].joy.invertX = server.hasArg("jix_" + idx);
      devices[i].joy.invertY = server.hasArg("jiy_" + idx);
      if (server.hasArg("jo_" + idx)) devices[i].joy.map.outMin = server.arg("jo_" + idx).toFloat();
      if (server.hasArg("jO_" + idx)) devices[i].joy.map.outMax = server.arg("jO_" + idx).toFloat();
      if (server.hasArg("jt_" + idx)) devices[i].joy.map.outType = (ValueType)server.arg("jt_" + idx).toInt();
      if (server.hasArg("jm_" + idx)) devices[i].joy.clickMode = (KeyMode)server.arg("jm_" + idx).toInt();
      if (server.hasArg("jb_" + idx)) devices[i].joy.press.address = server.arg("jb_" + idx);
      if (server.hasArg("jc_" + idx)) devices[i].joy.press.valueStr = server.arg("jc_" + idx);
      if (server.hasArg("je_" + idx)) devices[i].joy.press.valueType = (ValueType)server.arg("je_" + idx).toInt();
      if (server.hasArg("jf_" + idx)) devices[i].joy.release.address = server.arg("jf_" + idx);
      if (server.hasArg("jg_" + idx)) devices[i].joy.release.valueStr = server.arg("jg_" + idx);
      if (server.hasArg("jh_" + idx)) devices[i].joy.release.valueType = (ValueType)server.arg("jh_" + idx).toInt();
      if (server.hasArg("jk_" + idx)) devices[i].joy.clickSeq.address = server.arg("jk_" + idx);
      if (server.hasArg("jn_" + idx)) devices[i].joy.clickSeq.start = server.arg("jn_" + idx).toFloat();
      if (server.hasArg("j2_" + idx)) devices[i].joy.clickSeq.end = server.arg("j2_" + idx).toFloat();
      if (server.hasArg("j3_" + idx)) devices[i].joy.clickSeq.step = server.arg("j3_" + idx).toFloat();
      if (server.hasArg("jl_" + idx)) devices[i].joy.clickSeq.valueType = (ValueType)server.arg("jl_" + idx).toInt();
      normalizeSequence(devices[i].joy.clickSeq);
    }

    saveDeviceSettings(devices[i]);
  }
  server.send(200, "text/html", "<h2>Saved!</h2><p><a href='/'>Back</a></p>");
}

void handleSetRotation() {
  if (server.hasArg("rotation")) {
    int r = server.arg("rotation").toInt();
    if (r >= 0 && r <= 3) {
      displayRotation = r; saveDisplayRotation(); applyDisplayRotation();
      if (!isAPMode && !resetInProgress) drawMainScreen();
    }
  }
  server.sendHeader("Location", "/", true); server.send(302, "text/plain", "");
}
void handleDeleteWifi() {
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  server.send(200, "text/html", "<h2>WiFi deleted</h2>"); delay(1200); ESP.restart();
}
void handleDeleteDevice() {
  if (!server.hasArg("uid")) { server.send(400, "text/plain", "uid"); return; }
  deleteDeviceSettingsByUid(server.arg("uid"));
  server.send(200, "text/html", "<h2>Deleted</h2><p><a href='/'>Back</a></p>");
}

String buildFingerprint(const ChainDevice* list, int count) {
  String fp = String(count) + "|";
  for (int i = 0; i < count; i++) fp += list[i].uid + ":" + String((int)list[i].type) + ";";
  return fp;
}

bool refreshChainDevices(bool force = false) {
  if (!chainBusReady) return false;

  if (!M5Chain.isDeviceConnected()) {
    if (deviceCount != 0 || lastKnownDeviceCount != 0) {
      deviceCount = 0;
      lastKnownDeviceCount = 0;
      lastDeviceFingerprint = "";
      for (int i = 0; i < MAX_DEVICES; i++) devices[i] = ChainDevice();
      if (!isAPMode && !resetInProgress) drawMainScreen();
      return true;
    }
    return false;
  }

  uint16_t count = 0;
  if (M5Chain.getDeviceNum(&count) != CHAIN_OK) return false;
  if (count > MAX_DEVICES) count = MAX_DEVICES;

  static ChainDevice tmp[MAX_DEVICES];
  int tmpCount = 0;
  for (int i = 0; i < MAX_DEVICES; i++) tmp[i] = ChainDevice();

  if (count > 0) {
    device_info_t infoList[MAX_DEVICES];
    device_list_t list = { count, infoList };
    if (!M5Chain.getDeviceList(&list)) return false;

    for (uint16_t i = 0; i < count; i++) {
      ChainDevice& d = tmp[tmpCount];
      d = ChainDevice();
      d.active = true;
      d.chainId = infoList[i].id;
      d.type = infoList[i].device_type;
      d.lastButtonStatus = 0;

      uint8_t uid[12] = {0};
      if (M5Chain.getUID(d.chainId, UID_TYPE_12_BYTE, uid, 12, &operation_status) == CHAIN_OK) {
        d.uid = uidToString(uid, 12);
        d.uidShort = d.uid.substring(d.uid.length() - 8);
      } else {
        d.uid = "POS_" + String(d.chainId);
        d.uidShort = d.uid;
      }

      if (!isPlaceholderUid(d.uid)) {
        loadDeviceSettings(d);
      } else {
        setDefaultDeviceMessages(d);
        d.type = infoList[i].device_type;
      }
      tmpCount++;
    }
  }

  String fp = buildFingerprint(tmp, tmpCount);
  bool changed = (fp != lastDeviceFingerprint) || force;

  if (changed) {
    deviceCount = tmpCount;
    lastKnownDeviceCount = tmpCount;
    lastDeviceFingerprint = fp;
    for (int i = 0; i < MAX_DEVICES; i++) devices[i] = ChainDevice();
    for (int i = 0; i < tmpCount; i++) devices[i] = tmp[i];

    for (int i = 0; i < deviceCount; i++) {
      if (!devices[i].active) continue;
      if (devices[i].type == CHAIN_KEY_TYPE_CODE ||
          devices[i].type == CHAIN_ENCODER_TYPE_CODE ||
          devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
        M5Chain.setRGBLight(devices[i].chainId, 80, &operation_status);
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
      }
      if (devices[i].type == CHAIN_KEY_TYPE_CODE)
        M5Chain.getKeyButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE)
        M5Chain.getEncoderButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE)
        M5Chain.getJoystickButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
    }
    if (!isAPMode && !resetInProgress) drawMainScreen();
  }
  return changed;
}

bool initChainBus() {
  M5Chain.begin(&Serial2, 115200, RXD_PIN, TXD_PIN); delay(300);
  chainBusReady = true; refreshChainDevices(true); return true;
}

void setup() {
  auto cfg = M5.config(); cfg.serial_baudrate = 115200; M5.begin(cfg); Serial.begin(115200); delay(200);
  loadWifiAndOscCommon(); applyDisplayRotation(); showMessage("START");
  if (wifi_ssid.length()) {
    showMessage("WiFi", "Connecting"); WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    unsigned long t = millis(); while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    isAPMode = false; ipStr = WiFi.localIP().toString();
    if (MDNS.begin("atoms3r-osc")) hostStr = "atoms3r-osc.local"; else hostStr = "(mDNS fail)";
    server.on("/", handleRoot); server.on("/save", handleSave);
    server.on("/delete_wifi", handleDeleteWifi); server.on("/delete_device", handleDeleteDevice);
    server.on("/set_rotation", handleSetRotation); server.begin();
    showMessage("WiFi OK", ipStr.c_str()); delay(800);
  } else startAPMode();
  initChainBus();
  if (!isAPMode) drawMainScreen();
  lastReenumMs = millis();
}

void loop() {
  M5.update(); server.handleClient();
  if (M5.BtnA.isPressed()) {
    if (resetPressStart == 0) { resetPressStart = millis(); resetInProgress = true; lastResetDrawPercent = -1; }
    unsigned long held = millis() - resetPressStart;
    showResetProgress(held);
    if (held >= RESET_HOLD_MS) resetAllSettings();
  } else if (resetInProgress) {
    resetInProgress = false; resetPressStart = 0; lastResetDrawPercent = -1;
    if (!isAPMode) drawMainScreen(); else showMessage("AP Mode", "192.168.4.1");
  }
  if (resetInProgress) { delay(20); return; }

  if (isAPMode) {
    dnsServer.processNextRequest();
    if (millis() - lastReenumMs >= REENUM_INTERVAL_MS) { lastReenumMs = millis(); refreshChainDevices(false); }
    delay(10); return;
  }
  if (millis() - lastReenumMs >= REENUM_INTERVAL_MS) { lastReenumMs = millis(); refreshChainDevices(false); }

  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      uint8_t st = 0;
      if (M5Chain.getKeyButtonStatus(devices[i].chainId, &st) != CHAIN_OK) continue;
      if (st == devices[i].lastButtonStatus) continue;
      if (devices[i].mode == MODE_SEQUENCE) {
        if (st == 1) {
          M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_green, 3, &operation_status);
          handleSequencePress(devices[i].seq, deviceDisplayName(devices[i]));
        } else M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
      } else if (st == 1) {
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_red, 3, &operation_status);
        sendOSC(devices[i].press);
        showOscFeedback(deviceDisplayName(devices[i]), devices[i].press.address, devices[i].press.valueStr);
      } else {
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
        sendOSC(devices[i].release);
        showOscFeedback(deviceDisplayName(devices[i]), devices[i].release.address, devices[i].release.valueStr);
      }
      devices[i].lastButtonStatus = st;
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) pollEncoder(devices[i]);
    else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) pollAngle(devices[i]);
    else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) pollJoystick(devices[i]);
  }
  delay(10);
}