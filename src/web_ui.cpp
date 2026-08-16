#include "web_ui.h"
#include "globals.h"
#include "storage.h"
#include "display.h"
#include "chain_devices.h"
#include "memory_debug.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdlib.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>

#if M5CHAINOSC_WEB_PERF_DEBUG
static uint32_t webPerfRequestSequence = 0;

static void logWebPerf(uint32_t requestId, uint32_t requestStart,
                       const char* phase, size_t bytes = 0,
                       uint32_t operationMs = 0) {
  const uint32_t internalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  size_t freeHeap = heap_caps_get_free_size(internalCaps);
  size_t minFreeHeap = ESP.getMinFreeHeap();
  size_t largestBlock = heap_caps_get_largest_free_block(internalCaps);
  size_t freePsram = ESP.getFreePsram();
  unsigned fragmentation = freeHeap > 0
      ? 100U - (unsigned)((largestBlock * 100U) / freeHeap)
      : 100U;
  IPAddress remote = server.client().remoteIP();
  Serial.printf(
      "[M5OSC][WEBPERF] req=%lu phase=%s elapsed=%lu op=%lu bytes=%u "
      "free=%u min=%u largest=%u psram=%u frag=%u%% connected=%d rssi=%d remote=%s\n",
      (unsigned long)requestId, phase,
      (unsigned long)(millis() - requestStart), (unsigned long)operationMs,
      (unsigned)bytes, (unsigned)freeHeap, (unsigned)minFreeHeap,
      (unsigned)largestBlock, (unsigned)freePsram, fragmentation,
      server.client().connected() ? 1 : 0,
      WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0,
      remote.toString().c_str());
}
#define WEB_PERF_LOG(id, start, phase, bytes, opMs) \
  logWebPerf((id), (start), (phase), (bytes), (opMs))
#else
#define WEB_PERF_LOG(id, start, phase, bytes, opMs) do {} while (0)
#endif

static bool isJapaneseUi() {
  return uiLanguage == UI_LANG_JAPANESE;
}

static const char* tr(const char* english, const char* japanese) {
  return isJapaneseUi() ? japanese : english;
}

static void sendUiResult(int status, const String& title, const String& message,
                         bool showBack = true) {
  if (server.hasArg("ajax")) {
    server.send(status, "text/plain; charset=utf-8", message);
    return;
  }
  String html = "<!doctype html><html lang='" + String(isJapaneseUi() ? "ja" : "en") +
                "'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'></head><body><h2>" +
                htmlEscape(title) + "</h2><p>" + htmlEscape(message) + "</p>";
  if (showBack) html += "<p><a href='/'>" + String(tr("Back", "戻る")) + "</a></p>";
  html += "</body></html>";
  server.send(status, "text/html; charset=utf-8", html);
}

static void applyBrowserLanguageOnFirstVisit() {
  if (uiLanguageConfigured) return;
  String accepted = server.header("Accept-Language");
  accepted.toLowerCase();
  if (!accepted.length()) return;
  uiLanguage = accepted.startsWith("ja") ? UI_LANG_JAPANESE : UI_LANG_ENGLISH;
  saveUiLanguage();
}

// ---------------------------------------------------------------------------
// Small HTML helpers
// ---------------------------------------------------------------------------
static String typeSelectHtml(const String& name, ValueType cur) {
  String s = "<select class='type' name='" + name + "'>";
  s += "<option value='0'" + String(cur == TYPE_FLOAT ? " selected" : "") + ">Float</option>";
  s += "<option value='1'" + String(cur == TYPE_INT ? " selected" : "") + ">Int</option>";
  s += "<option value='2'" + String(cur == TYPE_STRING ? " selected" : "") + ">String</option></select>";
  return s;
}

static String numericTypeSelectHtml(const String& name, ValueType cur) {
  ValueType c = (cur == TYPE_STRING) ? TYPE_FLOAT : cur;
  String s = "<select name='" + name + "'>";
  s += "<option value='0'" + String(c == TYPE_FLOAT ? " selected" : "") + ">Float</option>";
  s += "<option value='1'" + String(c == TYPE_INT ? " selected" : "") + ">Int</option></select>";
  return s;
}

static String clickModeHtml(const String& name, KeyMode cur, const String& prId, const String& sqId) {
  String s = "<div class='mode-box'><label>" + String(tr("Click Mode", "クリックモード")) + "</label>";
  s += "<select name='" + name + "' onchange=\"toggleClickMode('" + prId + "','" + sqId + "',this)\">";
  s += "<option value='0'" + String(cur == MODE_PRESS_RELEASE ? " selected" : "") + ">" + String(tr("Press / Release", "押した時／離した時")) + "</option>";
  s += "<option value='1'" + String(cur == MODE_SEQUENCE ? " selected" : "") + ">" + String(tr("Sequence (press only)", "シーケンス（押した時のみ）")) + "</option>";
  s += "</select></div>";
  return s;
}

static String messageRowHtml(const String& group, const String& prefix, const char* eventName, int order, const OSCMessage& m) {
  String idx = group.substring(prefix.length());
  String p = prefix + (String(eventName) == "press" ? "p" : "r");
  String row = "<div class='osc-row' data-group='" + group + "' data-prefix='" + prefix + "' data-event='" + eventName + "'>";
  row += "<div class='order'><button type='button' class='mv' onclick='moveMsg(this,-1)'>&uarr;</button><button type='button' class='mv' onclick='moveMsg(this,1)'>&darr;</button></div>";
  row += "<div class='field'><label>" + String(tr("OSC Address", "OSCアドレス")) + "</label><input class='msg-address' maxlength='192' name='" + p + "a_" + idx + "_" + String(order) + "' value='" + htmlEscape(m.address) + "' oninput='limitAndValidate(this,192)'><small><span class='err'></span><span class='bytes'></span></small></div>";
  row += "<div class='field'><label>" + String(tr("Type", "型")) + "</label>" + typeSelectHtml(p + "t_" + idx + "_" + String(order), m.valueType) + "<small></small></div>";
  row += "<div class='field'><label>" + String(tr("Value", "値")) + "</label><input class='msg-value' maxlength='128' name='" + p + "v_" + idx + "_" + String(order) + "' value='" + htmlEscape(m.valueStr) + "' oninput='limitAndValidate(this,128)'><small><span class='err'></span><span class='bytes'></span></small></div>";
  row += "<button type='button' class='remove-msg' onclick='removeMsg(this)'>" + String(tr("Delete", "削除")) + "</button></div>";
  return row;
}

static String addressInputHtml(const String& label, const String& name,
                               const String& value,
                               const String& extraClass = "") {
  String classes = "address-field";
  if (extraClass.length()) classes += " " + extraClass;
  return "<div class='" + classes + "'><label>" + label +
         "</label><input class='osc-address' maxlength='192' name='" + name +
         "' value='" + htmlEscape(value) +
         "' oninput='limitAndValidate(this,192)'><small><span class='err'></span>"
         "<span class='bytes'></span></small></div>";
}

static String clickMessagesHtml(const String& idx, const String& prefix, bool sequenceMode,
                                const OSCMessage* press, uint8_t pressCount,
                                const OSCMessage* release, uint8_t releaseCount) {
  String group=prefix+idx, out="<div id='"+prefix+"pr_"+idx+"' style='display:"+String(sequenceMode?"none":"block")+"'>";
  out += "<div class='usage'><strong>" + String(tr("Messages", "メッセージ")) + " <span id='count_"+group+"'>"+String(pressCount+releaseCount)+"</span> / 8</strong><span>" + String(tr("Press + Release", "押した時＋離した時")) + "</span></div>";
  out += "<input type='hidden' id='pc_"+group+"' name='"+prefix+"pc_"+idx+"' value='"+String(pressCount)+"'><input type='hidden' id='rc_"+group+"' name='"+prefix+"rc_"+idx+"' value='"+String(releaseCount)+"'>";
  out += "<div class='event-tabs'><button type='button' class='event-tab active' onclick=\"showEvent('"+group+"','press',this)\">" + String(tr("Press", "押した時")) + "</button><button type='button' class='event-tab' onclick=\"showEvent('"+group+"','release',this)\">" + String(tr("Release", "離した時")) + "</button></div>";
  out += "<div class='event-panel' data-group='"+group+"' data-event='press'><div class='osc-list' id='list_press_"+group+"'>";
  for(uint8_t i=0;i<pressCount;i++) out+=messageRowHtml(group,prefix,"press",i,press[i]);
  out += "</div><div class='empty'>" + String(tr("No OSC message is sent when pressed.", "押したときはOSCメッセージを送信しません。")) + "</div><button type='button' class='add-msg' data-group='"+group+"' data-prefix='"+prefix+"' data-event='press' onclick='addMsg(this)'>" + String(tr("+ Add OSC Message", "+ OSCメッセージを追加")) + "</button></div>";
  out += "<div class='event-panel' data-group='"+group+"' data-event='release' style='display:none'><div class='osc-list' id='list_release_"+group+"'>";
  for(uint8_t i=0;i<releaseCount;i++) out+=messageRowHtml(group,prefix,"release",i,release[i]);
  out += "</div><div class='empty'>" + String(tr("No OSC message is sent when released.", "離したときはOSCメッセージを送信しません。")) + "</div><button type='button' class='add-msg' data-group='"+group+"' data-prefix='"+prefix+"' data-event='release' onclick='addMsg(this)'>" + String(tr("+ Add OSC Message", "+ OSCメッセージを追加")) + "</button></div></div>";
  return out;
}

static bool validOscAddressText(const String& address, String& error) {
  if (!address.length() || !address.startsWith("/")) { error = tr("OSC Address must start with /.", "OSC Addressは / から始めてください。"); return false; }
  if (address.length() > MAX_OSC_ADDRESS_BYTES) { error = tr("OSC Address is too long.", "OSC Addressが長すぎます。"); return false; }
  for (size_t i = 0; i < address.length(); i++) {
    char c = address[i];
    if (isspace((unsigned char)c) || c == '#' || c == '*' || c == ',' || c == '?' || c == '[' || c == ']' || c == '{' || c == '}') {
      error = tr("OSC Address contains an invalid character.", "OSC Addressに使用できない文字が含まれています。"); return false;
    }
  }
  return true;
}

static bool validOscMessage(const OSCMessage& m, String& error) {
  String address = m.address;
  address.trim();
  if (!validOscAddressText(address, error)) return false;
  if (m.valueStr.length() > MAX_OSC_VALUE_BYTES) { error = tr("OSC Value is too long.", "OSC Valueが長すぎます。"); return false; }
  if (m.valueType == TYPE_FLOAT) {
    char* end = nullptr; float value = strtof(m.valueStr.c_str(), &end);
    if (!end || end == m.valueStr.c_str() || *end != '\0' || !isfinite(value)) { error = tr("Float value is invalid.", "Float値が正しくありません。"); return false; }
  } else if (m.valueType == TYPE_INT) {
    errno = 0; char* end = nullptr; long value = strtol(m.valueStr.c_str(), &end, 10);
    if (!end || end == m.valueStr.c_str() || *end != '\0' || errno == ERANGE || value < INT32_MIN || value > INT32_MAX) { error = tr("Integer value is invalid.", "Int値が正しくありません。"); return false; }
  }
  return true;
}

static bool parseMessageList(const String& idx,const String& prefix,OSCMessage* press,uint8_t& pc,OSCMessage* release,uint8_t& rc,String& error){
  int p=constrain(server.arg(prefix+"pc_"+idx).toInt(),0,MAX_KEY_OSC_MESSAGES),r=constrain(server.arg(prefix+"rc_"+idx).toInt(),0,MAX_KEY_OSC_MESSAGES);
  if(p+r>MAX_KEY_OSC_MESSAGES){error=tr("Press and Release messages must total 8 or fewer.","PressとReleaseのメッセージは合計8件以内にしてください。");return false;}
  pc=p;rc=r;
  for(int i=0;i<p;i++){press[i].address=server.arg(prefix+"pa_"+idx+"_"+String(i));press[i].valueStr=server.arg(prefix+"pv_"+idx+"_"+String(i));int t=server.arg(prefix+"pt_"+idx+"_"+String(i)).toInt();press[i].valueType=(ValueType)constrain(t,(int)TYPE_FLOAT,(int)TYPE_STRING);if(!validOscMessage(press[i],error))return false;}
  for(int i=0;i<r;i++){release[i].address=server.arg(prefix+"ra_"+idx+"_"+String(i));release[i].valueStr=server.arg(prefix+"rv_"+idx+"_"+String(i));int t=server.arg(prefix+"rt_"+idx+"_"+String(i)).toInt();release[i].valueType=(ValueType)constrain(t,(int)TYPE_FLOAT,(int)TYPE_STRING);if(!validOscMessage(release[i],error))return false;}
  return true;
}

static String jsonString(const String& value) {
  String out = "\"";
  for (size_t i = 0; i < value.length(); i++) {
    uint8_t c = (uint8_t)value[i];
    if (c == '\"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\b') out += "\\b";
    else if (c == '\f') out += "\\f";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else if (c < 0x20) {
      char escaped[7];
      snprintf(escaped, sizeof(escaped), "\\u%04X", c);
      out += escaped;
    } else out += (char)c;
  }
  out += "\"";
  return out;
}

static String messageJson(const OSCMessage& message) {
  return String("{\"address\":") + jsonString(message.address) +
         ",\"value\":" + jsonString(message.valueStr) +
         ",\"type\":" + String((int)message.valueType) + "}";
}
static String messageArrayJson(const OSCMessage* messages,uint8_t count){String out="[";for(uint8_t i=0;i<count;i++){if(i)out+=',';out+=messageJson(messages[i]);}return out+"]";}

static String sequenceJson(const SequenceConfig& sequence) {
  return String("{\"address\":") + jsonString(sequence.address) +
         ",\"type\":" + String((int)sequence.valueType) +
         ",\"start\":" + String(sequence.start, 6) +
         ",\"end\":" + String(sequence.end, 6) +
         ",\"step\":" + String(sequence.step, 6) + "}";
}

static String rangeJson(const RangeMap& map) {
  return String("{\"outMin\":") + String(map.outMin, 6) +
         ",\"outMax\":" + String(map.outMax, 6) +
         ",\"type\":" + String((int)map.outType) + "}";
}

static String deviceJson(const ChainDevice& device, bool includeIdentity = true) {
  String out = "{";
  if (includeIdentity) {
    out += String("\"uid\":") + jsonString(device.uid) +
           ",\"deviceType\":" + String((int)device.type) +
           ",\"deviceTypeName\":" + jsonString(String(typeToName(device.type))) +
           ",\"displayName\":" + jsonString(device.displayName);
  } else {
    out += String("\"format\":\"M5ChainOSC-device-preset\"") +
           ",\"schemaVersion\":1" +
           ",\"deviceType\":" + String((int)device.type) +
           ",\"deviceTypeName\":" + jsonString(String(typeToName(device.type)));
  }
  if (device.type == CHAIN_KEY_TYPE_CODE) {
    out += ",\"key\":{\"mode\":" + String((int)device.mode) + ",\"press\":[";
    for (uint8_t i = 0; i < device.pressMessageCount; i++) {
      if (i) out += ',';
      out += messageJson(device.pressMessages[i]);
    }
    out += "],\"release\":[";
    for (uint8_t i = 0; i < device.releaseMessageCount; i++) {
      if (i) out += ',';
      out += messageJson(device.releaseMessages[i]);
    }
    out += "],\"sequence\":" + sequenceJson(device.seq) + "}";
  } else if (device.type == CHAIN_ENCODER_TYPE_CODE) {
    out += ",\"encoder\":{\"rotationAddress\":" + jsonString(device.enc.rotAddr) +
           ",\"sendIncrement\":" + String(device.enc.sendIncrement ? "true" : "false") +
           ",\"absoluteInputMin\":" + String(device.enc.absInMin, 6) +
           ",\"absoluteInputMax\":" + String(device.enc.absInMax, 6) +
           ",\"incrementScale\":" + String(device.enc.incScale, 6) +
           ",\"range\":" + rangeJson(device.enc.map) +
           ",\"clickMode\":" + String((int)device.enc.clickMode) +
           ",\"press\":" + messageArrayJson(device.enc.pressMessages,device.enc.pressMessageCount) +
           ",\"release\":" + messageArrayJson(device.enc.releaseMessages,device.enc.releaseMessageCount) +
           ",\"sequence\":" + sequenceJson(device.enc.clickSeq) + "}";
  } else if (device.type == CHAIN_ANGLE_TYPE_CODE) {
    out += ",\"angle\":{\"address\":" + jsonString(device.angle.addr) +
           ",\"use12bit\":" + String(device.angle.use12bit ? "true" : "false") +
           ",\"deadband\":" + String(device.angle.deadband) +
           ",\"range\":" + rangeJson(device.angle.map) + "}";
  } else if (device.type == CHAIN_JOYSTICK_TYPE_CODE) {
    out += ",\"joystick\":{\"xAddress\":" + jsonString(device.joy.xAddr) +
           ",\"yAddress\":" + jsonString(device.joy.yAddr) +
           ",\"deadband\":" + String(device.joy.deadband) +
           ",\"invertX\":" + String(device.joy.invertX ? "true" : "false") +
           ",\"invertY\":" + String(device.joy.invertY ? "true" : "false") +
           ",\"range\":" + rangeJson(device.joy.map) +
           ",\"clickMode\":" + String((int)device.joy.clickMode) +
           ",\"press\":" + messageArrayJson(device.joy.pressMessages,device.joy.pressMessageCount) +
           ",\"release\":" + messageArrayJson(device.joy.releaseMessages,device.joy.releaseMessageCount) +
           ",\"sequence\":" + sequenceJson(device.joy.clickSeq) + "}";
  } else if (device.type == CHAIN_TOF_TYPE_CODE) {
    out += ",\"tof\":{\"address\":" + jsonString(device.tof.addr) +
           ",\"deadband\":" + String(device.tof.deadband) +
           ",\"maxDistanceMm\":" + String(device.tof.maxDistanceMm) +
           ",\"nearValueHigh\":" + String(device.tof.nearValueHigh ? "true" : "false") +
           ",\"range\":" + rangeJson(device.tof.map) + "}";
  }
  out += '}';
  return out;
}

static bool jsonMessage(JsonObjectConst object, OSCMessage& message, String& error) {
  if (object.isNull() || !object["address"].is<const char*>() || !object["value"].is<const char*>() || !object["type"].is<int>()) {
    error = "OSC message has missing or invalid fields."; return false;
  }
  message.address = object["address"].as<const char*>();
  message.valueStr = object["value"].as<const char*>();
  int type = object["type"].as<int>();
  if (type < TYPE_FLOAT || type > TYPE_STRING) { error = "OSC message type is invalid."; return false; }
  message.valueType = (ValueType)type;
  return validOscMessage(message, error);
}
static bool jsonMessageArrays(JsonVariantConst pv,JsonVariantConst rv,OSCMessage* press,uint8_t& pc,OSCMessage* release,uint8_t& rc,String& error){if(!pv.is<JsonArrayConst>()||!rv.is<JsonArrayConst>()){error="Click message arrays are missing.";return false;}JsonArrayConst p=pv.as<JsonArrayConst>(),r=rv.as<JsonArrayConst>();if(p.size()+r.size()>MAX_KEY_OSC_MESSAGES){error="Click messages exceed the limit of 8.";return false;}pc=p.size();rc=r.size();uint8_t i=0;for(JsonObjectConst m:p)if(!jsonMessage(m,press[i++],error))return false;i=0;for(JsonObjectConst m:r)if(!jsonMessage(m,release[i++],error))return false;return true;}

static bool jsonSequence(JsonObjectConst object, SequenceConfig& sequence, String& error) {
  if (object.isNull() || !object["address"].is<const char*>() || !object["type"].is<int>() ||
      !object.containsKey("start") || !object.containsKey("end") || !object.containsKey("step")) {
    error = "Sequence has missing or invalid fields."; return false;
  }
  sequence.address = object["address"].as<const char*>();
  sequence.address.trim();
  if (!validOscAddressText(sequence.address, error)) return false;
  int type = object["type"].as<int>();
  if (type < TYPE_FLOAT || type > TYPE_STRING) { error = "Sequence type is invalid."; return false; }
  sequence.valueType = (ValueType)type;
  sequence.start = object["start"].as<float>();
  sequence.end = object["end"].as<float>();
  sequence.step = object["step"].as<float>();
  if (!isfinite(sequence.start) || !isfinite(sequence.end) || !isfinite(sequence.step)) {
    error = "Sequence contains a non-finite number."; return false;
  }
  normalizeSequence(sequence);
  return true;
}

static bool jsonRange(JsonObjectConst object, RangeMap& range, String& error) {
  if (object.isNull() || !object.containsKey("outMin") || !object.containsKey("outMax") || !object["type"].is<int>()) {
    error = "Range has missing or invalid fields."; return false;
  }
  range.outMin = object["outMin"].as<float>();
  range.outMax = object["outMax"].as<float>();
  int type = object["type"].as<int>();
  if (!isfinite(range.outMin) || !isfinite(range.outMax) || type < TYPE_FLOAT || type > TYPE_STRING) {
    error = "Range value or type is invalid."; return false;
  }
  range.outType = (ValueType)type;
  return true;
}

static bool jsonAddress(JsonVariantConst value, String& address, String& error) {
  if (!value.is<const char*>()) { error = "OSC Address is missing."; return false; }
  address = value.as<const char*>();
  address.trim();
  return validOscAddressText(address, error);
}

static bool deviceFromJson(JsonObjectConst object, ChainDevice& device, String& error) {
  if (object.isNull() || !object["uid"].is<const char*>() || !object["deviceType"].is<int>()) {
    error = "Device UID or type is missing."; return false;
  }
  device = ChainDevice();
  device.uid = object["uid"].as<const char*>();
  device.uid.trim();
  if (!device.uid.length() || isPlaceholderUid(device.uid)) { error = "Device UID is invalid."; return false; }
  device.uidShort = device.uid.substring(max(0, (int)device.uid.length() - 8));
  device.type = (chain_device_type_t)object["deviceType"].as<int>();
  setDefaultDeviceMessages(device);
  if (object["displayName"].is<const char*>()) device.displayName = object["displayName"].as<const char*>();
  if (device.displayName.length() > MAX_DEVICE_NAME_BYTES) { error = "Device Name is too long."; return false; }

  if (device.type == CHAIN_KEY_TYPE_CODE) {
    JsonObjectConst key = object["key"].as<JsonObjectConst>();
    if (key.isNull() || !key["mode"].is<int>() || !key["press"].is<JsonArrayConst>() || !key["release"].is<JsonArrayConst>()) {
      error = "Key settings are missing."; return false;
    }
    int mode = key["mode"].as<int>();
    if (mode < MODE_PRESS_RELEASE || mode > MODE_SEQUENCE) { error = "Key mode is invalid."; return false; }
    device.mode = (KeyMode)mode;
    JsonArrayConst press = key["press"].as<JsonArrayConst>();
    JsonArrayConst release = key["release"].as<JsonArrayConst>();
    if (press.size() + release.size() > MAX_KEY_OSC_MESSAGES) { error = "Key messages exceed the limit of 8."; return false; }
    device.pressMessageCount = (uint8_t)press.size();
    device.releaseMessageCount = (uint8_t)release.size();
    uint8_t i = 0;
    for (JsonObjectConst message : press) if (!jsonMessage(message, device.pressMessages[i++], error)) return false;
    i = 0;
    for (JsonObjectConst message : release) if (!jsonMessage(message, device.releaseMessages[i++], error)) return false;
    if (!jsonSequence(key["sequence"].as<JsonObjectConst>(), device.seq, error)) return false;
    if (device.pressMessageCount) device.press = device.pressMessages[0];
    if (device.releaseMessageCount) device.release = device.releaseMessages[0];
  } else if (device.type == CHAIN_ENCODER_TYPE_CODE) {
    JsonObjectConst v = object["encoder"].as<JsonObjectConst>();
    if (v.isNull() || !jsonAddress(v["rotationAddress"], device.enc.rotAddr, error)) return false;
    device.enc.sendIncrement = v["sendIncrement"] | false;
    device.enc.absInMin = v["absoluteInputMin"].as<float>(); device.enc.absInMax = v["absoluteInputMax"].as<float>();
    device.enc.incScale = v["incrementScale"].as<float>();
    if (!isfinite(device.enc.absInMin) || !isfinite(device.enc.absInMax) || !isfinite(device.enc.incScale)) { error = "Encoder number is invalid."; return false; }
    if (!jsonRange(v["range"].as<JsonObjectConst>(), device.enc.map, error)) return false;
    int mode = v["clickMode"] | 0; device.enc.clickMode = mode == MODE_SEQUENCE ? MODE_SEQUENCE : MODE_PRESS_RELEASE;
    if (!jsonMessageArrays(v["press"],v["release"],device.enc.pressMessages,device.enc.pressMessageCount,device.enc.releaseMessages,device.enc.releaseMessageCount,error) || !jsonSequence(v["sequence"].as<JsonObjectConst>(), device.enc.clickSeq, error)) return false;
    if(device.enc.pressMessageCount)device.enc.press=device.enc.pressMessages[0];if(device.enc.releaseMessageCount)device.enc.release=device.enc.releaseMessages[0];
  } else if (device.type == CHAIN_ANGLE_TYPE_CODE) {
    JsonObjectConst v = object["angle"].as<JsonObjectConst>();
    if (v.isNull() || !jsonAddress(v["address"], device.angle.addr, error)) return false;
    device.angle.use12bit = v["use12bit"] | true; device.angle.deadband = v["deadband"] | 8;
    if (!jsonRange(v["range"].as<JsonObjectConst>(), device.angle.map, error)) return false;
  } else if (device.type == CHAIN_JOYSTICK_TYPE_CODE) {
    JsonObjectConst v = object["joystick"].as<JsonObjectConst>();
    if (v.isNull() || !jsonAddress(v["xAddress"], device.joy.xAddr, error) || !jsonAddress(v["yAddress"], device.joy.yAddr, error)) return false;
    device.joy.deadband = v["deadband"] | 3; device.joy.invertX = v["invertX"] | false; device.joy.invertY = v["invertY"] | false;
    if (!jsonRange(v["range"].as<JsonObjectConst>(), device.joy.map, error)) return false;
    int mode = v["clickMode"] | 0; device.joy.clickMode = mode == MODE_SEQUENCE ? MODE_SEQUENCE : MODE_PRESS_RELEASE;
    if (!jsonMessageArrays(v["press"],v["release"],device.joy.pressMessages,device.joy.pressMessageCount,device.joy.releaseMessages,device.joy.releaseMessageCount,error) || !jsonSequence(v["sequence"].as<JsonObjectConst>(), device.joy.clickSeq, error)) return false;
    if(device.joy.pressMessageCount)device.joy.press=device.joy.pressMessages[0];if(device.joy.releaseMessageCount)device.joy.release=device.joy.releaseMessages[0];
  } else if (device.type == CHAIN_TOF_TYPE_CODE) {
    JsonObjectConst v = object["tof"].as<JsonObjectConst>();
    if (v.isNull() || !jsonAddress(v["address"], device.tof.addr, error)) return false;
    device.tof.deadband = v["deadband"] | 5;
    device.tof.maxDistanceMm = v["maxDistanceMm"] | 2000;
    device.tof.nearValueHigh = v["nearValueHigh"] | false;
    if (device.tof.deadband < 1 || device.tof.deadband > 2000 ||
        device.tof.maxDistanceMm < 31 || device.tof.maxDistanceMm > 2000) {
      error = "ToF distance or deadband is out of range."; return false;
    }
    if (!jsonRange(v["range"].as<JsonObjectConst>(), device.tof.map, error)) return false;
    if (device.tof.map.outType != TYPE_FLOAT && device.tof.map.outType != TYPE_INT) {
      error = "ToF output type must be Float or Int."; return false;
    }
    device.tof.map.inMin = 30;
    device.tof.map.inMax = device.tof.maxDistanceMm;
  } else { error = "Unsupported device type."; return false; }
  if (deviceConfigStorageBytes(device) > MAX_DEVICE_CONFIG_BYTES) { error = "Device configuration is too large."; return false; }
  return true;
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------
void registerWebRoutes() {
  const char* trackedHeaders[] = {"Accept-Language"};
  server.collectHeaders(trackedHeaders, 1);
  server.on("/", handleRoot);
  server.on("/set_language", HTTP_POST, handleSetLanguage);
  server.on("/save", handleSave);
  server.on("/delete_wifi", handleDeleteWifi);
  server.on("/delete_device", handleDeleteDevice);
  server.on("/set_rotation", handleSetRotation);
  server.on("/export_settings", HTTP_GET, handleExportSettings);
  server.on("/import_settings", HTTP_POST, handleImportSettings);
  server.on("/export_device_preset", HTTP_GET, handleExportDevicePreset);
  server.on("/import_device_preset", HTTP_POST, handleImportDevicePreset);
  server.on("/identify_device", HTTP_POST, handleIdentifyDevice);
}

// ---------------------------------------------------------------------------
// AP captive portal
// ---------------------------------------------------------------------------
void handleAPRoot() {
  applyBrowserLanguageOnFirstVisit();
  String html = "<!doctype html><html lang='" + String(isJapaneseUi() ? "ja" : "en") + "'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>WiFi Setup</title></head><body>";
  html += "<form method='POST' action='/set_language'><label>" + String(tr("Language", "言語")) + "</label><select name='language' onchange='this.form.submit()'><option value='en'" + String(!isJapaneseUi() ? " selected" : "") + ">English</option><option value='ja'" + String(isJapaneseUi() ? " selected" : "") + ">日本語</option></select></form>";
  html += "<h2>" + String(tr("WiFi Setup", "Wi-Fi設定")) + "</h2>";
  html += "<p role='alert' style='padding:12px;border:1px solid #d99b22;border-radius:8px;background:#fff4d6;color:#5f4300;font-weight:bold;line-height:1.5'>" + String(tr(
      "AtomS3R supports 2.4 GHz Wi-Fi only and cannot connect to 5 GHz-only networks. Select a 2.4 GHz SSID.",
      "AtomS3Rが接続できるWi-Fiは2.4 GHz帯のみです。5 GHz帯専用のSSIDには接続できません。2.4 GHz帯に対応するSSIDを選択してください。")) + "</p>";
  html += "<form method='POST' action='/save_wifi'>";
  html += "SSID<input name='ssid'><br>" + String(tr("Password", "パスワード")) + "<input type='password' name='password'><br>";
  html += "<button type='submit'>" + String(tr("Save & Restart", "保存して再起動")) + "</button></form></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSaveWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password") && server.arg("ssid").length()) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", server.arg("ssid"));
    prefs.putString("password", server.arg("password"));
    prefs.end();
    server.send(200, "text/html; charset=utf-8", String("<meta charset='utf-8'><h2>") + tr("Saved", "保存しました") + "</h2>");
    delay(1500);
    ESP.restart();
  }
  server.send(400, "text/plain; charset=utf-8", tr("Error", "エラー"));
}

void handleSetLanguage() {
  if (server.hasArg("language")) {
    uiLanguage = server.arg("language") == "ja" ? UI_LANG_JAPANESE : UI_LANG_ENGLISH;
    saveUiLanguage();
  }
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

// ---------------------------------------------------------------------------
// Main settings page
// ---------------------------------------------------------------------------
void handleRoot() {
  MEMORY_DEBUG_LOG("WEB_ROOT_BEGIN", 0);
  applyBrowserLanguageOnFirstVisit();
#if M5CHAINOSC_WEB_PERF_DEBUG
  const uint32_t requestId = ++webPerfRequestSequence;
#else
  const uint32_t requestId = 0;
#endif
  const uint32_t requestStart = millis();
  WEB_PERF_LOG(requestId, requestStart, "BEGIN", 0, 0);
  // WebServer runs handlers serially, so retaining this buffer is safe and
  // avoids allocating and freeing one large block on every page reload.
  static String html;
  html.remove(0);
  html.reserve(20000);
  html += R"raw(
<!DOCTYPE html><html lang="__LANG__"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OSC Settings</title>
<style>
body{font-family:sans-serif;margin:16px;background:#f5f5f5}
.card{background:#fff;padding:16px;border-radius:10px;margin-bottom:16px;box-shadow:0 2px 5px rgba(0,0,0,.1)}
.saved-settings{margin-top:28px}
.tool-card .note{margin-bottom:10px}.tool-row{display:grid;grid-template-columns:1fr 1fr;gap:8px}.tool-row a,.tool-row button{box-sizing:border-box;text-align:center;text-decoration:none;margin:0;padding:11px;border-radius:6px;font-size:15px}.tool-row a{display:block;background:#3267e3;color:#fff}.tool-row button{background:#fff;color:#3267e3;border:1px solid #3267e3}.tool-status{min-height:18px;margin:7px 0 0}@media(max-width:720px){.osc-row{grid-template-columns:52px 1fr}.osc-row .field,.remove-msg{grid-column:2}.key-grid,.seq-grid{grid-template-columns:1fr}.seq-address{grid-column:1}}@media(max-width:520px){.tool-row{grid-template-columns:1fr}}
h1{font-size:1.4em}h2{margin-top:0;font-size:1.1em}
label{display:block;margin-top:10px;font-weight:bold;font-size:.9em}
input,select{width:100%;padding:8px;margin-top:4px;box-sizing:border-box}
input.invalid,select.invalid{border:2px solid #c73c4a;background:#fff8f8}
.key-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:start}.key-grid label{margin-top:0}
.usage{display:flex;justify-content:space-between;align-items:center;margin:14px 0;padding:11px 13px;border:1px solid #cddbf8;border-radius:9px;background:#edf3ff;color:#244da7}
.event-tabs{display:flex;gap:4px;padding:4px;background:#edf0f4;border-radius:9px}.event-tab{margin:0;background:transparent;color:#697586}.event-tab.active{background:white;color:#18212f;box-shadow:0 1px 4px #bbb}
.event-panel{margin-top:12px}.osc-list{display:grid;gap:10px}.osc-row{display:grid;grid-template-columns:62px minmax(180px,1fr) 115px minmax(100px,.55fr) 68px;gap:9px;align-items:start;padding:12px;border:1px solid #dce2ea;border-radius:10px;background:#fbfcfe}
.osc-row .field label{margin-top:0}.osc-row small,.address-field small{display:flex;justify-content:space-between;min-height:17px;color:#697586}.osc-row .err,.address-field .err{color:#c73c4a}.order{display:flex;gap:3px;align-self:center}.mv{width:auto;margin:0;padding:7px;background:#fff;color:#526075;border:1px solid #dce2ea}.remove-msg{width:auto;margin-top:22px;padding:9px;background:#fff3f4;color:#c73c4a;border:1px solid #efc6cb}.add-msg{background:#f7faff;color:#3267e3;border:1px dashed #9db6ef}.add-msg:disabled{background:#eee;color:#888}.empty{display:none;padding:18px;text-align:center;color:#697586;border:1px dashed #dce2ea;border-radius:9px}.osc-list:empty+.empty{display:block}
.sequence-card{margin-top:12px;padding:15px;border:1px solid #dce2ea;border-radius:10px;background:#fbfcfe}.seq-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.seq-address{grid-column:1/-1}
button{width:100%;padding:12px;background:#28a745;color:#fff;border:none;border-radius:6px;font-size:16px;margin-top:8px}
.save-bar{position:sticky;z-index:15;bottom:8px;display:flex;align-items:center;gap:12px;padding:10px 12px;margin-top:16px;background:rgba(255,255,255,.96);border:1px solid #dce2ea;border-radius:10px;box-shadow:0 5px 18px rgba(0,0,0,.14)}.save-bar button{flex:1;margin:0}.dirty-status{flex:0 0 auto;color:#a45a00;font-size:.9em;font-weight:bold}.dirty-status[hidden]{display:none}
.toast{position:fixed;z-index:50;top:16px;left:50%;transform:translate(-50%,-12px);max-width:min(520px,calc(100% - 32px));padding:12px 18px;border-radius:9px;color:#fff;font-weight:bold;box-shadow:0 6px 22px rgba(0,0,0,.22);opacity:0;pointer-events:none;transition:opacity .18s,transform .18s}.toast.show{opacity:1;transform:translate(-50%,0)}.toast.success{background:#218838}.toast.error{background:#c73c4a}
.btn-danger{background:#dc3545}.btn-warning{background:#ff9800}.btn-export{background:#3267e3}.btn-rot{background:#6f42c1;flex:1;margin:0}
.device{position:relative}.device-head{display:flex;align-items:flex-start;justify-content:space-between;gap:10px;margin-bottom:10px}.device-head h2{display:flex;align-items:center;gap:4px;margin-bottom:0}.collapse-button{width:30px;height:30px;margin:0 3px 0 0;padding:0;background:#f1f4f8;color:#42516a;border:1px solid #dce2ea;border-radius:7px;font-size:16px;line-height:1;transition:transform .15s}.collapse-button.collapsed{transform:rotate(-90deg)}.device-body[hidden]{display:none}.device-menu-wrap{position:relative;flex:0 0 auto}.more-button{width:34px;height:30px;margin:0;padding:0;background:#f1f4f8;color:#42516a;border:1px solid #dce2ea;border-radius:7px;font-size:18px;line-height:1}.device-menu{display:none;position:absolute;z-index:20;right:0;top:36px;width:235px;padding:8px;background:#fff;border:1px solid #dce2ea;border-radius:10px;box-shadow:0 8px 24px rgba(0,0,0,.18)}.device-menu.open{display:block}.device-menu a,.device-menu button{display:block;box-sizing:border-box;width:100%;margin:0;padding:10px;text-align:left;text-decoration:none;border-radius:7px;background:#fff;color:#253550;border:0;font-size:14px}.device-menu a:hover,.device-menu button:hover{background:#edf3ff}.device-menu .menu-note{padding:6px 10px 8px;color:#7a8494;font-size:12px}.preset-status{min-height:18px;margin:5px 10px;color:#666;font-size:12px}
.btn-rot-cur{background:#9b59b6;font-weight:bold;box-shadow:inset 0 0 0 2px #fff}
.rot-row{display:flex;gap:8px;margin-top:10px}
.press{border-left:5px solid #dc3545;padding-left:10px;margin-top:12px}
.release{border-left:5px solid #007bff;padding-left:10px;margin-top:12px}
.seq{border-left:5px solid #20c997;padding-left:10px;margin-top:12px}
.click-sequence{padding:10px;margin-top:12px}
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
.click-section{margin-top:14px;padding-left:12px;border-left:5px solid #20c997}.click-section>.mode-box{margin-top:0}
.chk{display:flex;align-items:center;gap:8px;margin-top:6px;margin-bottom:4px}
.chk input{width:auto}
.language-row{display:flex;align-items:center;justify-content:space-between;gap:12px}.language-row h2{margin:0}.language-row form{margin:0;min-width:150px}.language-row select{margin:0}
</style>
<script>
const JA=__JA__;const tx=(en,ja)=>JA?ja:en;const MAX_MSG=8;const enc=new TextEncoder();
function bytes(v){return enc.encode(v).length}
function toggleMode(pr,sq,sel){if(!pr||!sq)return;if(sel.value==='1'){pr.style.display='none';sq.style.display='block';}else{pr.style.display='block';sq.style.display='none';}}
function toggleClickMode(prId,sqId,sel){toggleMode(document.getElementById(prId),document.getElementById(sqId),sel);}
function updateEncoderMode(sel){let enc=sel.closest('.enc'),showAbsolute=sel.value==='0';if(!enc)return;enc.querySelectorAll('.encoder-absolute-setting').forEach(x=>x.style.display=showAbsolute?'':'none')}
function showEvent(group,event,btn){document.querySelectorAll('.event-panel[data-group="'+group+'"]').forEach(x=>x.style.display=x.dataset.event===event?'block':'none');btn.parentNode.querySelectorAll('.event-tab').forEach(x=>x.classList.remove('active'));btn.classList.add('active')}
function allRows(group){return document.querySelectorAll('.osc-row[data-group="'+group+'"]')}
function renumber(group){let rows=allRows(group),prefix=rows.length?rows[0].dataset.prefix:document.querySelector('.add-msg[data-group="'+group+'"]').dataset.prefix,idx=group.substring(prefix.length);['press','release'].forEach(ev=>{document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="'+ev+'"]').forEach((r,i)=>{let p=prefix+(ev==='press'?'p':'r');r.querySelector('.msg-address').name=p+'a_'+idx+'_'+i;r.querySelector('.type').name=p+'t_'+idx+'_'+i;r.querySelector('.msg-value').name=p+'v_'+idx+'_'+i})});let n=rows.length;document.getElementById('count_'+group).textContent=n;document.getElementById('pc_'+group).value=document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="press"]').length;document.getElementById('rc_'+group).value=document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="release"]').length;document.querySelectorAll('.add-msg[data-group="'+group+'"]').forEach(b=>b.disabled=n>=MAX_MSG)}
function markDirty(){let s=document.getElementById('dirty-status');if(!s)return;window.settingsDirty=true;s.hidden=false}
function moveMsg(btn,d){let r=btn.closest('.osc-row'),s=d<0?r.previousElementSibling:r.nextElementSibling;if(!s)return;d<0?r.parentNode.insertBefore(r,s):r.parentNode.insertBefore(s,r);renumber(r.dataset.group);markDirty()}
function removeMsg(btn){let r=btn.closest('.osc-row'),g=r.dataset.group;r.remove();renumber(g);markDirty()}
function addMsg(btn){let g=btn.dataset.group,prefix=btn.dataset.prefix,ev=btn.dataset.event;if(allRows(g).length>=MAX_MSG)return;let list=document.getElementById('list_'+ev+'_'+g),r=document.createElement('div');r.className='osc-row';r.dataset.group=g;r.dataset.prefix=prefix;r.dataset.event=ev;r.innerHTML='<div class="order"><button type="button" class="mv" onclick="moveMsg(this,-1)">&uarr;</button><button type="button" class="mv" onclick="moveMsg(this,1)">&darr;</button></div><div class="field"><label>'+tx('OSC Address','OSCアドレス')+'</label><input class="msg-address" maxlength="192" oninput="limitAndValidate(this,192)"><small><span class="err"></span><span class="bytes"></span></small></div><div class="field"><label>'+tx('Type','型')+'</label><select class="type" onchange="validateInput(this.closest(\'.osc-row\').querySelector(\'.msg-value\'))"><option value="0">Float</option><option value="1">Int</option><option value="2">String</option></select><small></small></div><div class="field"><label>'+tx('Value','値')+'</label><input class="msg-value" maxlength="128" value="1.0" oninput="limitAndValidate(this,128)"><small><span class="err"></span><span class="bytes"></span></small></div><button type="button" class="remove-msg" onclick="removeMsg(this)">'+tx('Delete','削除')+'</button>';list.appendChild(r);renumber(g);markDirty();r.querySelector('.msg-address').focus()}
function limitBytes(i,max){while(bytes(i.value)>max)i.value=i.value.slice(0,-1)}
function limitAndValidate(i,max){limitBytes(i,max);validateInput(i)}
function validateInput(i){let isAddress=i.classList.contains('msg-address')||i.classList.contains('osc-address'),max=isAddress?192:128,b=bytes(i.value),err='';if(isAddress){if(!i.value)err=tx('Required','必須です');else if(i.value[0]!='/')err=tx('Start with /','/ から始めてください');else if(/[\s#*,?\[\]{}]/.test(i.value))err=tx('Invalid character','使用できない文字があります')}else if(i.classList.contains('msg-value')){let t=i.closest('.osc-row').querySelector('.type').value;if(t==='0'&&(!i.value.trim()||!Number.isFinite(Number(i.value))))err=tx('Invalid float','小数として正しくありません');if(t==='1'&&!/^[+-]?\d+$/.test(i.value.trim()))err=tx('Invalid integer','整数として正しくありません')}if(b>max)err=tx('Too long','長すぎます');i.classList.toggle('invalid',!!err);let sm=i.parentNode.querySelector('small');sm.querySelector('.err').textContent=err;sm.querySelector('.bytes').textContent=b+' / '+max+' bytes';return !err}
function initializeMessageRows(){let groups=new Set();document.querySelectorAll('.osc-row').forEach(row=>{groups.add(row.dataset.group);validateInput(row.querySelector('.msg-address'));validateInput(row.querySelector('.msg-value'))});document.querySelectorAll('.add-msg[data-group]').forEach(button=>groups.add(button.dataset.group));groups.forEach(group=>renumber(group));document.querySelectorAll('.osc-address').forEach(validateInput)}
function rememberScroll(){sessionStorage.setItem('m5osc-scroll',String(window.scrollY))}
function validateForm(){let ok=true;document.querySelectorAll('.msg-address,.msg-value,.osc-address').forEach(i=>{if(!validateInput(i))ok=false});if(!ok){let bad=document.querySelector('.invalid');if(bad)bad.focus();alert(tx('Please correct the highlighted OSC fields.','赤く表示されたOSC設定項目を修正してください。'))}return ok}
let toastTimer;function showToast(message,success){let toast=document.getElementById('save-toast');toast.textContent=message;toast.className='toast '+(success?'success':'error')+' show';clearTimeout(toastTimer);toastTimer=setTimeout(()=>toast.classList.remove('show'),success?3000:6000)}
async function saveSettings(event){event.preventDefault();if(!validateForm())return false;let form=event.currentTarget,button=form.querySelector('.save-bar button'),oldText=button.textContent,params=new URLSearchParams();new FormData(form).forEach((value,key)=>params.append(key,value));window.settingsSubmitting=true;button.disabled=true;button.textContent=tx('Saving...','保存中...');try{let response=await fetch('/save?ajax=1',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:params.toString()}),message=await response.text();if(!response.ok)throw new Error(message||tx('Settings could not be saved.','設定を保存できませんでした。'));window.settingsDirty=false;let dirty=document.getElementById('dirty-status');if(dirty)dirty.hidden=true;showToast(message,true)}catch(error){showToast(error.message,false)}finally{window.settingsSubmitting=false;button.disabled=false;button.textContent=oldText}return false}
async function deleteSavedDevice(event,form){event.preventDefault();if(!confirm(tx('Delete these settings?','この設定を削除しますか？')))return false;let button=form.querySelector('button'),oldText=button.textContent,params=new URLSearchParams();new FormData(form).forEach((value,key)=>params.append(key,value));button.disabled=true;button.textContent=tx('Deleting...','削除中...');try{let response=await fetch('/delete_device?ajax=1',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:params.toString()}),message=await response.text();if(!response.ok)throw new Error(message||tx('The saved device settings could not be deleted.','保存済みデバイス設定を削除できませんでした。'));let card=form.closest('.saved-device-card');if(card)card.remove();showToast(message,true)}catch(error){button.disabled=false;button.textContent=oldText;showToast(error.message,false)}return false}
function showImportError(status,reason){let message=tx('Import failed. The selected JSON file is not valid for this import.','インポートに失敗しました。選択したJSONファイルはこのインポートには使用できません。')+(reason?'\n\n'+reason:'');status.textContent=message.replace(/\n+/g,' ');alert(message)}
async function importSettings(){let input=document.getElementById('import-file'),status=document.getElementById('import-status');if(!input.files.length)return;let file=input.files[0];if(file.size>49152){showImportError(status,tx('The JSON file is too large.','JSONファイルが大きすぎます。'));input.value='';return}if(!confirm(tx('Import the settings in this file? Matching device settings will be overwritten.','このファイルの設定をインポートしますか？同じデバイスの設定は上書きされます。'))){input.value='';return}status.textContent=tx('Importing...','インポート中...');try{let body=await file.text(),response=await fetch('/import_settings',{method:'POST',headers:{'Content-Type':'application/json'},body});let message=await response.text();if(!response.ok)throw new Error(message);status.textContent=message;setTimeout(()=>location.reload(),1000)}catch(e){showImportError(status,e.message)}finally{input.value=''}}
function chooseSettingsFile(){document.getElementById('import-file').click()}
function closeDeviceMenus(except){document.querySelectorAll('.device-menu.open').forEach(menu=>{if(menu!==except){menu.classList.remove('open');let button=menu.parentNode.querySelector('.more-button');if(button)button.setAttribute('aria-expanded','false')}})}
function toggleDeviceMenu(event,index){event.stopPropagation();let menu=document.getElementById('device-menu-'+index),opening=!menu.classList.contains('open');closeDeviceMenus(menu);menu.classList.toggle('open',opening);event.currentTarget.setAttribute('aria-expanded',opening?'true':'false')}
function toggleDeviceCollapse(index,key){let body=document.getElementById('device-body-'+index),button=document.getElementById('collapse-'+index),collapsed=!body.hidden;body.hidden=collapsed;button.classList.toggle('collapsed',collapsed);button.setAttribute('aria-expanded',collapsed?'false':'true');sessionStorage.setItem('m5osc-collapse-'+key,collapsed?'1':'0')}
function chooseDevicePreset(index){document.getElementById('preset-file-'+index).click()}
async function identifyDevice(index,uid){closeDeviceMenus();let status=document.getElementById('preset-status-'+index);status.textContent='';try{let body='index='+encodeURIComponent(index)+'&uid='+encodeURIComponent(uid),response=await fetch('/identify_device',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}),message=await response.text();if(!response.ok)throw new Error(message);status.textContent=''}catch(e){status.textContent=e.message;alert(e.message)}}
async function importDevicePreset(index,input){let status=document.getElementById('preset-status-'+index);if(!input.files.length)return;let file=input.files[0];if(file.size>16384){showImportError(status,tx('The preset file is too large.','プリセットファイルが大きすぎます。'));input.value='';return}if(!confirm(tx('Apply this preset to the selected device? Its device settings will be overwritten.','選択したデバイスへこのプリセットを適用しますか？デバイス設定は上書きされます。'))){input.value='';return}status.textContent=tx('Importing preset...','プリセットをインポート中...');try{let body=await file.text(),response=await fetch('/import_device_preset?index='+index,{method:'POST',headers:{'Content-Type':'application/json'},body});let message=await response.text();if(!response.ok)throw new Error(message);status.textContent=message;setTimeout(()=>location.reload(),800)}catch(e){showImportError(status,e.message)}finally{input.value=''}}
document.addEventListener('click',()=>closeDeviceMenus());
function initializePage(){let form=document.getElementById('settings-form');if(form){form.addEventListener('input',markDirty);form.addEventListener('change',markDirty)}initializeMessageRows();document.querySelectorAll('.device[data-collapse-key]').forEach(card=>{let key=card.dataset.collapseKey,index=card.dataset.deviceIndex;if(sessionStorage.getItem('m5osc-collapse-'+key)==='1'){let body=document.getElementById('device-body-'+index),button=document.getElementById('collapse-'+index);body.hidden=true;button.classList.add('collapsed');button.setAttribute('aria-expanded','false')}});let saved=sessionStorage.getItem('m5osc-scroll');if(saved!==null){sessionStorage.removeItem('m5osc-scroll');requestAnimationFrame(()=>window.scrollTo(0,Number(saved)||0))}document.querySelectorAll('form:not(#settings-form)').forEach(f=>f.addEventListener('submit',rememberScroll))}
window.settingsDirty=false;window.settingsSubmitting=false;window.addEventListener('pageshow',()=>window.settingsSubmitting=false);window.addEventListener('beforeunload',e=>{if(window.settingsDirty&&!window.settingsSubmitting){e.preventDefault();e.returnValue=''}});window.addEventListener('DOMContentLoaded',initializePage)
</script></head><body><div id='save-toast' class='toast' role='status' aria-live='polite'></div><h1>Chain OSC Setting</h1>
)raw";

  html.replace("__LANG__", isJapaneseUi() ? "ja" : "en");
  html.replace("__JA__", isJapaneseUi() ? "true" : "false");

  html += "<div class='card language-row'><h2>" + String(tr("Language", "言語")) + "</h2><form action='/set_language' method='POST'>";
  html += "<select name='language' onchange='this.form.submit()'><option value='en'" + String(!isJapaneseUi() ? " selected" : "") + ">English</option><option value='ja'" + String(isJapaneseUi() ? " selected" : "") + ">日本語</option></select></form></div>";

  html += "<div class='card'><h2>WiFi</h2><p class='meta'>IP: " + ipStr + "</p>";
  html += "<form action='/delete_wifi' method='POST' onsubmit=\"return confirm('" + String(tr("Delete WiFi settings?", "Wi-Fi設定を削除しますか？")) + "');\">";
  html += "<button class='btn-danger' type='submit'>" + String(tr("Delete WiFi Settings", "Wi-Fi設定を削除")) + "</button></form></div>";

  html += "<div class='card tool-card'><h2>" + String(tr("Settings Backup &amp; Restore", "設定のバックアップと復元")) + "</h2>";
  html += "<p class='note'>" + String(tr("Back up or restore all M5ChainOSC settings as versioned JSON. WiFi credentials are not included.", "M5ChainOSCの全設定をバージョン付きJSONでバックアップ・復元します。Wi-Fi認証情報は含まれません。")) + "</p>";
  html += "<input id='import-file' type='file' accept='application/json,.json' hidden onchange='importSettings()'>";
  html += "<div class='tool-row'><a href='/export_settings'>" + String(tr("Export Settings (JSON)", "設定をエクスポート（JSON）")) + "</a>";
  html += "<button type='button' onclick='chooseSettingsFile()'>" + String(tr("Import Settings (JSON)", "設定をインポート（JSON）")) + "</button></div>";
  html += "<p id='import-status' class='meta tool-status'></p></div>";

  html += "<div class='card'><h2>" + String(tr("Display Rotation", "画面の回転")) + "</h2>";
  html += "<p class='meta'>" + String(tr("Current", "現在")) + ": <strong>" + String((int)displayRotation * 90) + "&deg;</strong> (index " + String(displayRotation) + ")</p>";
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

  html += "<form id='settings-form' action='/save' method='POST' onsubmit='saveSettings(event);return false'><div class='card'><h2>" + String(tr("OSC Destination", "OSC送信先")) + "</h2>";
  html += "<label>Host IP</label><input name='host' value='" + htmlEscape(osc_host) + "'>";
  html += "<label>Port</label><input type='number' name='port' value='" + String(osc_port) + "'></div>";

  if (deviceCount == 0)
    html += "<div class='card'><p class='note'>" + String(tr("No device connected.", "デバイスが接続されていません。")) + "</p></div>";

  WEB_PERF_LOG(requestId, requestStart, "HEADER_BUILT", html.length(), 0);
  MEMORY_DEBUG_LOG("WEB_HEADER_BUILT", html.length());

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");
  uint32_t sendStarted = millis();
  server.send(200, "text/html; charset=utf-8", "");
  WEB_PERF_LOG(requestId, requestStart, "HEADER_SENT", 0,
               millis() - sendStarted);
  auto flushHtml = [requestId, requestStart](const char* phase) -> bool {
    if (!server.client().connected()) {
      WEB_PERF_LOG(requestId, requestStart, phase, html.length(), 0);
      html.remove(0);
      return false;
    }
    size_t chunkBytes = html.length();
    uint32_t chunkStarted = millis();
    server.sendContent(html);
    uint32_t chunkMs = millis() - chunkStarted;
    html.remove(0);
    yield();
    WEB_PERF_LOG(requestId, requestStart, phase, chunkBytes, chunkMs);
    MEMORY_DEBUG_LOG(phase, chunkBytes);
    return server.client().connected();
  };
  if (!flushHtml("COMMON_SENT")) return;
  html.reserve(12000);

  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    String idx = String(i);
    bool isSeq = devices[i].mode == MODE_SEQUENCE;
    bool encSeq = devices[i].enc.clickMode == MODE_SEQUENCE;
    bool joySeq = devices[i].joy.clickMode == MODE_SEQUENCE;
    bool ph = isPlaceholderUid(devices[i].uid);

    html += "<div class='card device' data-device-index='" + idx + "' data-collapse-key='" + htmlEscape(devices[i].uid) + "'><div class='device-head'><h2>";
    html += "<button id='collapse-" + idx + "' class='collapse-button' type='button' aria-label='" + String(tr("Collapse or expand device settings", "デバイス設定を折りたたむ／展開する")) + "' aria-expanded='true' onclick=\"toggleDeviceCollapse(" + idx + ",'" + devices[i].uid + "')\">&#9660;</button>";
    html += "<span class='badge badge-type'>" + String(typeToName(devices[i].type)) + "</span>";
    html += " #" + String(devices[i].chainId);
    html += " <span class='badge badge-on'>" + String(tr("Connected", "接続済み")) + "</span></h2>";
    if (!ph && devices[i].type != CHAIN_UNKNOWN_TYPE_CODE) {
      html += "<div class='device-menu-wrap'><button class='more-button' type='button' aria-label='" + String(tr("Device menu", "デバイスメニュー")) + "' aria-expanded='false' onclick='toggleDeviceMenu(event," + idx + ")'>&hellip;</button>";
      html += "<div id='device-menu-" + idx + "' class='device-menu' onclick='event.stopPropagation()'>";
      html += "<button type='button' onclick=\"identifyDevice(" + idx + ",'" + devices[i].uid + "')\">" + String(tr("Identify Device (Orange LED for 10s)", "デバイスを識別（LEDを10秒間オレンジ点灯）")) + "</button>";
      html += "<div class='menu-note'>" + String(tr("Device preset (UID and Device Name are not included)", "デバイスプリセット（UIDとデバイス名は含まれません）")) + "</div>";
      html += "<a href='/export_device_preset?index=" + idx + "' onclick='closeDeviceMenus()'>" + String(tr("Export Preset (JSON)", "プリセットをエクスポート（JSON）")) + "</a>";
      html += "<button type='button' onclick='chooseDevicePreset(" + idx + ")'>" + String(tr("Import Preset (JSON)", "プリセットをインポート（JSON）")) + "</button>";
      html += "<p id='preset-status-" + idx + "' class='preset-status'></p></div></div>";
    }
    html += "</div><div id='device-body-" + idx + "' class='device-body'>";
    html += "<div class='uid'>" + htmlEscape(devices[i].uid) + "</div>";
    if (!ph && devices[i].type != CHAIN_UNKNOWN_TYPE_CODE)
      html += "<input id='preset-file-" + idx + "' type='file' accept='application/json,.json' hidden onchange='importDevicePreset(" + idx + ",this)'>";
    if (ph) html += "<p class='note'>UID取得失敗（仮ID）。設定は保存されません。</p>";
    if (devices[i].type != CHAIN_KEY_TYPE_CODE)
      html += "<label>" + String(tr("Device Name", "デバイス名")) + "</label><input maxlength='64' name='nm_" + idx + "' value='" + htmlEscape(devices[i].displayName) + "' oninput='limitBytes(this,64)'>";
    html += "<input type='hidden' name='uid_" + idx + "' value='" + htmlEscape(devices[i].uid) + "'>";

    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      html += "<div class='key-grid'><div><label>" + String(tr("Device Name", "デバイス名")) + "</label><input maxlength='64' name='nm_" + idx + "' value='" + htmlEscape(devices[i].displayName) + "' oninput='limitBytes(this,64)'></div>";
      html += "<div><label>" + String(tr("Key Mode", "キーモード")) + "</label><select name='md_" + idx + "' onchange=\"toggleClickMode('kpr_" + idx + "','ksq_" + idx + "',this)\">";
      html += "<option value='0'" + String(!isSeq ? " selected" : "") + ">" + String(tr("Press / Release", "押した時／離した時")) + "</option>";
      html += "<option value='1'" + String(isSeq ? " selected" : "") + ">" + String(tr("Sequence", "シーケンス")) + "</option></select></div></div>";
      html += clickMessagesHtml(idx,"k",isSeq,devices[i].pressMessages,devices[i].pressMessageCount,devices[i].releaseMessages,devices[i].releaseMessageCount);
      html += "<div id='ksq_" + idx + "' class='sequence-card' style='display:" + String(isSeq ? "block" : "none") + "'><h3>" + String(tr("Advance the value on each press", "押すたびに値を進める")) + "</h3><p class='note'>" + String(tr("Move from Start by Step and return to Start after End.", "開始値から増減量ずつ進み、終了値を超えると開始値へ戻ります。")) + "</p><div class='seq-grid'>";
      html += addressInputHtml(tr("OSC Address", "OSCアドレス"), "sa_" + idx,
                               devices[i].seq.address, "seq-address");
      html += "<div><label>" + String(tr("Start", "開始値")) + "</label><input type='number' step='any' name='ss_" + idx + "' value='" + String(devices[i].seq.start) + "'></div>";
      html += "<div><label>" + String(tr("End", "終了値")) + "</label><input type='number' step='any' name='se_" + idx + "' value='" + String(devices[i].seq.end) + "'></div>";
      html += "<div><label>" + String(tr("Step", "増減量")) + "</label><input type='number' step='any' name='sp_" + idx + "' value='" + String(devices[i].seq.step) + "'></div>";
      html += "<div><label>" + String(tr("Type", "型")) + "</label>" + typeSelectHtml("st_" + idx, devices[i].seq.valueType) + "</div></div></div>";
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      html += "<div class='enc'><strong>" + String(tr("Encoder Rotation", "エンコーダー回転")) + "</strong>";
      html += addressInputHtml(tr("Rotation Address", "回転OSCアドレス"),
                               "er_" + idx, devices[i].enc.rotAddr);
      html += "<label>" + String(tr("Mode", "モード")) + "</label><select name='ei_" + idx + "' onchange='updateEncoderMode(this)'><option value='0'" + String(!devices[i].enc.sendIncrement ? " selected" : "") + ">" + String(tr("Absolute", "絶対値")) + "</option>";
      html += "<option value='1'" + String(devices[i].enc.sendIncrement ? " selected" : "") + ">" + String(tr("Increment", "増分")) + "</option></select>";
      const String absoluteStyle = devices[i].enc.sendIncrement ? " style='display:none'" : "";
      html += "<label class='encoder-absolute-setting'" + absoluteStyle + ">" + String(tr("Abs In Min", "絶対値入力の最小値")) + "</label><input class='encoder-absolute-setting'" + absoluteStyle + " type='number' step='any' name='e0_" + idx + "' value='" + String(devices[i].enc.absInMin) + "'>";
      html += "<label class='encoder-absolute-setting'" + absoluteStyle + ">" + String(tr("Abs In Max", "絶対値入力の最大値")) + "</label><input class='encoder-absolute-setting'" + absoluteStyle + " type='number' step='any' name='e1_" + idx + "' value='" + String(devices[i].enc.absInMax) + "'>";
      html += "<label>" + String(tr("Inc Scale", "増分倍率")) + "</label><input type='number' step='any' name='es_" + idx + "' value='" + String(devices[i].enc.incScale) + "'>";
      html += "<label>" + String(tr("Out Min", "出力最小値")) + "</label><input type='number' step='any' name='eo_" + idx + "' value='" + String(devices[i].enc.map.outMin) + "'>";
      html += "<label>" + String(tr("Out Max", "出力最大値")) + "</label><input type='number' step='any' name='eO_" + idx + "' value='" + String(devices[i].enc.map.outMax) + "'>";
      html += "<label>" + String(tr("Out Type", "出力の型")) + "</label>" + typeSelectHtml("et_" + idx, devices[i].enc.map.outType) + "</div>";
      html += "<div class='click-section encoder-click'>";
      html += clickModeHtml("em_" + idx, devices[i].enc.clickMode, "epr_" + idx, "esq_" + idx);
      html += clickMessagesHtml(idx,"e",encSeq,devices[i].enc.pressMessages,devices[i].enc.pressMessageCount,devices[i].enc.releaseMessages,devices[i].enc.releaseMessageCount);
      html += "<div id='esq_" + idx + "' class='click-sequence' style='display:" + String(encSeq ? "block" : "none") + "'><strong>" + String(tr("Click Sequence", "クリックシーケンス")) + "</strong>";
      html += addressInputHtml(tr("Address", "OSCアドレス"), "ek_" + idx,
                               devices[i].enc.clickSeq.address);
      html += "<label>" + String(tr("Start", "開始値")) + "</label><input type='number' step='any' name='en_" + idx + "' value='" + String(devices[i].enc.clickSeq.start) + "'>";
      html += "<label>" + String(tr("End", "終了値")) + "</label><input type='number' step='any' name='e2_" + idx + "' value='" + String(devices[i].enc.clickSeq.end) + "'>";
      html += "<label>" + String(tr("Step", "増減量")) + "</label><input type='number' step='any' name='e3_" + idx + "' value='" + String(devices[i].enc.clickSeq.step) + "'>";
      html += "<label>" + String(tr("Type", "型")) + "</label>" + typeSelectHtml("el_" + idx, devices[i].enc.clickSeq.valueType) + "</div></div>";
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      html += "<div class='ang'><strong>" + String(tr("Angle", "角度")) + "</strong>";
      html += addressInputHtml(tr("Address", "OSCアドレス"), "aa_" + idx,
                               devices[i].angle.addr);
      html += "<label>" + String(tr("Resolution", "分解能")) + "</label><select name='a1_" + idx + "'><option value='1'" + String(devices[i].angle.use12bit ? " selected" : "") + ">12-bit</option>";
      html += "<option value='0'" + String(!devices[i].angle.use12bit ? " selected" : "") + ">8-bit</option></select>";
      html += "<label>" + String(tr("Deadband", "不感帯")) + "</label><input type='number' name='ad_" + idx + "' value='" + String(devices[i].angle.deadband) + "'>";
      html += "<label>" + String(tr("Out Min", "出力最小値")) + "</label><input type='number' step='any' name='ao_" + idx + "' value='" + String(devices[i].angle.map.outMin) + "'>";
      html += "<label>" + String(tr("Out Max", "出力最大値")) + "</label><input type='number' step='any' name='aO_" + idx + "' value='" + String(devices[i].angle.map.outMax) + "'>";
      html += "<label>" + String(tr("Out Type", "出力の型")) + "</label>" + typeSelectHtml("at_" + idx, devices[i].angle.map.outType) + "</div>";
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      html += "<div class='joy'><strong>" + String(tr("Joystick XY", "ジョイスティック XY")) + "</strong>";
      html += addressInputHtml(tr("X Address", "X軸OSCアドレス"), "jx_" + idx,
                               devices[i].joy.xAddr);
      html += "<div class='chk'><input type='checkbox' name='jix_" + idx + "' value='1'" + String(devices[i].joy.invertX ? " checked" : "") + ">";
      html += "<span>" + String(tr("Invert X (+/-)", "X軸反転 (+/-)")) + "</span></div>";
      html += addressInputHtml(tr("Y Address", "Y軸OSCアドレス"), "jy_" + idx,
                               devices[i].joy.yAddr);
      html += "<div class='chk'><input type='checkbox' name='jiy_" + idx + "' value='1'" + String(devices[i].joy.invertY ? " checked" : "") + ">";
      html += "<span>" + String(tr("Invert Y (+/-)", "Y軸反転 (+/-)")) + "</span></div>";
      html += "<label>" + String(tr("Deadband", "不感帯")) + "</label><input type='number' name='jd_" + idx + "' value='" + String(devices[i].joy.deadband) + "'>";
      html += "<label>" + String(tr("Out Min", "出力最小値")) + "</label><input type='number' step='any' name='jo_" + idx + "' value='" + String(devices[i].joy.map.outMin) + "'>";
      html += "<label>" + String(tr("Out Max", "出力最大値")) + "</label><input type='number' step='any' name='jO_" + idx + "' value='" + String(devices[i].joy.map.outMax) + "'>";
      html += "<label>" + String(tr("Out Type", "出力の型")) + "</label>" + typeSelectHtml("jt_" + idx, devices[i].joy.map.outType) + "</div>";
      html += "<div class='click-section joystick-click'>";
      html += clickModeHtml("jm_" + idx, devices[i].joy.clickMode, "jpr_" + idx, "jsq_" + idx);
      html += clickMessagesHtml(idx,"j",joySeq,devices[i].joy.pressMessages,devices[i].joy.pressMessageCount,devices[i].joy.releaseMessages,devices[i].joy.releaseMessageCount);
      html += "<div id='jsq_" + idx + "' class='click-sequence' style='display:" + String(joySeq ? "block" : "none") + "'><strong>" + String(tr("Click Sequence", "クリックシーケンス")) + "</strong>";
      html += addressInputHtml(tr("Address", "OSCアドレス"), "jk_" + idx,
                               devices[i].joy.clickSeq.address);
      html += "<label>" + String(tr("Start", "開始値")) + "</label><input type='number' step='any' name='jn_" + idx + "' value='" + String(devices[i].joy.clickSeq.start) + "'>";
      html += "<label>" + String(tr("End", "終了値")) + "</label><input type='number' step='any' name='j2_" + idx + "' value='" + String(devices[i].joy.clickSeq.end) + "'>";
      html += "<label>" + String(tr("Step", "増減量")) + "</label><input type='number' step='any' name='j3_" + idx + "' value='" + String(devices[i].joy.clickSeq.step) + "'>";
      html += "<label>" + String(tr("Type", "型")) + "</label>" + typeSelectHtml("jl_" + idx, devices[i].joy.clickSeq.valueType) + "</div></div>";
    } else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
      html += "<div class='ang'><strong>" + String(tr("ToF Distance (mm)", "ToF距離 (mm)")) + "</strong>";
      html += addressInputHtml(tr("Address", "OSCアドレス"), "fa_" + idx,
                               devices[i].tof.addr);
      html += "<label>" + String(tr("Deadband (mm)", "不感帯 (mm)")) + "</label><input type='number' name='fd_" + idx + "' value='" + String(devices[i].tof.deadband) + "'>";
      html += "<label>" + String(tr("Maximum Distance (mm)", "最大距離 (mm)")) + "</label><input type='number' min='31' max='2000' name='fm_" + idx + "' value='" + String(devices[i].tof.maxDistanceMm) + "'>";
      html += "<label>" + String(tr("Output Direction", "出力方向")) + "</label><select name='fi_" + idx + "'>";
      html += "<option value='0'" + String(!devices[i].tof.nearValueHigh ? " selected" : "") + ">" + String(tr("Near → Out Min / Far → Out Max", "近い → 出力最小値／遠い → 出力最大値")) + "</option>";
      html += "<option value='1'" + String(devices[i].tof.nearValueHigh ? " selected" : "") + ">" + String(tr("Near → Out Max / Far → Out Min", "近い → 出力最大値／遠い → 出力最小値")) + "</option></select>";
      html += "<label>" + String(tr("Out Min", "出力最小値")) + "</label><input type='number' step='any' name='fo_" + idx + "' value='" + String(devices[i].tof.map.outMin) + "'>";
      html += "<label>" + String(tr("Out Max", "出力最大値")) + "</label><input type='number' step='any' name='fO_" + idx + "' value='" + String(devices[i].tof.map.outMax) + "'>";
      html += "<label>" + String(tr("Out Type", "出力の型")) + "</label>" + numericTypeSelectHtml("ft_" + idx, devices[i].tof.map.outType);
      html += "<p class='note'>" + String(tr("Active range: 30 mm to less than Maximum Distance. OSC transmission stops outside this range.", "有効範囲は30 mm以上、最大距離未満です。範囲外ではOSC送信を停止します。")) + "</p></div>";
    } else {
      html += "<p class='note'>Type code: " + String((int)devices[i].type) + "</p>";
    }
    html += "</div></div>";
    if (!flushHtml("DEVICE_SENT")) return;
  }
  html += "<div class='save-bar'><span id='dirty-status' class='dirty-status' hidden>" + String(tr("Unsaved changes", "未保存の変更")) + "</span><button type='submit'>" + String(tr("Save All Settings", "すべての設定を保存")) + "</button></div></form>";

  html += "<div class='card saved-settings'><h2>" + String(tr("Saved Device Settings", "保存済みデバイス設定")) + "</h2>";
  html += "<p class='note'>" + String(tr("Only devices whose settings have been saved are shown.", "設定を保存したデバイスのみ表示します。")) + "</p></div>";
  if (!flushHtml("SAVED_HEADER_SENT")) return;
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used || isPlaceholderUid(knownDevices[i].uid)) continue;
    bool con = isUidConnected(knownDevices[i].uid);
    String label = knownDevices[i].displayName.length()
                       ? knownDevices[i].displayName
                       : knownDevices[i].uid.substring(max(0, (int)knownDevices[i].uid.length() - 8));
    html += "<div class='card saved-device-card'><h2>";
    html += "<span class='badge badge-type'>" + String(typeToName(knownDevices[i].type)) + "</span> ";
    html += htmlEscape(label) + " ";
    html += con ? String("<span class='badge badge-on'>") + tr("Connected", "接続済み") + "</span>" : String("<span class='badge badge-off'>") + tr("Off", "未接続") + "</span>";
    html += "</h2>";
    html += "<p class='meta'>Type: <strong>" + String(typeToName(knownDevices[i].type)) + "</strong></p>";
    html += "<div class='uid'>" + htmlEscape(knownDevices[i].uid) + "</div>";
    html += "<form action='/delete_device' method='POST' onsubmit='deleteSavedDevice(event,this);return false'>";
    html += "<input type='hidden' name='uid' value='" + htmlEscape(knownDevices[i].uid) + "'>";
    html += "<button class='btn-warning' type='submit'>" + String(tr("Delete Settings", "設定を削除")) + "</button></form></div>";
    if (!flushHtml("SAVED_DEVICE_SENT")) return;
  }
  html += "</body></html>";
  if (!flushHtml("FOOTER_SENT")) return;
  if (server.client().connected()) {
    uint32_t finalStarted = millis();
    server.sendContent("");
    WEB_PERF_LOG(requestId, requestStart, "END", 0,
                 millis() - finalStarted);
    MEMORY_DEBUG_LOG("WEB_ROOT_END", 0);
  } else {
    WEB_PERF_LOG(requestId, requestStart, "ABORT_END", 0, 0);
  }
}

// ---------------------------------------------------------------------------
// Save form
// ---------------------------------------------------------------------------
void handleSave() {
  MEMORY_DEBUG_LOG("SAVE_BEGIN", 0);
#if M5CHAINOSC_STORAGE_DEBUG
  Serial.printf("[M5OSC][WEB] SAVE request args=%d devices=%d\n", server.args(), deviceCount);
#endif
  if (server.hasArg("host")) osc_host = server.arg("host");
  if (server.hasArg("port")) osc_port = server.arg("port").toInt();
  prefs.begin("osc", false);
  prefs.putString("host", osc_host);
  prefs.putInt("port", osc_port);
  prefs.end();

  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    String idx = String(i);
    if (!server.hasArg("uid_" + idx)) continue;
    if (server.hasArg("nm_" + idx)) {
      devices[i].displayName = server.arg("nm_" + idx);
      devices[i].displayName.trim();
      if (devices[i].displayName.length() > MAX_DEVICE_NAME_BYTES) {
        sendUiResult(400, tr("Save error", "保存エラー"), tr("Device Name is too long. Reduce it and try again.", "デバイス名が長すぎます。短くしてからもう一度お試しください。"));
        return;
      }
    }

    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      ChainDevice candidate = devices[i];
      if (server.hasArg("md_" + idx)) candidate.mode = server.arg("md_" + idx).toInt() == MODE_SEQUENCE ? MODE_SEQUENCE : MODE_PRESS_RELEASE;
      int pc = constrain(server.arg("kpc_" + idx).toInt(), 0, MAX_KEY_OSC_MESSAGES);
      int rc = constrain(server.arg("krc_" + idx).toInt(), 0, MAX_KEY_OSC_MESSAGES);
#if M5CHAINOSC_STORAGE_DEBUG
      Serial.printf("[M5OSC][WEB] KEY parsed idx=%d uid=%s press=%d release=%d\n", i, devices[i].uid.c_str(), pc, rc);
#endif
      if (pc + rc > MAX_KEY_OSC_MESSAGES) {
        sendUiResult(400, tr("Save error", "保存エラー"), tr("Press and Release messages must total 8 or fewer.", "PressとReleaseのメッセージは合計8件以内にしてください。"));
        return;
      }
      candidate.pressMessageCount = (uint8_t)pc;
      candidate.releaseMessageCount = (uint8_t)rc;
      String validationError;
      for (int m = 0; m < pc; m++) {
        candidate.pressMessages[m].address = server.arg("kpa_" + idx + "_" + String(m));
        candidate.pressMessages[m].valueStr = server.arg("kpv_" + idx + "_" + String(m));
        candidate.pressMessages[m].valueType = (ValueType)constrain(server.arg("kpt_" + idx + "_" + String(m)).toInt(), (int)TYPE_FLOAT, (int)TYPE_STRING);
        if (!validOscMessage(candidate.pressMessages[m], validationError)) {
          sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Press message ", "Pressメッセージ ")) + String(m + 1) + ": " + validationError); return;
        }
      }
      for (int m = 0; m < rc; m++) {
        candidate.releaseMessages[m].address = server.arg("kra_" + idx + "_" + String(m));
        candidate.releaseMessages[m].valueStr = server.arg("krv_" + idx + "_" + String(m));
        candidate.releaseMessages[m].valueType = (ValueType)constrain(server.arg("krt_" + idx + "_" + String(m)).toInt(), (int)TYPE_FLOAT, (int)TYPE_STRING);
        if (!validOscMessage(candidate.releaseMessages[m], validationError)) {
          sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Release message ", "Releaseメッセージ ")) + String(m + 1) + ": " + validationError); return;
        }
      }
      if (pc > 0) candidate.press = candidate.pressMessages[0];
      if (rc > 0) candidate.release = candidate.releaseMessages[0];
      if (server.hasArg("sa_" + idx)) candidate.seq.address = server.arg("sa_" + idx);
      candidate.seq.address.trim();
      if (!validOscAddressText(candidate.seq.address, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String("Sequence: ") + validationError); return;
      }
      if (server.hasArg("ss_" + idx)) candidate.seq.start = server.arg("ss_" + idx).toFloat();
      if (server.hasArg("se_" + idx)) candidate.seq.end = server.arg("se_" + idx).toFloat();
      if (server.hasArg("sp_" + idx)) candidate.seq.step = server.arg("sp_" + idx).toFloat();
      if (server.hasArg("st_" + idx)) candidate.seq.valueType = (ValueType)constrain(server.arg("st_" + idx).toInt(), (int)TYPE_FLOAT, (int)TYPE_STRING);
      normalizeSequence(candidate.seq);
      if (deviceConfigStorageBytes(candidate) > MAX_DEVICE_CONFIG_BYTES) {
        sendUiResult(413, tr("Save error", "保存エラー"), tr("The device configuration is too large. Delete messages or shorten Address and Value fields.", "デバイス設定の容量が大きすぎます。メッセージを削除するか、AddressとValueを短くしてください。")); return;
      }
      devices[i] = candidate;
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      EncoderOscConfig candidate = devices[i].enc;
      if (server.hasArg("er_" + idx)) candidate.rotAddr = server.arg("er_" + idx);
      candidate.rotAddr.trim();
      String validationError;
      if (!validOscAddressText(candidate.rotAddr, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Encoder Rotation Address: ", "エンコーダー回転OSCアドレス: ")) + validationError); return;
      }
      if (server.hasArg("ei_" + idx)) candidate.sendIncrement = server.arg("ei_" + idx).toInt() != 0;
      if (server.hasArg("e0_" + idx)) candidate.absInMin = server.arg("e0_" + idx).toFloat();
      if (server.hasArg("e1_" + idx)) candidate.absInMax = server.arg("e1_" + idx).toFloat();
      if (server.hasArg("es_" + idx)) candidate.incScale = server.arg("es_" + idx).toFloat();
      if (server.hasArg("eo_" + idx)) candidate.map.outMin = server.arg("eo_" + idx).toFloat();
      if (server.hasArg("eO_" + idx)) candidate.map.outMax = server.arg("eO_" + idx).toFloat();
      if (server.hasArg("et_" + idx)) candidate.map.outType = (ValueType)server.arg("et_" + idx).toInt();
      if (server.hasArg("em_" + idx)) candidate.clickMode = (KeyMode)server.arg("em_" + idx).toInt();
      if (!parseMessageList(idx,"e",candidate.pressMessages,candidate.pressMessageCount,candidate.releaseMessages,candidate.releaseMessageCount,validationError)) { sendUiResult(400,tr("Save error","保存エラー"),validationError); return; }
      if (candidate.pressMessageCount) candidate.press = candidate.pressMessages[0];
      if (candidate.releaseMessageCount) candidate.release = candidate.releaseMessages[0];
      if (server.hasArg("ek_" + idx)) candidate.clickSeq.address = server.arg("ek_" + idx);
      candidate.clickSeq.address.trim();
      if (!validOscAddressText(candidate.clickSeq.address, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Encoder Click Sequence Address: ", "エンコーダークリックシーケンスOSCアドレス: ")) + validationError); return;
      }
      if (server.hasArg("en_" + idx)) candidate.clickSeq.start = server.arg("en_" + idx).toFloat();
      if (server.hasArg("e2_" + idx)) candidate.clickSeq.end = server.arg("e2_" + idx).toFloat();
      if (server.hasArg("e3_" + idx)) candidate.clickSeq.step = server.arg("e3_" + idx).toFloat();
      if (server.hasArg("el_" + idx)) candidate.clickSeq.valueType = (ValueType)server.arg("el_" + idx).toInt();
      normalizeSequence(candidate.clickSeq);
      devices[i].enc = candidate;
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      AngleOscConfig candidate = devices[i].angle;
      if (server.hasArg("aa_" + idx)) candidate.addr = server.arg("aa_" + idx);
      candidate.addr.trim();
      String validationError;
      if (!validOscAddressText(candidate.addr, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Angle Address: ", "Angle OSCアドレス: ")) + validationError); return;
      }
      if (server.hasArg("a1_" + idx)) candidate.use12bit = server.arg("a1_" + idx).toInt() != 0;
      if (server.hasArg("ad_" + idx)) candidate.deadband = server.arg("ad_" + idx).toInt();
      if (server.hasArg("ao_" + idx)) candidate.map.outMin = server.arg("ao_" + idx).toFloat();
      if (server.hasArg("aO_" + idx)) candidate.map.outMax = server.arg("aO_" + idx).toFloat();
      if (server.hasArg("at_" + idx)) candidate.map.outType = (ValueType)server.arg("at_" + idx).toInt();
      devices[i].angle = candidate;
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      JoystickOscConfig candidate = devices[i].joy;
      if (server.hasArg("jx_" + idx)) candidate.xAddr = server.arg("jx_" + idx);
      if (server.hasArg("jy_" + idx)) candidate.yAddr = server.arg("jy_" + idx);
      candidate.xAddr.trim(); candidate.yAddr.trim();
      String validationError;
      if (!validOscAddressText(candidate.xAddr, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Joystick X Address: ", "Joystick X軸OSCアドレス: ")) + validationError); return;
      }
      if (!validOscAddressText(candidate.yAddr, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Joystick Y Address: ", "Joystick Y軸OSCアドレス: ")) + validationError); return;
      }
      if (server.hasArg("jd_" + idx)) candidate.deadband = server.arg("jd_" + idx).toInt();
      candidate.invertX = server.hasArg("jix_" + idx);
      candidate.invertY = server.hasArg("jiy_" + idx);
      if (server.hasArg("jo_" + idx)) candidate.map.outMin = server.arg("jo_" + idx).toFloat();
      if (server.hasArg("jO_" + idx)) candidate.map.outMax = server.arg("jO_" + idx).toFloat();
      if (server.hasArg("jt_" + idx)) candidate.map.outType = (ValueType)server.arg("jt_" + idx).toInt();
      if (server.hasArg("jm_" + idx)) candidate.clickMode = (KeyMode)server.arg("jm_" + idx).toInt();
      if (!parseMessageList(idx,"j",candidate.pressMessages,candidate.pressMessageCount,candidate.releaseMessages,candidate.releaseMessageCount,validationError)) { sendUiResult(400,tr("Save error","保存エラー"),validationError); return; }
      if (candidate.pressMessageCount) candidate.press = candidate.pressMessages[0];
      if (candidate.releaseMessageCount) candidate.release = candidate.releaseMessages[0];
      if (server.hasArg("jk_" + idx)) candidate.clickSeq.address = server.arg("jk_" + idx);
      candidate.clickSeq.address.trim();
      if (!validOscAddressText(candidate.clickSeq.address, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String(tr("Joystick Click Sequence Address: ", "JoystickクリックシーケンスOSCアドレス: ")) + validationError); return;
      }
      if (server.hasArg("jn_" + idx)) candidate.clickSeq.start = server.arg("jn_" + idx).toFloat();
      if (server.hasArg("j2_" + idx)) candidate.clickSeq.end = server.arg("j2_" + idx).toFloat();
      if (server.hasArg("j3_" + idx)) candidate.clickSeq.step = server.arg("j3_" + idx).toFloat();
      if (server.hasArg("jl_" + idx)) candidate.clickSeq.valueType = (ValueType)server.arg("jl_" + idx).toInt();
      normalizeSequence(candidate.clickSeq);
      devices[i].joy = candidate;
    } else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
      TofOscConfig candidate = devices[i].tof;
      if (server.hasArg("fa_" + idx)) candidate.addr = server.arg("fa_" + idx);
      candidate.addr.trim();
      String validationError;
      if (!validOscAddressText(candidate.addr, validationError)) {
        sendUiResult(400, tr("Save error", "保存エラー"), String("ToF Address: ") + validationError); return;
      }
      if (server.hasArg("fd_" + idx)) candidate.deadband = server.arg("fd_" + idx).toInt();
      if (server.hasArg("fm_" + idx)) candidate.maxDistanceMm = server.arg("fm_" + idx).toInt();
      candidate.nearValueHigh = server.hasArg("fi_" + idx) && server.arg("fi_" + idx).toInt() != 0;
      if (candidate.deadband < 1 || candidate.deadband > 2000 ||
          candidate.maxDistanceMm < 31 || candidate.maxDistanceMm > 2000) {
        sendUiResult(400, tr("Save error", "保存エラー"), tr("ToF Maximum Distance must be 31–2000 mm and Deadband must be 1–2000 mm.", "ToFの最大距離は31～2000 mm、不感帯は1～2000 mmに設定してください。")); return;
      }
      if (server.hasArg("fo_" + idx)) candidate.map.outMin = server.arg("fo_" + idx).toFloat();
      if (server.hasArg("fO_" + idx)) candidate.map.outMax = server.arg("fO_" + idx).toFloat();
      if (server.hasArg("ft_" + idx)) {
        int t = server.arg("ft_" + idx).toInt();
        candidate.map.outType = (t == TYPE_INT) ? TYPE_INT : TYPE_FLOAT;
      }
      if (!isfinite(candidate.map.outMin) || !isfinite(candidate.map.outMax)) {
        sendUiResult(400, tr("Save error", "保存エラー"), tr("ToF output range is invalid.", "ToFの出力範囲が正しくありません。")); return;
      }
      candidate.map.inMin = 30;
      candidate.map.inMax = candidate.maxDistanceMm;
      devices[i].tof = candidate;
      devices[i].tofInited = false;
      devices[i].lastTofMm = -1;
    }

    if (!isPlaceholderUid(devices[i].uid) && !saveDeviceSettings(devices[i])) {
      sendUiResult(507, tr("Save error", "保存エラー"), tr("The device configuration could not be written to storage. Delete messages or shorten Address and Value fields, then try again.", "デバイス設定をストレージへ書き込めませんでした。メッセージを削除するか、AddressとValueを短くしてからもう一度お試しください。"));
      return;
    }
  }
  sendUiResult(200, tr("Saved!", "保存しました！"), tr("All settings were saved.", "すべての設定を保存しました。"));
  MEMORY_DEBUG_LOG("SAVE_END", 0);
}

void handleSetRotation() {
  if (server.hasArg("rotation")) {
    int r = server.arg("rotation").toInt();
    if (r >= 0 && r <= 3) {
      displayRotation = r;
      saveDisplayRotation();
      applyDisplayRotation();
      if (!isAPMode && !resetInProgress) drawMainScreen();
    }
  }
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleDeleteWifi() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  sendUiResult(200, tr("WiFi deleted", "Wi-Fi設定を削除しました"), tr("The device will restart.", "デバイスを再起動します。"), false);
  delay(1200);
  ESP.restart();
}

void handleDeleteDevice() {
  if (!server.hasArg("uid")) {
    server.send(400, "text/plain", "uid");
    return;
  }
  deleteDeviceSettingsByUid(server.arg("uid"));
  sendUiResult(200, tr("Deleted", "削除しました"), tr("The saved device settings were deleted.", "保存済みデバイス設定を削除しました。"));
}

static int requestedActiveDeviceIndex() {
  if (!server.hasArg("index")) return -1;
  String value = server.arg("index");
  if (!value.length()) return -1;
  for (size_t i = 0; i < value.length(); i++)
    if (!isdigit((unsigned char)value[i])) return -1;
  int index = value.toInt();
  if (index < 0 || index >= deviceCount || !devices[index].active ||
      !devices[index].uid.length() || isPlaceholderUid(devices[index].uid)) return -1;
  return index;
}

void handleIdentifyDevice() {
  int index = requestedActiveDeviceIndex();
  if (index < 0 || !server.hasArg("uid") || server.arg("uid") != devices[index].uid) {
    server.send(404, "text/plain; charset=utf-8", tr("The selected connected device was not found.", "選択した接続済みデバイスが見つかりません。"));
    return;
  }
  if (!identifyChainDevice(index, server.arg("uid"))) {
    server.send(502, "text/plain; charset=utf-8", tr("The device LED could not be changed.", "デバイスのLEDを変更できませんでした。"));
    return;
  }
  server.send(200, "text/plain; charset=utf-8", tr("Orange LED active for 10 seconds.", "LEDを10秒間オレンジ色に点灯します。"));
}

void handleExportDevicePreset() {
  int index = requestedActiveDeviceIndex();
  if (index < 0) {
    server.send(404, "text/plain; charset=utf-8", tr("The selected connected device was not found.", "選択した接続済みデバイスが見つかりません。"));
    return;
  }

  String typeName = String(typeToName(devices[index].type));
  typeName.replace(" ", "-");
  server.sendHeader("Content-Disposition", "attachment; filename=\"M5ChainOSC-" + typeName + "-preset.json\"");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", deviceJson(devices[index], false));
}

void handleImportDevicePreset() {
  int index = requestedActiveDeviceIndex();
  if (index < 0) {
    server.send(404, "text/plain; charset=utf-8", tr("The selected connected device was not found.", "選択した接続済みデバイスが見つかりません。"));
    return;
  }

  String body = server.arg("plain");
  MEMORY_DEBUG_LOG("PRESET_BODY_RECEIVED", body.length());
  if (!body.length()) {
    server.send(400, "text/plain; charset=utf-8", tr("Preset file is empty.", "プリセットファイルが空です。"));
    return;
  }
  if (body.length() > 16384) {
    server.send(413, "text/plain; charset=utf-8", tr("Preset file exceeds 16 KiB.", "プリセットファイルが16 KiBを超えています。"));
    return;
  }

  DynamicJsonDocument document(24576);
  MEMORY_DEBUG_JSON("PRESET_DOCUMENT_CREATED", body.length(), document);
  DeserializationError parseError = deserializeJson(document, body);
  MEMORY_DEBUG_JSON("PRESET_JSON_PARSED", body.length(), document);
  body = "";
  MEMORY_DEBUG_JSON("PRESET_BODY_RELEASED", 0, document);
  if (parseError) {
    server.send(400, "text/plain; charset=utf-8", String(tr("Invalid JSON: ", "JSONが正しくありません: ")) + parseError.c_str());
    return;
  }

  JsonObject root = document.as<JsonObject>();
  if (root.isNull() || !root["format"].is<const char*>() ||
      String(root["format"].as<const char*>()) != "M5ChainOSC-device-preset") {
    server.send(400, "text/plain; charset=utf-8", tr("This is not an M5ChainOSC device preset.", "M5ChainOSCのデバイスプリセットではありません。"));
    return;
  }
  if (!root["schemaVersion"].is<int>() || root["schemaVersion"].as<int>() != 1) {
    server.send(400, "text/plain; charset=utf-8", tr("Unsupported or missing preset schemaVersion.", "プリセットのschemaVersionがないか、対応していません。"));
    return;
  }
  if (!root["deviceType"].is<int>() ||
      root["deviceType"].as<int>() != (int)devices[index].type) {
    server.send(400, "text/plain; charset=utf-8",
                "Device type mismatch. Select a preset for " + String(typeToName(devices[index].type)) + ".");
    return;
  }

  // A preset intentionally has no identity. Bind it only to the device card
  // selected by the user, while preserving that device's local name.
  root["uid"] = devices[index].uid;
  root["displayName"] = devices[index].displayName;

  ChainDevice* candidate = new (std::nothrow) ChainDevice();
  MEMORY_DEBUG_JSON("PRESET_CANDIDATE_CREATED", 0, document);
  if (!candidate) {
    server.send(503, "text/plain; charset=utf-8", tr("Not enough memory to validate the preset.", "プリセットを検証するためのメモリが不足しています。"));
    return;
  }
  String validationError;
  JsonObjectConst presetObject = root;
  if (!deviceFromJson(presetObject, *candidate, validationError)) {
    delete candidate;
    server.send(400, "text/plain; charset=utf-8", String(tr("Invalid preset: ", "プリセットが正しくありません: ")) + validationError);
    return;
  }
  if (!saveDeviceSettings(*candidate)) {
    delete candidate;
    server.send(507, "text/plain; charset=utf-8", tr("The preset could not be written to storage.", "プリセットをストレージへ書き込めませんでした。"));
    return;
  }
  delete candidate;

  loadDeviceSettings(devices[index]);
  MEMORY_DEBUG_JSON("PRESET_END", 0, document);
  server.send(200, "text/plain; charset=utf-8",
              String(tr("Preset imported for ", "プリセットをインポートしました: ")) + String(typeToName(devices[index].type)) + ".");
}

void handleExportSettings() {
  MEMORY_DEBUG_LOG("EXPORT_BEGIN", 0);
  server.sendHeader("Content-Disposition", "attachment; filename=\"M5ChainOSC-settings-v" + String(SETTINGS_SCHEMA_VERSION) + ".json\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json; charset=utf-8", "");

  String header = String("{\"format\":") + jsonString(String(SETTINGS_FORMAT_NAME)) +
                  ",\"schemaVersion\":" + String(SETTINGS_SCHEMA_VERSION) +
                  ",\"wifiCredentialsIncluded\":false" +
                  ",\"global\":{\"oscHost\":" + jsonString(osc_host) +
                  ",\"oscPort\":" + String(osc_port) +
                  ",\"displayRotation\":" + String(displayRotation) +
                  ",\"uiLanguage\":" + jsonString(isJapaneseUi() ? "ja" : "en") + "},\"devices\":[";
  server.sendContent(header);

  bool first = true;
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used || !knownDevices[i].uid.length() || isPlaceholderUid(knownDevices[i].uid)) continue;
    ChainDevice saved;
    saved.uid = knownDevices[i].uid;
    saved.uidShort = saved.uid.substring(max(0, (int)saved.uid.length() - 8));
    saved.type = knownDevices[i].type;
    loadDeviceSettings(saved);
    String chunk = first ? "" : ",";
    chunk += deviceJson(saved);
    server.sendContent(chunk);
    MEMORY_DEBUG_LOG("EXPORT_DEVICE_SENT", chunk.length());
    first = false;
  }
  server.sendContent("]}");
  server.sendContent("");
  MEMORY_DEBUG_LOG("EXPORT_END", 0);
}

void handleImportSettings() {
  String body = server.arg("plain");
  MEMORY_DEBUG_LOG("IMPORT_BODY_RECEIVED", body.length());
  if (!body.length()) { server.send(400, "text/plain; charset=utf-8", tr("Import file is empty.", "インポートファイルが空です。")); return; }
  if (body.length() > 49152) { server.send(413, "text/plain; charset=utf-8", tr("Import file exceeds 48 KiB.", "インポートファイルが48 KiBを超えています。")); return; }

  DynamicJsonDocument document(65536);
  MEMORY_DEBUG_JSON("IMPORT_DOCUMENT_CREATED", body.length(), document);
  DeserializationError parseError = deserializeJson(document, body);
  MEMORY_DEBUG_JSON("IMPORT_JSON_PARSED", body.length(), document);
  body = "";
  MEMORY_DEBUG_JSON("IMPORT_BODY_RELEASED", 0, document);
  if (parseError) {
    server.send(400, "text/plain; charset=utf-8", String(tr("Invalid JSON: ", "JSONが正しくありません: ")) + parseError.c_str()); return;
  }
  JsonObjectConst root = document.as<JsonObjectConst>();
  if (!root["format"].is<const char*>() || String(root["format"].as<const char*>()) != SETTINGS_FORMAT_NAME) {
    server.send(400, "text/plain; charset=utf-8", tr("This is not an M5ChainOSC settings file.", "M5ChainOSCの全体設定ファイルではありません。")); return;
  }
  if (!root["schemaVersion"].is<int>()) {
    server.send(400, "text/plain; charset=utf-8", tr("schemaVersion is missing.", "schemaVersionがありません。")); return;
  }
  int version = root["schemaVersion"].as<int>();
  if (version != SETTINGS_SCHEMA_VERSION) {
    server.send(400, "text/plain; charset=utf-8", String(tr("Unsupported schemaVersion: ", "対応していないschemaVersionです: ")) + String(version)); return;
  }
  JsonObjectConst global = root["global"].as<JsonObjectConst>();
  JsonArrayConst importedDevices = root["devices"].as<JsonArrayConst>();
  if (global.isNull() || importedDevices.isNull() || importedDevices.size() > MAX_KNOWN ||
      !global["oscHost"].is<const char*>() || !global["oscPort"].is<int>() || !global["displayRotation"].is<int>()) {
    server.send(400, "text/plain; charset=utf-8", tr("Global settings or device list is invalid.", "共通設定またはデバイス一覧が正しくありません。")); return;
  }
  String importedHost = global["oscHost"].as<const char*>();
  int importedPort = global["oscPort"].as<int>();
  int importedRotation = global["displayRotation"].as<int>();
  UiLanguage importedLanguage = uiLanguage;
  if (global["uiLanguage"].is<const char*>()) {
    String language = global["uiLanguage"].as<const char*>();
    if (language != "en" && language != "ja") {
      server.send(400, "text/plain; charset=utf-8", tr("uiLanguage must be en or ja.", "uiLanguageはenまたはjaで指定してください。")); return;
    }
    importedLanguage = language == "ja" ? UI_LANG_JAPANESE : UI_LANG_ENGLISH;
  }
  if (!importedHost.length() || importedHost.length() > 253 || importedPort < 1 || importedPort > 65535 || importedRotation < 0 || importedRotation > 3) {
    server.send(400, "text/plain; charset=utf-8", tr("Global OSC or display setting is out of range.", "共通OSC設定または画面設定が範囲外です。")); return;
  }

  ChainDevice* candidate = new (std::nothrow) ChainDevice();
  MEMORY_DEBUG_JSON("IMPORT_CANDIDATE_CREATED", 0, document);
  if (!candidate) { server.send(503, "text/plain; charset=utf-8", tr("Not enough memory to validate settings.", "設定を検証するためのメモリが不足しています。")); return; }
  String validationError;
  int deviceNumber = 0;
  for (size_t deviceIndex = 0; deviceIndex < importedDevices.size(); deviceIndex++) {
    JsonObjectConst object = importedDevices[deviceIndex].as<JsonObjectConst>();
    deviceNumber = (int)deviceIndex + 1;
    if (!deviceFromJson(object, *candidate, validationError)) {
      delete candidate;
      server.send(400, "text/plain; charset=utf-8", String(tr("Device ", "デバイス ")) + String(deviceNumber) + ": " + validationError); return;
    }
    for (size_t previousIndex = 0; previousIndex < deviceIndex; previousIndex++) {
      JsonObjectConst previous = importedDevices[previousIndex].as<JsonObjectConst>();
      if (previous["uid"].is<const char*>() && String(previous["uid"].as<const char*>()) == candidate->uid) {
        String duplicateUid = candidate->uid;
        delete candidate;
        server.send(400, "text/plain; charset=utf-8", String(tr("Duplicate device UID: ", "デバイスUIDが重複しています: ")) + duplicateUid); return;
      }
    }
  }

  deviceNumber = 0;
  for (JsonObjectConst object : importedDevices) {
    deviceNumber++;
    if (!deviceFromJson(object, *candidate, validationError) || !saveDeviceSettings(*candidate)) {
      delete candidate;
      server.send(507, "text/plain; charset=utf-8", String(tr("Storage write failed at device ", "デバイス設定のストレージ書き込みに失敗しました: ")) + String(deviceNumber) + "."); return;
    }
    MEMORY_DEBUG_JSON("IMPORT_DEVICE_SAVED", 0, document);
  }
  delete candidate;
  MEMORY_DEBUG_JSON("IMPORT_CANDIDATE_RELEASED", 0, document);

  osc_host = importedHost;
  osc_port = importedPort;
  displayRotation = (uint8_t)importedRotation;
  uiLanguage = importedLanguage;
  prefs.begin("osc", false); prefs.putString("host", osc_host); prefs.putInt("port", osc_port); prefs.end();
  saveDisplayRotation();
  saveUiLanguage();
  applyDisplayRotation();
  refreshChainDevices(true);
  if (!isAPMode && !resetInProgress) drawMainScreen();
  MEMORY_DEBUG_JSON("IMPORT_END", 0, document);
  server.send(200, "text/plain; charset=utf-8", String(tr("Import completed. ", "インポートが完了しました。")) + String(importedDevices.size()) + tr(" device(s) restored.", "件のデバイス設定を復元しました。"));
}
