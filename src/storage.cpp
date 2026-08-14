#include "storage.h"
#include "globals.h"
#include "display.h"

// ---------------------------------------------------------------------------
// NVS keys (15-char limit; use 32-bit hash of full UID)
// ---------------------------------------------------------------------------
uint32_t uidHash32(const String& uid) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < uid.length(); i++) {
    h ^= (uint8_t)uid.charAt(i);
    h *= 16777619u;
  }
  return h;
}

String deviceCfgKey(const String& uid) {
  char buf[12];
  snprintf(buf, sizeof(buf), "d%08X", (unsigned)uidHash32(uid));
  return String(buf);
}

String deviceNameKey(const String& uid) {
  char buf[12];
  snprintf(buf, sizeof(buf), "n%08X", (unsigned)uidHash32(uid));
  return String(buf);
}

String deviceCfgKeyLegacy(const String& uid) {
  String tail = uid.length() <= 12 ? uid : uid.substring(uid.length() - 12);
  return String("d_") + tail;
}

String deviceNameKeyLegacy(const String& uid) {
  String tail = uid.length() <= 12 ? uid : uid.substring(uid.length() - 12);
  return String("nm") + tail;
}

// ---------------------------------------------------------------------------
// Defaults
// ---------------------------------------------------------------------------
void setDefaultDeviceMessages(ChainDevice& d) {
  d.displayName = "";
  d.mode = MODE_PRESS_RELEASE;
  d.press   = {"/avatar/parameters/Key", "1.0", TYPE_FLOAT};
  d.release = {"/avatar/parameters/Key", "0.0", TYPE_FLOAT};
  d.seq     = {"/avatar/parameters/KeySeq", TYPE_FLOAT, 0, 10, 1, 0};

  d.enc.rotAddr       = "/avatar/parameters/Encoder";
  d.enc.sendIncrement = false;
  d.enc.absInMin      = 0;
  d.enc.absInMax      = 20;
  d.enc.incScale      = 0.05f;
  d.enc.map           = {0, 20, 0, 1, TYPE_FLOAT};
  d.enc.clickMode     = MODE_PRESS_RELEASE;
  d.enc.press         = {"/avatar/parameters/EncoderClick", "1.0", TYPE_FLOAT};
  d.enc.release       = {"/avatar/parameters/EncoderClick", "0.0", TYPE_FLOAT};
  d.enc.clickSeq      = {"/avatar/parameters/EncoderSeq", TYPE_FLOAT, 0, 10, 1, 0};

  d.angle.addr     = "/avatar/parameters/Angle";
  d.angle.use12bit = true;
  d.angle.deadband = 8;
  d.angle.map      = {0, 4095, 0, 1, TYPE_FLOAT};

  d.joy.xAddr     = "/avatar/parameters/JoyX";
  d.joy.yAddr     = "/avatar/parameters/JoyY";
  d.joy.deadband  = 3;
  d.joy.invertX   = false;
  d.joy.invertY   = false;
  d.joy.map       = {-127, 127, -1, 1, TYPE_FLOAT};
  d.joy.clickMode = MODE_PRESS_RELEASE;
  d.joy.press     = {"/avatar/parameters/JoyClick", "1.0", TYPE_FLOAT};
  d.joy.release   = {"/avatar/parameters/JoyClick", "0.0", TYPE_FLOAT};
  d.joy.clickSeq  = {"/avatar/parameters/JoySeq", TYPE_FLOAT, 0, 10, 1, 0};

  d.tof.addr     = "/avatar/parameters/ToF";
  d.tof.deadband = 5;
  d.tof.map      = {30, 2000, 0, 1, TYPE_FLOAT};

  d.encInited = false;
  d.joyInited = false;
  d.tofInited = false;
  d.tofConfigured = false;
  d.lastTofMm = -1;
  d.lastTofPollMs = 0;
  d.lastTofConfigMs = 0;
  d.tofReadFailures = 0;
  d.lastAngle = -99999;
  d.lastButtonStatus = 0;
}

// ---------------------------------------------------------------------------
// Serialization (field separator 0x1F)
// ---------------------------------------------------------------------------
static const char FS = '\x1f';

static void appendField(String& out, const String& v) {
  if (out.length()) out += FS;
  String t = v;
  t.replace(String(FS), " ");
  out += t;
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

  appendField(o, d.tof.addr);
  appendField(o, d.tof.deadband);
  appendField(o, d.tof.map.outMin); appendField(o, d.tof.map.outMax);
  appendField(o, (int)d.tof.map.outType);
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
  String invX = s();
  String invY = s();
  if ((invX == "0" || invX == "1") && (invY == "0" || invY == "1" || invY.length() <= 2)) {
    d.joy.invertX = invX.toInt() != 0;
    d.joy.invertY = invY.toInt() != 0;
    d.joy.map.outMin = asFloat();
    d.joy.map.outMax = asFloat();
    d.joy.map.outType = (ValueType)asInt();
    d.joy.clickMode = (KeyMode)asInt();
    d.joy.press.address = s(); d.joy.press.valueStr = s(); d.joy.press.valueType = (ValueType)asInt();
    d.joy.release.address = s(); d.joy.release.valueStr = s(); d.joy.release.valueType = (ValueType)asInt();
    d.joy.clickSeq.address = s(); d.joy.clickSeq.valueType = (ValueType)asInt();
    d.joy.clickSeq.start = asFloat(); d.joy.clickSeq.end = asFloat(); d.joy.clickSeq.step = asFloat();
  } else {
    // legacy blob without invert flags
    d.joy.invertX = false;
    d.joy.invertY = false;
    d.joy.map.outMin = invX.toFloat();
    d.joy.map.outMax = invY.toFloat();
    d.joy.map.outType = (ValueType)asInt();
    d.joy.clickMode = (KeyMode)asInt();
    d.joy.press.address = s(); d.joy.press.valueStr = s(); d.joy.press.valueType = (ValueType)asInt();
    d.joy.release.address = s(); d.joy.release.valueStr = s(); d.joy.release.valueType = (ValueType)asInt();
    d.joy.clickSeq.address = s(); d.joy.clickSeq.valueType = (ValueType)asInt();
    d.joy.clickSeq.start = asFloat(); d.joy.clickSeq.end = asFloat(); d.joy.clickSeq.step = asFloat();
  }

  // ToF (optional trailing fields — older blobs omit these)
  {
    String ta = s();
    if (ta.length()) {
      d.tof.addr = ta;
      d.tof.deadband = asInt();
      d.tof.map.outMin = asFloat();
      d.tof.map.outMax = asFloat();
      d.tof.map.outType = (ValueType)asInt();
    }
  }
}

// ---------------------------------------------------------------------------
// Name-only + known fallback
// ---------------------------------------------------------------------------
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
  if (ki >= 0 && knownDevices[ki].displayName.length())
    d.displayName = knownDevices[ki].displayName;
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
  d.tof.map.inMin = 30;
  d.tof.map.inMax = 2000;
}

void saveDeviceSettings(const ChainDevice& d) {
  if (!d.uid.length() || isPlaceholderUid(d.uid)) return;
  saveDeviceNameOnly(d.uid, d.displayName);

  String key    = deviceCfgKey(d.uid);
  String keyOld = deviceCfgKeyLegacy(d.uid);
  String blob   = serializeDeviceConfig(d);

  auto tryWrite = [&](bool clearOldNs) -> bool {
    if (clearOldNs) {
      Preferences p2;
      if (p2.begin("keycfg", false)) {
        p2.clear();
        p2.end();
      }
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
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].active && devices[i].uid == uid) {
      chain_device_type_t t = devices[i].type;
      setDefaultDeviceMessages(devices[i]);
      devices[i].type = t;
      break;
    }
  }
}

// ---------------------------------------------------------------------------
// Known device list
// ---------------------------------------------------------------------------
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
    String n = knownDevices[i].displayName;
    n.replace(";", " ");
    n.replace("|", " ");
    blob += knownDevices[i].uid + "|" + n + "|" + String((int)knownDevices[i].type);
  }
  prefs.begin("known", false);
  prefs.putString("list", blob);
  prefs.end();
}

void loadKnownList() {
  clearKnownInMemory();
  prefs.begin("known", true);
  String blob = prefs.getString("list", "");
  prefs.end();

  int start = 0;
  bool cleaned = false;
  while (start < (int)blob.length() && knownCount < MAX_KNOWN) {
    int sep = blob.indexOf(';', start);
    if (sep < 0) sep = blob.length();
    String item = blob.substring(start, sep);
    int bar1 = item.indexOf('|');
    String uid = (bar1 >= 0) ? item.substring(0, bar1) : item;
    String rest = (bar1 >= 0) ? item.substring(bar1 + 1) : "";
    int bar2 = rest.indexOf('|');
    String name = (bar2 >= 0) ? rest.substring(0, bar2) : rest;
    String typeStr = (bar2 >= 0) ? rest.substring(bar2 + 1) : "0";
    uid.trim();
    name.trim();
    typeStr.trim();
    if (uid.length() && !isPlaceholderUid(uid)) {
      knownDevices[knownCount].used = true;
      knownDevices[knownCount].uid = uid;
      knownDevices[knownCount].displayName = name;
      knownDevices[knownCount].type = (chain_device_type_t)typeStr.toInt();
      knownCount++;
    } else if (uid.length()) {
      cleaned = true;
    }
    start = sep + 1;
  }
  if (cleaned) saveKnownList();
}

int findKnownIndex(const String& uid) {
  for (int i = 0; i < MAX_KNOWN; i++)
    if (knownDevices[i].used && knownDevices[i].uid == uid) return i;
  return -1;
}

bool isUidConnected(const String& uid) {
  for (int i = 0; i < deviceCount; i++)
    if (devices[i].active && devices[i].uid == uid) return true;
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
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used) {
      knownDevices[i].used = true;
      knownDevices[i].uid = uid;
      knownDevices[i].displayName = displayName;
      knownDevices[i].type = type;
      knownCount++;
      saveKnownList();
      return;
    }
  }
}

void unregisterKnownDevice(const String& uid) {
  int idx = findKnownIndex(uid);
  if (idx < 0) return;
  knownDevices[idx].used = false;
  knownDevices[idx].uid = "";
  knownDevices[idx].displayName = "";
  knownDevices[idx].type = CHAIN_UNKNOWN_TYPE_CODE;
  knownCount = 0;
  for (int i = 0; i < MAX_KNOWN; i++)
    if (knownDevices[i].used) knownCount++;
  saveKnownList();
}

// ---------------------------------------------------------------------------
// Global prefs
// ---------------------------------------------------------------------------
void loadWifiAndOscCommon() {
  prefs.begin("wifi", true);
  wifi_ssid = prefs.getString("ssid", "");
  wifi_password = prefs.getString("password", "");
  prefs.end();

  prefs.begin("osc", true);
  osc_host = prefs.getString("host", "192.168.1.100");
  osc_port = prefs.getInt("port", 9000);
  prefs.end();

  prefs.begin("ui", true);
  displayRotation = prefs.getUChar("rotation", 0);
  if (displayRotation > 3) displayRotation = 0;
  prefs.end();

  loadKnownList();
}

void saveDisplayRotation() {
  prefs.begin("ui", false);
  prefs.putUChar("rotation", displayRotation);
  prefs.end();
}

void resetAllSettings() {
  showResetProgress(RESET_HOLD_MS);
  delay(300);
  showMessage("RESET", "Clearing...");
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("osc", false); prefs.clear(); prefs.end();
  prefs.begin("devcfg", false); prefs.clear(); prefs.end();
  prefs.begin("keycfg", false); prefs.clear(); prefs.end();
  prefs.begin("ui", false); prefs.clear(); prefs.end();
  prefs.begin("known", false); prefs.clear(); prefs.end();
  delay(1000);
  showMessage("Cleared", "Reboot");
  delay(800);
  ESP.restart();
}
