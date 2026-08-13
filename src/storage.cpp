#include "storage.h"
#include "globals.h"
#include "display.h"
#include <ctype.h>

#if M5CHAINOSC_STORAGE_DEBUG
#define STORAGE_LOG(...) do { Serial.print("[M5OSC][NVS] "); Serial.printf(__VA_ARGS__); Serial.println(); } while (0)
#else
#define STORAGE_LOG(...) do {} while (0)
#endif

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
static OSCMessage makeOSCMessage(const char* address, const char* value,
                                 ValueType type) {
  OSCMessage message;
  message.address = address;
  message.valueStr = value;
  message.valueType = type;
  return message;
}

static SequenceConfig makeSequenceConfig(const char* address, ValueType type,
                                         float start, float end, float step,
                                         float current) {
  SequenceConfig sequence;
  sequence.address = address;
  sequence.valueType = type;
  sequence.start = start;
  sequence.end = end;
  sequence.step = step;
  sequence.current = current;
  return sequence;
}

static RangeMap makeRangeMap(float inMin, float inMax, float outMin,
                             float outMax, ValueType type) {
  RangeMap map;
  map.inMin = inMin;
  map.inMax = inMax;
  map.outMin = outMin;
  map.outMax = outMax;
  map.outType = type;
  return map;
}

void setDefaultDeviceMessages(ChainDevice& d) {
  d.displayName = "";
  d.mode = MODE_PRESS_RELEASE;
  d.press   = makeOSCMessage("/avatar/parameters/Key", "1.0", TYPE_FLOAT);
  d.release = makeOSCMessage("/avatar/parameters/Key", "0.0", TYPE_FLOAT);
  d.pressMessageCount = 1;
  d.releaseMessageCount = 1;
  for (int i = 0; i < MAX_KEY_OSC_MESSAGES; i++) {
    d.pressMessages[i] = OSCMessage();
    d.releaseMessages[i] = OSCMessage();
  }
  d.pressMessages[0] = d.press;
  d.releaseMessages[0] = d.release;
  d.seq = makeSequenceConfig("/avatar/parameters/KeySeq", TYPE_FLOAT,
                             0, 10, 1, 0);

  d.enc.rotAddr       = "/avatar/parameters/Encoder";
  d.enc.sendIncrement = false;
  d.enc.absInMin      = 0;
  d.enc.absInMax      = 20;
  d.enc.incScale      = 0.05f;
  d.enc.map           = makeRangeMap(0, 20, 0, 1, TYPE_FLOAT);
  d.enc.clickMode     = MODE_PRESS_RELEASE;
  d.enc.press = makeOSCMessage("/avatar/parameters/EncoderClick", "1.0",
                               TYPE_FLOAT);
  d.enc.release = makeOSCMessage("/avatar/parameters/EncoderClick", "0.0",
                                 TYPE_FLOAT);
  d.enc.pressMessageCount = d.enc.releaseMessageCount = 1;
  d.enc.pressMessages[0] = d.enc.press; d.enc.releaseMessages[0] = d.enc.release;
  d.enc.clickSeq = makeSequenceConfig("/avatar/parameters/EncoderSeq",
                                      TYPE_FLOAT, 0, 10, 1, 0);

  d.angle.addr     = "/avatar/parameters/Angle";
  d.angle.use12bit = true;
  d.angle.deadband = 8;
  d.angle.map      = makeRangeMap(0, 4095, 0, 1, TYPE_FLOAT);

  d.joy.xAddr     = "/avatar/parameters/JoyX";
  d.joy.yAddr     = "/avatar/parameters/JoyY";
  d.joy.deadband  = 3;
  d.joy.invertX   = false;
  d.joy.invertY   = false;
  d.joy.map       = makeRangeMap(-127, 127, -1, 1, TYPE_FLOAT);
  d.joy.clickMode = MODE_PRESS_RELEASE;
  d.joy.press = makeOSCMessage("/avatar/parameters/JoyClick", "1.0",
                               TYPE_FLOAT);
  d.joy.release = makeOSCMessage("/avatar/parameters/JoyClick", "0.0",
                                 TYPE_FLOAT);
  d.joy.pressMessageCount = d.joy.releaseMessageCount = 1;
  d.joy.pressMessages[0] = d.joy.press; d.joy.releaseMessages[0] = d.joy.release;
  d.joy.clickSeq = makeSequenceConfig("/avatar/parameters/JoySeq", TYPE_FLOAT,
                                      0, 10, 1, 0);

  d.tof.addr     = "/avatar/parameters/ToF";
  d.tof.deadband = 5;
  d.tof.map      = makeRangeMap(30, 2000, 0, 1, TYPE_FLOAT);

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

static bool plausibleOscAddress(const String& address) {
  if (!address.startsWith("/") || address.length() > MAX_OSC_ADDRESS_BYTES) return false;
  for (size_t i = 0; i < address.length(); i++) {
    char c = address[i];
    if (isspace((unsigned char)c) || c == '#' || c == '*' || c == ',' || c == '?' ||
        c == '[' || c == ']' || c == '{' || c == '}') return false;
  }
  return true;
}

static void sanitizeStoredMessages(OSCMessage* messages, uint8_t& count) {
  uint8_t write = 0;
  for (uint8_t read = 0; read < count && read < MAX_KEY_OSC_MESSAGES; read++) {
    OSCMessage message = messages[read];
    message.address.trim();
    // Recover the recognizable address/value reversal seen in damaged settings.
    if (!plausibleOscAddress(message.address) && plausibleOscAddress(message.valueStr)) {
      String oldAddress = message.address;
      message.address = message.valueStr;
      message.valueStr = oldAddress;
    }
    if (!plausibleOscAddress(message.address)) continue;
    messages[write++] = message;
  }
  count = write;
}

static String serializeKeyMessages(const ChainDevice& d) {
  const OSCMessage* press = d.pressMessages; const OSCMessage* release = d.releaseMessages;
  uint8_t pressCount = d.pressMessageCount, releaseCount = d.releaseMessageCount;
  if (d.type == CHAIN_ENCODER_TYPE_CODE) { press=d.enc.pressMessages; release=d.enc.releaseMessages; pressCount=d.enc.pressMessageCount; releaseCount=d.enc.releaseMessageCount; }
  else if (d.type == CHAIN_JOYSTICK_TYPE_CODE) { press=d.joy.pressMessages; release=d.joy.releaseMessages; pressCount=d.joy.pressMessageCount; releaseCount=d.joy.releaseMessageCount; }
  String out;
  appendField(out, String("KM3"));
  appendField(out, (int)pressCount);
  for (uint8_t i = 0; i < pressCount && i < MAX_KEY_OSC_MESSAGES; i++) {
    appendField(out, press[i].address); appendField(out, press[i].valueStr); appendField(out, (int)press[i].valueType);
  }
  appendField(out, (int)releaseCount);
  for (uint8_t i = 0; i < releaseCount && i < MAX_KEY_OSC_MESSAGES; i++) {
    appendField(out, release[i].address); appendField(out, release[i].valueStr); appendField(out, (int)release[i].valueType);
  }
  return out;
}

static bool applyKeyMessages(ChainDevice& d, const String& blob) {
  if (!blob.length()) {
    STORAGE_LOG("KM2 apply skipped: uid=%s empty", d.uid.c_str());
    return false;
  }
  int pos = 0;
  auto field = [&]() { return nextField(blob, pos); };
  String marker = field();
  if (marker != "KM2" && marker != "KM3") {
    STORAGE_LOG("KM2 invalid marker: uid=%s len=%u marker=%s", d.uid.c_str(), (unsigned)blob.length(), marker.c_str());
    return false;
  }
  int parsedPressCount = field().toInt();
  int pc = constrain(parsedPressCount, 0, MAX_KEY_OSC_MESSAGES);
  OSCMessage* press = d.pressMessages; OSCMessage* release = d.releaseMessages;
  uint8_t* pressCount = &d.pressMessageCount; uint8_t* releaseCount = &d.releaseMessageCount;
  OSCMessage* legacyPress=&d.press; OSCMessage* legacyRelease=&d.release;
  if (d.type == CHAIN_ENCODER_TYPE_CODE) { press=d.enc.pressMessages; release=d.enc.releaseMessages; pressCount=&d.enc.pressMessageCount; releaseCount=&d.enc.releaseMessageCount; legacyPress=&d.enc.press; legacyRelease=&d.enc.release; }
  else if (d.type == CHAIN_JOYSTICK_TYPE_CODE) { press=d.joy.pressMessages; release=d.joy.releaseMessages; pressCount=&d.joy.pressMessageCount; releaseCount=&d.joy.releaseMessageCount; legacyPress=&d.joy.press; legacyRelease=&d.joy.release; }
  *pressCount = (uint8_t)pc;
  for (int n = 0; n < pc; n++) {
    press[n].address = field(); press[n].valueStr = field();
    int parsedType = field().toInt();
    press[n].valueType = (ValueType)constrain(parsedType, (int)TYPE_FLOAT, (int)TYPE_STRING);
  }
  int parsedReleaseCount = field().toInt();
  int rc = constrain(parsedReleaseCount, 0, MAX_KEY_OSC_MESSAGES - pc);
  *releaseCount = (uint8_t)rc;
  for (int n = 0; n < rc; n++) {
    release[n].address = field(); release[n].valueStr = field();
    int parsedType = field().toInt();
    release[n].valueType = (ValueType)constrain(parsedType, (int)TYPE_FLOAT, (int)TYPE_STRING);
  }
  sanitizeStoredMessages(press, *pressCount); sanitizeStoredMessages(release, *releaseCount);
  if (*pressCount > 0) *legacyPress = press[0]; if (*releaseCount > 0) *legacyRelease = release[0];
  STORAGE_LOG("KM applied: uid=%s press=%u release=%u len=%u", d.uid.c_str(), *pressCount, *releaseCount, (unsigned)blob.length());
  return true;
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

size_t deviceConfigStorageBytes(const ChainDevice& d) {
  size_t bytes = serializeDeviceConfig(d).length();
  if (d.type == CHAIN_KEY_TYPE_CODE || d.type == CHAIN_ENCODER_TYPE_CODE || d.type == CHAIN_JOYSTICK_TYPE_CODE) bytes += serializeKeyMessages(d).length();
  return bytes;
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

  // Multi-message extension. Without it, migrate the legacy single messages.
  d.pressMessageCount = 1;
  d.releaseMessageCount = 1;
  d.pressMessages[0] = d.press;
  d.releaseMessages[0] = d.release;
  String marker = s();
  if (marker == "KM1") {
    int parsedPressCount = asInt();
    int pc = constrain(parsedPressCount, 0, MAX_KEY_OSC_MESSAGES);
    d.pressMessageCount = (uint8_t)pc;
    for (int n = 0; n < pc; n++) {
      d.pressMessages[n].address = s();
      d.pressMessages[n].valueStr = s();
      int parsedType = asInt();
      d.pressMessages[n].valueType = (ValueType)constrain(parsedType, (int)TYPE_FLOAT, (int)TYPE_STRING);
    }
    int remaining = MAX_KEY_OSC_MESSAGES - pc;
    int parsedReleaseCount = asInt();
    int rc = constrain(parsedReleaseCount, 0, remaining);
    d.releaseMessageCount = (uint8_t)rc;
    for (int n = 0; n < rc; n++) {
      d.releaseMessages[n].address = s();
      d.releaseMessages[n].valueStr = s();
      int parsedType = asInt();
      d.releaseMessages[n].valueType = (ValueType)constrain(parsedType, (int)TYPE_FLOAT, (int)TYPE_STRING);
    }
    if (pc > 0) d.press = d.pressMessages[0];
    if (rc > 0) d.release = d.releaseMessages[0];
  }
  sanitizeStoredMessages(d.pressMessages, d.pressMessageCount);
  sanitizeStoredMessages(d.releaseMessages, d.releaseMessageCount);
  if (d.pressMessageCount > 0) d.press = d.pressMessages[0];
  if (d.releaseMessageCount > 0) d.release = d.releaseMessages[0];
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
  STORAGE_LOG("LOAD begin: uid=%s type=%d key=%s legacy=%s", d.uid.c_str(), (int)liveType, keyNew.c_str(), keyOld.c_str());

  if (!prefs.begin("devcfg", true)) {
    STORAGE_LOG("LOAD devcfg begin failed: uid=%s", d.uid.c_str());
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
  bool usedLegacy = false;
  if (!blob.length()) {
    blob = readBlob(keyOld);
    usedLegacy = blob.length() > 0;
  }
  prefs.end();
  STORAGE_LOG("LOAD base: uid=%s len=%u source=%s", d.uid.c_str(), (unsigned)blob.length(), usedLegacy ? "legacy" : (blob.length() ? "hashed" : "none"));

  if (blob.length()) {
    applySerializedConfig(d, blob);
    if (liveType != CHAIN_UNKNOWN_TYPE_CODE) d.type = liveType;
  }

  // KM2 is stored separately so loading does not depend on the layout of the
  // legacy all-device blob. The embedded KM1 extension remains a fallback.
  if (liveType == CHAIN_KEY_TYPE_CODE || liveType == CHAIN_ENCODER_TYPE_CODE || liveType == CHAIN_JOYSTICK_TYPE_CODE) {
    if (prefs.begin("keymulti", true)) {
      String multi = prefs.getString(keyNew.c_str(), "");
      prefs.end();
      STORAGE_LOG("LOAD keymulti: uid=%s key=%s len=%u", d.uid.c_str(), keyNew.c_str(), (unsigned)multi.length());
      applyKeyMessages(d, multi);
    } else {
      STORAGE_LOG("LOAD keymulti begin failed: uid=%s", d.uid.c_str());
    }
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

bool saveDeviceSettings(const ChainDevice& d) {
  if (!d.uid.length() || isPlaceholderUid(d.uid)) {
    STORAGE_LOG("SAVE rejected placeholder: uid=%s", d.uid.c_str());
    return false;
  }
  saveDeviceNameOnly(d.uid, d.displayName);

  String key    = deviceCfgKey(d.uid);
  String keyOld = deviceCfgKeyLegacy(d.uid);
  String blob   = serializeDeviceConfig(d);
  STORAGE_LOG("SAVE begin: uid=%s type=%d key=%s baseLen=%u press=%u release=%u", d.uid.c_str(), (int)d.type, key.c_str(), (unsigned)blob.length(), d.pressMessageCount, d.releaseMessageCount);

  auto tryWrite = [&](bool clearOldNs) -> bool {
    if (clearOldNs) {
      Preferences p2;
      if (p2.begin("keycfg", false)) {
        p2.clear();
        p2.end();
      }
    }
    if (!prefs.begin("devcfg", false)) {
      STORAGE_LOG("SAVE devcfg begin failed: uid=%s", d.uid.c_str());
      return false;
    }
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
    STORAGE_LOG("SAVE base result: uid=%s wrote=%u storedLen=%u expected=%u ok=%d", d.uid.c_str(), (unsigned)wrote, (unsigned)len2, (unsigned)blob.length(), ok ? 1 : 0);
    return ok;
  };

  bool ok = tryWrite(false) || tryWrite(true);
  if (ok && (d.type == CHAIN_KEY_TYPE_CODE || d.type == CHAIN_ENCODER_TYPE_CODE || d.type == CHAIN_JOYSTICK_TYPE_CODE)) {
    String multi = serializeKeyMessages(d);
    if (!prefs.begin("keymulti", false)) {
      STORAGE_LOG("SAVE keymulti begin failed: uid=%s", d.uid.c_str());
      return false;
    }
    size_t wrote = prefs.putString(key.c_str(), multi);
    String verify = prefs.getString(key.c_str(), "");
    prefs.end();
    ok = wrote > 0 && verify == multi;
    STORAGE_LOG("SAVE keymulti result: uid=%s key=%s wrote=%u readLen=%u expected=%u match=%d", d.uid.c_str(), key.c_str(), (unsigned)wrote, (unsigned)verify.length(), (unsigned)multi.length(), ok ? 1 : 0);
  }
  if (ok) registerKnownDevice(d.uid, d.displayName, d.type);
  STORAGE_LOG("SAVE end: uid=%s ok=%d", d.uid.c_str(), ok ? 1 : 0);
  return ok;
}

void deleteDeviceSettingsByUid(const String& uid) {
  if (!uid.length()) return;
  if (!isPlaceholderUid(uid)) {
    const String cfgKey = deviceCfgKey(uid);
    const String nameKey = deviceNameKey(uid);
    const String legacyCfgKey = deviceCfgKeyLegacy(uid);
    const String legacyNameKey = deviceNameKeyLegacy(uid);
    bool cfgKeyShared = false;
    bool nameKeyShared = false;
    bool legacyCfgKeyShared = false;
    bool legacyNameKeyShared = false;
    auto checkOtherUid = [&](const String& otherUid) {
      if (!otherUid.length() || otherUid == uid || isPlaceholderUid(otherUid)) return;
      if (deviceCfgKey(otherUid) == cfgKey) cfgKeyShared = true;
      if (deviceNameKey(otherUid) == nameKey) nameKeyShared = true;
      if (deviceCfgKeyLegacy(otherUid) == legacyCfgKey) legacyCfgKeyShared = true;
      if (deviceNameKeyLegacy(otherUid) == legacyNameKey) legacyNameKeyShared = true;
    };
    for (int i = 0; i < MAX_KNOWN; i++)
      if (knownDevices[i].used) checkOtherUid(knownDevices[i].uid);
    for (int i = 0; i < deviceCount; i++)
      if (devices[i].active) checkOtherUid(devices[i].uid);

    prefs.begin("devcfg", false);
    if (!cfgKeyShared) prefs.remove(cfgKey.c_str());
    if (!nameKeyShared) prefs.remove(nameKey.c_str());
    // The legacy keys use only the UID suffix and are not guaranteed unique.
    if (!legacyCfgKeyShared) prefs.remove(legacyCfgKey.c_str());
    if (!legacyNameKeyShared) prefs.remove(legacyNameKey.c_str());
    prefs.end();
    if (prefs.begin("keymulti", false)) {
      if (!cfgKeyShared) prefs.remove(cfgKey.c_str());
      prefs.end();
    }
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
  prefs.begin("keymulti", false); prefs.clear(); prefs.end();
  prefs.begin("ui", false); prefs.clear(); prefs.end();
  prefs.begin("known", false); prefs.clear(); prefs.end();
  delay(1000);
  showMessage("Cleared", "Reboot");
  delay(800);
  ESP.restart();
}
