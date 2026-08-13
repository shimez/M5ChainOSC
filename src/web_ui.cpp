#include "web_ui.h"
#include "globals.h"
#include "storage.h"
#include "display.h"
#include "chain_devices.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <new>
#include <stdlib.h>
#include <ArduinoJson.h>

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
  String s = "<div class='mode-box'><label>Click Mode</label>";
  s += "<select name='" + name + "' onchange=\"toggleClickMode('" + prId + "','" + sqId + "',this)\">";
  s += "<option value='0'" + String(cur == MODE_PRESS_RELEASE ? " selected" : "") + ">Press / Release</option>";
  s += "<option value='1'" + String(cur == MODE_SEQUENCE ? " selected" : "") + ">Sequence (press only)</option>";
  s += "</select></div>";
  return s;
}

static String messageRowHtml(const String& group, const String& prefix, const char* eventName, int order, const OSCMessage& m) {
  String idx = group.substring(prefix.length());
  String p = prefix + (String(eventName) == "press" ? "p" : "r");
  String row = "<div class='osc-row' data-group='" + group + "' data-prefix='" + prefix + "' data-event='" + eventName + "'>";
  row += "<div class='order'><button type='button' class='mv' onclick='moveMsg(this,-1)'>&uarr;</button><button type='button' class='mv' onclick='moveMsg(this,1)'>&darr;</button></div>";
  row += "<div class='field'><label>OSC Address</label><input class='msg-address' maxlength='192' name='" + p + "a_" + idx + "_" + String(order) + "' value='" + htmlEscape(m.address) + "' oninput='limitAndValidate(this,192)'><small><span class='err'></span><span class='bytes'></span></small></div>";
  row += "<div class='field'><label>Type</label>" + typeSelectHtml(p + "t_" + idx + "_" + String(order), m.valueType) + "<small></small></div>";
  row += "<div class='field'><label>Value</label><input class='msg-value' maxlength='128' name='" + p + "v_" + idx + "_" + String(order) + "' value='" + htmlEscape(m.valueStr) + "' oninput='limitAndValidate(this,128)'><small><span class='err'></span><span class='bytes'></span></small></div>";
  row += "<button type='button' class='remove-msg' onclick='removeMsg(this)'>Delete</button></div>";
  return row;
}

static String clickMessagesHtml(const String& idx, const String& prefix, bool sequenceMode,
                                const OSCMessage* press, uint8_t pressCount,
                                const OSCMessage* release, uint8_t releaseCount) {
  String group=prefix+idx, out="<div id='"+prefix+"pr_"+idx+"' style='display:"+String(sequenceMode?"none":"block")+"'>";
  out += "<div class='usage'><strong>Messages <span id='count_"+group+"'>"+String(pressCount+releaseCount)+"</span> / 8</strong><span>Press + Release</span></div>";
  out += "<input type='hidden' id='pc_"+group+"' name='"+prefix+"pc_"+idx+"' value='"+String(pressCount)+"'><input type='hidden' id='rc_"+group+"' name='"+prefix+"rc_"+idx+"' value='"+String(releaseCount)+"'>";
  out += "<div class='event-tabs'><button type='button' class='event-tab active' onclick=\"showEvent('"+group+"','press',this)\">Press</button><button type='button' class='event-tab' onclick=\"showEvent('"+group+"','release',this)\">Release</button></div>";
  out += "<div class='event-panel' data-group='"+group+"' data-event='press'><div class='osc-list' id='list_press_"+group+"'>";
  for(uint8_t i=0;i<pressCount;i++) out+=messageRowHtml(group,prefix,"press",i,press[i]);
  out += "</div><div class='empty'>No OSC message is sent when pressed.</div><button type='button' class='add-msg' data-group='"+group+"' data-prefix='"+prefix+"' data-event='press' onclick='addMsg(this)'>+ Add OSC Message</button></div>";
  out += "<div class='event-panel' data-group='"+group+"' data-event='release' style='display:none'><div class='osc-list' id='list_release_"+group+"'>";
  for(uint8_t i=0;i<releaseCount;i++) out+=messageRowHtml(group,prefix,"release",i,release[i]);
  out += "</div><div class='empty'>No OSC message is sent when released.</div><button type='button' class='add-msg' data-group='"+group+"' data-prefix='"+prefix+"' data-event='release' onclick='addMsg(this)'>+ Add OSC Message</button></div></div>";
  return out;
}

static bool validOscAddressText(const String& address, String& error) {
  if (!address.length() || !address.startsWith("/")) { error = "OSC Address must start with /."; return false; }
  if (address.length() > MAX_OSC_ADDRESS_BYTES) { error = "OSC Address is too long."; return false; }
  for (size_t i = 0; i < address.length(); i++) {
    char c = address[i];
    if (isspace((unsigned char)c) || c == '#' || c == '*' || c == ',' || c == '?' || c == '[' || c == ']' || c == '{' || c == '}') {
      error = "OSC Address contains an invalid character."; return false;
    }
  }
  return true;
}

static bool validOscMessage(const OSCMessage& m, String& error) {
  String address = m.address;
  address.trim();
  if (!validOscAddressText(address, error)) return false;
  if (m.valueStr.length() > MAX_OSC_VALUE_BYTES) { error = "OSC Value is too long."; return false; }
  if (m.valueType == TYPE_FLOAT) {
    char* end = nullptr; float value = strtof(m.valueStr.c_str(), &end);
    if (!end || end == m.valueStr.c_str() || *end != '\0' || !isfinite(value)) { error = "Float value is invalid."; return false; }
  } else if (m.valueType == TYPE_INT) {
    errno = 0; char* end = nullptr; long value = strtol(m.valueStr.c_str(), &end, 10);
    if (!end || end == m.valueStr.c_str() || *end != '\0' || errno == ERANGE || value < INT32_MIN || value > INT32_MAX) { error = "Integer value is invalid."; return false; }
  }
  return true;
}

static bool parseMessageList(const String& idx,const String& prefix,OSCMessage* press,uint8_t& pc,OSCMessage* release,uint8_t& rc,String& error){
  int p=constrain(server.arg(prefix+"pc_"+idx).toInt(),0,MAX_KEY_OSC_MESSAGES),r=constrain(server.arg(prefix+"rc_"+idx).toInt(),0,MAX_KEY_OSC_MESSAGES);
  if(p+r>MAX_KEY_OSC_MESSAGES){error="Press and Release messages must total 8 or fewer.";return false;}
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

static String deviceJson(const ChainDevice& device) {
  String out = String("{\"uid\":") + jsonString(device.uid) +
               ",\"deviceType\":" + String((int)device.type) +
               ",\"deviceTypeName\":" + jsonString(String(typeToName(device.type))) +
               ",\"displayName\":" + jsonString(device.displayName);
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
    if (!jsonRange(v["range"].as<JsonObjectConst>(), device.tof.map, error)) return false;
  } else { error = "Unsupported device type."; return false; }
  if (deviceConfigStorageBytes(device) > MAX_DEVICE_CONFIG_BYTES) { error = "Device configuration is too large."; return false; }
  return true;
}

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------
void registerWebRoutes() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/delete_wifi", handleDeleteWifi);
  server.on("/delete_device", handleDeleteDevice);
  server.on("/set_rotation", handleSetRotation);
  server.on("/export_settings", HTTP_GET, handleExportSettings);
  server.on("/import_settings", HTTP_POST, handleImportSettings);
}

// ---------------------------------------------------------------------------
// AP captive portal
// ---------------------------------------------------------------------------
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
    server.send(200, "text/html", "<h2>Saved</h2>");
    delay(1500);
    ESP.restart();
  }
  server.send(400, "text/plain", "Error");
}

// ---------------------------------------------------------------------------
// Main settings page
// ---------------------------------------------------------------------------
void handleRoot() {
  String html = R"raw(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>OSC Settings</title>
<style>
body{font-family:sans-serif;margin:16px;background:#f5f5f5}
.card{background:#fff;padding:16px;border-radius:10px;margin-bottom:16px;box-shadow:0 2px 5px rgba(0,0,0,.1)}
.saved-settings{margin-top:28px}
.backup-grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}.backup-grid .card{margin-bottom:16px;display:flex;flex-direction:column}.backup-grid form,.backup-grid button{margin-top:auto}@media(max-width:720px){.backup-grid{grid-template-columns:1fr}.osc-row{grid-template-columns:52px 1fr}.osc-row .field,.remove-msg{grid-column:2}.key-grid,.seq-grid{grid-template-columns:1fr}.seq-address{grid-column:1}}
h1{font-size:1.4em}h2{margin-top:0;font-size:1.1em}
label{display:block;margin-top:10px;font-weight:bold;font-size:.9em}
input,select{width:100%;padding:8px;margin-top:4px;box-sizing:border-box}
input.invalid,select.invalid{border:2px solid #c73c4a;background:#fff8f8}
.key-grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;align-items:start}.key-grid label{margin-top:0}
.usage{display:flex;justify-content:space-between;align-items:center;margin:14px 0;padding:11px 13px;border:1px solid #cddbf8;border-radius:9px;background:#edf3ff;color:#244da7}
.event-tabs{display:flex;gap:4px;padding:4px;background:#edf0f4;border-radius:9px}.event-tab{margin:0;background:transparent;color:#697586}.event-tab.active{background:white;color:#18212f;box-shadow:0 1px 4px #bbb}
.event-panel{margin-top:12px}.osc-list{display:grid;gap:10px}.osc-row{display:grid;grid-template-columns:62px minmax(180px,1fr) 115px minmax(100px,.55fr) 68px;gap:9px;align-items:start;padding:12px;border:1px solid #dce2ea;border-radius:10px;background:#fbfcfe}
.osc-row .field label{margin-top:0}.osc-row small{display:flex;justify-content:space-between;min-height:17px;color:#697586}.osc-row .err{color:#c73c4a}.order{display:flex;gap:3px;align-self:center}.mv{width:auto;margin:0;padding:7px;background:#fff;color:#526075;border:1px solid #dce2ea}.remove-msg{width:auto;margin-top:22px;padding:9px;background:#fff3f4;color:#c73c4a;border:1px solid #efc6cb}.add-msg{background:#f7faff;color:#3267e3;border:1px dashed #9db6ef}.add-msg:disabled{background:#eee;color:#888}.empty{display:none;padding:18px;text-align:center;color:#697586;border:1px dashed #dce2ea;border-radius:9px}.osc-list:empty+.empty{display:block}
.sequence-card{margin-top:12px;padding:15px;border:1px solid #dce2ea;border-radius:10px;background:#fbfcfe}.seq-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}.seq-address{grid-column:1/-1}
button{width:100%;padding:12px;background:#28a745;color:#fff;border:none;border-radius:6px;font-size:16px;margin-top:8px}
.btn-danger{background:#dc3545}.btn-warning{background:#ff9800}.btn-export{background:#3267e3}.btn-rot{background:#6f42c1;flex:1;margin:0}
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
</style>
<script>
const MAX_MSG=8;const enc=new TextEncoder();
function bytes(v){return enc.encode(v).length}
function toggleMode(pr,sq,sel){if(!pr||!sq)return;if(sel.value==='1'){pr.style.display='none';sq.style.display='block';}else{pr.style.display='block';sq.style.display='none';}}
function toggleClickMode(prId,sqId,sel){toggleMode(document.getElementById(prId),document.getElementById(sqId),sel);}
function showEvent(group,event,btn){document.querySelectorAll('.event-panel[data-group="'+group+'"]').forEach(x=>x.style.display=x.dataset.event===event?'block':'none');btn.parentNode.querySelectorAll('.event-tab').forEach(x=>x.classList.remove('active'));btn.classList.add('active')}
function allRows(group){return document.querySelectorAll('.osc-row[data-group="'+group+'"]')}
function renumber(group){let rows=allRows(group),prefix=rows.length?rows[0].dataset.prefix:document.querySelector('.add-msg[data-group="'+group+'"]').dataset.prefix,idx=group.substring(prefix.length);['press','release'].forEach(ev=>{document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="'+ev+'"]').forEach((r,i)=>{let p=prefix+(ev==='press'?'p':'r');r.querySelector('.msg-address').name=p+'a_'+idx+'_'+i;r.querySelector('.type').name=p+'t_'+idx+'_'+i;r.querySelector('.msg-value').name=p+'v_'+idx+'_'+i})});let n=rows.length;document.getElementById('count_'+group).textContent=n;document.getElementById('pc_'+group).value=document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="press"]').length;document.getElementById('rc_'+group).value=document.querySelectorAll('.osc-row[data-group="'+group+'"][data-event="release"]').length;document.querySelectorAll('.add-msg[data-group="'+group+'"]').forEach(b=>b.disabled=n>=MAX_MSG)}
function moveMsg(btn,d){let r=btn.closest('.osc-row'),s=d<0?r.previousElementSibling:r.nextElementSibling;if(!s)return;d<0?r.parentNode.insertBefore(r,s):r.parentNode.insertBefore(s,r);renumber(r.dataset.group)}
function removeMsg(btn){let r=btn.closest('.osc-row'),g=r.dataset.group;r.remove();renumber(g)}
function addMsg(btn){let g=btn.dataset.group,prefix=btn.dataset.prefix,ev=btn.dataset.event;if(allRows(g).length>=MAX_MSG)return;let list=document.getElementById('list_'+ev+'_'+g),r=document.createElement('div');r.className='osc-row';r.dataset.group=g;r.dataset.prefix=prefix;r.dataset.event=ev;r.innerHTML='<div class="order"><button type="button" class="mv" onclick="moveMsg(this,-1)">&uarr;</button><button type="button" class="mv" onclick="moveMsg(this,1)">&darr;</button></div><div class="field"><label>OSC Address</label><input class="msg-address" maxlength="192" oninput="limitAndValidate(this,192)"><small><span class="err"></span><span class="bytes"></span></small></div><div class="field"><label>Type</label><select class="type" onchange="validateInput(this.closest(\'.osc-row\').querySelector(\'.msg-value\'))"><option value="0">Float</option><option value="1">Int</option><option value="2">String</option></select><small></small></div><div class="field"><label>Value</label><input class="msg-value" maxlength="128" value="1.0" oninput="limitAndValidate(this,128)"><small><span class="err"></span><span class="bytes"></span></small></div><button type="button" class="remove-msg" onclick="removeMsg(this)">Delete</button>';list.appendChild(r);renumber(g);r.querySelector('.msg-address').focus()}
function limitBytes(i,max){while(bytes(i.value)>max)i.value=i.value.slice(0,-1)}
function limitAndValidate(i,max){limitBytes(i,max);validateInput(i)}
function validateInput(i){let max=i.classList.contains('msg-address')?192:128,b=bytes(i.value),err='';if(i.classList.contains('msg-address')){if(!i.value)err='Required';else if(i.value[0]!='/')err='Start with /';else if(/[\s#*,?\[\]{}]/.test(i.value))err='Invalid character'}else if(i.classList.contains('msg-value')){let t=i.closest('.osc-row').querySelector('.type').value;if(t==='0'&&(!i.value.trim()||!Number.isFinite(Number(i.value))))err='Invalid float';if(t==='1'&&!/^[+-]?\d+$/.test(i.value.trim()))err='Invalid integer'}if(b>max)err='Too long';i.classList.toggle('invalid',!!err);let sm=i.parentNode.querySelector('small');sm.querySelector('.err').textContent=err;sm.querySelector('.bytes').textContent=b+' / '+max+' bytes';return !err}
function validateForm(){let ok=true;document.querySelectorAll('.osc-row .msg-address,.osc-row .msg-value').forEach(i=>{if(!validateInput(i))ok=false});if(!ok){let bad=document.querySelector('.invalid');if(bad)bad.focus();alert('Please correct the highlighted OSC message fields.')}return ok}
async function importSettings(){let input=document.getElementById('import-file'),status=document.getElementById('import-status');if(!input.files.length){alert('Select a JSON file first.');return}let file=input.files[0];if(file.size>49152){alert('The JSON file is too large.');return}if(!confirm('Import the settings in this file? Matching device settings will be overwritten.'))return;status.textContent='Importing...';try{let body=await file.text(),response=await fetch('/import_settings',{method:'POST',headers:{'Content-Type':'application/json'},body});let message=await response.text();if(!response.ok)throw new Error(message);status.textContent=message;setTimeout(()=>location.reload(),1000)}catch(e){status.textContent='Import failed: '+e.message}}
window.addEventListener('DOMContentLoaded',()=>{document.querySelectorAll('.osc-row').forEach(r=>{validateInput(r.querySelector('.msg-address'));validateInput(r.querySelector('.msg-value'))});document.querySelectorAll('[id^="count_"]').forEach(x=>renumber(x.id.substring(6)))})
</script></head><body><h1>Chain OSC (VRChat)</h1>
)raw";

  html += "<div class='card'><h2>WiFi</h2><p class='meta'>IP: " + ipStr + "</p>";
  html += "<form action='/delete_wifi' method='POST' onsubmit=\"return confirm('Delete WiFi?');\">";
  html += "<button class='btn-danger' type='submit'>Delete WiFi Settings</button></form></div>";

  html += "<div class='backup-grid'><div class='card'><h2>Settings Backup</h2>";
  html += "<p class='note'>Download saved settings as versioned JSON. WiFi credentials are not included.</p>";
  html += "<form action='/export_settings' method='GET'><button class='btn-export' type='submit'>Export Settings (JSON)</button></form></div>";
  html += "<div class='card'><h2>Restore Settings</h2>";
  html += "<p class='note'>Import a M5ChainOSC settings JSON. Devices in the file overwrite settings with the same UID; other saved devices are kept.</p>";
  html += "<input id='import-file' type='file' accept='application/json,.json'>";
  html += "<p id='import-status' class='meta'></p><button class='btn-export' type='button' onclick='importSettings()'>Import Settings (JSON)</button></div></div>";

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

  html += "<form action='/save' method='POST' onsubmit='return validateForm()'><div class='card'><h2>OSC Destination</h2>";
  html += "<label>Host IP</label><input name='host' value='" + htmlEscape(osc_host) + "'>";
  html += "<label>Port</label><input type='number' name='port' value='" + String(osc_port) + "'></div>";

  if (deviceCount == 0)
    html += "<div class='card'><p class='note'>No device connected.</p></div>";

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
    if (devices[i].type != CHAIN_KEY_TYPE_CODE)
      html += "<label>Device Name</label><input maxlength='64' name='nm_" + idx + "' value='" + htmlEscape(devices[i].displayName) + "' oninput='limitBytes(this,64)'>";
    html += "<input type='hidden' name='uid_" + idx + "' value='" + htmlEscape(devices[i].uid) + "'>";

    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      html += "<div class='key-grid'><div><label>Device Name</label><input maxlength='64' name='nm_" + idx + "' value='" + htmlEscape(devices[i].displayName) + "' oninput='limitBytes(this,64)'></div>";
      html += "<div><label>Key Mode</label><select name='md_" + idx + "' onchange=\"toggleClickMode('kpr_" + idx + "','ksq_" + idx + "',this)\">";
      html += "<option value='0'" + String(!isSeq ? " selected" : "") + ">Press / Release</option>";
      html += "<option value='1'" + String(isSeq ? " selected" : "") + ">Sequence</option></select></div></div>";
      html += clickMessagesHtml(idx,"k",isSeq,devices[i].pressMessages,devices[i].pressMessageCount,devices[i].releaseMessages,devices[i].releaseMessageCount);
      html += "<div id='ksq_" + idx + "' class='sequence-card' style='display:" + String(isSeq ? "block" : "none") + "'><h3>Advance the value on each press</h3><p class='note'>Move from Start by Step and return to Start after End.</p><div class='seq-grid'>";
      html += "<div class='seq-address'><label>OSC Address</label><input maxlength='192' name='sa_" + idx + "' value='" + htmlEscape(devices[i].seq.address) + "' oninput='limitBytes(this,192)'></div>";
      html += "<div><label>Start</label><input type='number' step='any' name='ss_" + idx + "' value='" + String(devices[i].seq.start) + "'></div>";
      html += "<div><label>End</label><input type='number' step='any' name='se_" + idx + "' value='" + String(devices[i].seq.end) + "'></div>";
      html += "<div><label>Step</label><input type='number' step='any' name='sp_" + idx + "' value='" + String(devices[i].seq.step) + "'></div>";
      html += "<div><label>Type</label>" + typeSelectHtml("st_" + idx, devices[i].seq.valueType) + "</div></div></div>";
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
      html += "<div class='click-section encoder-click'>";
      html += clickModeHtml("em_" + idx, devices[i].enc.clickMode, "epr_" + idx, "esq_" + idx);
      html += clickMessagesHtml(idx,"e",encSeq,devices[i].enc.pressMessages,devices[i].enc.pressMessageCount,devices[i].enc.releaseMessages,devices[i].enc.releaseMessageCount);
      html += "<div id='esq_" + idx + "' class='click-sequence' style='display:" + String(encSeq ? "block" : "none") + "'><strong>Click Sequence</strong>";
      html += "<label>Address</label><input name='ek_" + idx + "' value='" + htmlEscape(devices[i].enc.clickSeq.address) + "'>";
      html += "<label>Start</label><input type='number' step='any' name='en_" + idx + "' value='" + String(devices[i].enc.clickSeq.start) + "'>";
      html += "<label>End</label><input type='number' step='any' name='e2_" + idx + "' value='" + String(devices[i].enc.clickSeq.end) + "'>";
      html += "<label>Step</label><input type='number' step='any' name='e3_" + idx + "' value='" + String(devices[i].enc.clickSeq.step) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("el_" + idx, devices[i].enc.clickSeq.valueType) + "</div></div>";
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
      html += "<div class='chk'><input type='checkbox' name='jix_" + idx + "' value='1'" + String(devices[i].joy.invertX ? " checked" : "") + ">";
      html += "<span>Invert X (+/- 反転)</span></div>";
      html += "<label>Y Address</label><input name='jy_" + idx + "' value='" + htmlEscape(devices[i].joy.yAddr) + "'>";
      html += "<div class='chk'><input type='checkbox' name='jiy_" + idx + "' value='1'" + String(devices[i].joy.invertY ? " checked" : "") + ">";
      html += "<span>Invert Y (+/- 反転)</span></div>";
      html += "<label>Deadband</label><input type='number' name='jd_" + idx + "' value='" + String(devices[i].joy.deadband) + "'>";
      html += "<label>Out Min</label><input type='number' step='any' name='jo_" + idx + "' value='" + String(devices[i].joy.map.outMin) + "'>";
      html += "<label>Out Max</label><input type='number' step='any' name='jO_" + idx + "' value='" + String(devices[i].joy.map.outMax) + "'>";
      html += "<label>Out Type</label>" + typeSelectHtml("jt_" + idx, devices[i].joy.map.outType) + "</div>";
      html += "<div class='click-section joystick-click'>";
      html += clickModeHtml("jm_" + idx, devices[i].joy.clickMode, "jpr_" + idx, "jsq_" + idx);
      html += clickMessagesHtml(idx,"j",joySeq,devices[i].joy.pressMessages,devices[i].joy.pressMessageCount,devices[i].joy.releaseMessages,devices[i].joy.releaseMessageCount);
      html += "<div id='jsq_" + idx + "' class='click-sequence' style='display:" + String(joySeq ? "block" : "none") + "'><strong>Click Sequence</strong>";
      html += "<label>Address</label><input name='jk_" + idx + "' value='" + htmlEscape(devices[i].joy.clickSeq.address) + "'>";
      html += "<label>Start</label><input type='number' step='any' name='jn_" + idx + "' value='" + String(devices[i].joy.clickSeq.start) + "'>";
      html += "<label>End</label><input type='number' step='any' name='j2_" + idx + "' value='" + String(devices[i].joy.clickSeq.end) + "'>";
      html += "<label>Step</label><input type='number' step='any' name='j3_" + idx + "' value='" + String(devices[i].joy.clickSeq.step) + "'>";
      html += "<label>Type</label>" + typeSelectHtml("jl_" + idx, devices[i].joy.clickSeq.valueType) + "</div></div>";
    } else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
      html += "<div class='ang'><strong>ToF Distance (mm)</strong>";
      html += "<label>Address</label><input name='fa_" + idx + "' value='" + htmlEscape(devices[i].tof.addr) + "'>";
      html += "<label>Deadband (mm)</label><input type='number' name='fd_" + idx + "' value='" + String(devices[i].tof.deadband) + "'>";
      html += "<label>Out Min</label><input type='number' step='any' name='fo_" + idx + "' value='" + String(devices[i].tof.map.outMin) + "'>";
      html += "<label>Out Max</label><input type='number' step='any' name='fO_" + idx + "' value='" + String(devices[i].tof.map.outMax) + "'>";
      html += "<label>Out Type</label>" + numericTypeSelectHtml("ft_" + idx, devices[i].tof.map.outType);
      html += "<p class='note'>Input range fixed: 30–2000 mm → mapped to Out Min/Max</p></div>";
    } else {
      html += "<p class='note'>Type code: " + String((int)devices[i].type) + "</p>";
    }
    html += "</div>";
  }
  html += "<button type='submit'>Save All Settings</button></form>";

  html += "<div class='card saved-settings'><h2>Saved Device Settings</h2>";
  html += "<p class='note'>Save したデバイスのみ表示。</p></div>";
  for (int i = 0; i < MAX_KNOWN; i++) {
    if (!knownDevices[i].used || isPlaceholderUid(knownDevices[i].uid)) continue;
    bool con = isUidConnected(knownDevices[i].uid);
    String label = knownDevices[i].displayName.length()
                       ? knownDevices[i].displayName
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

// ---------------------------------------------------------------------------
// Save form
// ---------------------------------------------------------------------------
void handleSave() {
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
        server.send(400, "text/html; charset=utf-8", "<h2>Save error</h2><p>Device Name is too long. Reduce it and try again.</p><p><a href='/'>Back</a></p>");
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
        server.send(400, "text/html; charset=utf-8", "<h2>Save error</h2><p>Press and Release messages must total 8 or fewer.</p><p><a href='/'>Back</a></p>");
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
          server.send(400, "text/html; charset=utf-8", "<h2>Save error</h2><p>Press message " + String(m + 1) + ": " + htmlEscape(validationError) + "</p><p><a href='/'>Back</a></p>"); return;
        }
      }
      for (int m = 0; m < rc; m++) {
        candidate.releaseMessages[m].address = server.arg("kra_" + idx + "_" + String(m));
        candidate.releaseMessages[m].valueStr = server.arg("krv_" + idx + "_" + String(m));
        candidate.releaseMessages[m].valueType = (ValueType)constrain(server.arg("krt_" + idx + "_" + String(m)).toInt(), (int)TYPE_FLOAT, (int)TYPE_STRING);
        if (!validOscMessage(candidate.releaseMessages[m], validationError)) {
          server.send(400, "text/html; charset=utf-8", "<h2>Save error</h2><p>Release message " + String(m + 1) + ": " + htmlEscape(validationError) + "</p><p><a href='/'>Back</a></p>"); return;
        }
      }
      if (pc > 0) candidate.press = candidate.pressMessages[0];
      if (rc > 0) candidate.release = candidate.releaseMessages[0];
      if (server.hasArg("sa_" + idx)) candidate.seq.address = server.arg("sa_" + idx);
      candidate.seq.address.trim();
      if (!validOscAddressText(candidate.seq.address, validationError)) {
        server.send(400, "text/html; charset=utf-8", "<h2>Save error</h2><p>Sequence: " + htmlEscape(validationError) + "</p><p><a href='/'>Back</a></p>"); return;
      }
      if (server.hasArg("ss_" + idx)) candidate.seq.start = server.arg("ss_" + idx).toFloat();
      if (server.hasArg("se_" + idx)) candidate.seq.end = server.arg("se_" + idx).toFloat();
      if (server.hasArg("sp_" + idx)) candidate.seq.step = server.arg("sp_" + idx).toFloat();
      if (server.hasArg("st_" + idx)) candidate.seq.valueType = (ValueType)constrain(server.arg("st_" + idx).toInt(), (int)TYPE_FLOAT, (int)TYPE_STRING);
      normalizeSequence(candidate.seq);
      if (deviceConfigStorageBytes(candidate) > MAX_DEVICE_CONFIG_BYTES) {
        server.send(413, "text/html; charset=utf-8", "<h2>Save error</h2><p>The device configuration is too large. Delete messages or shorten Address and Value fields.</p><p><a href='/'>Back</a></p>"); return;
      }
      devices[i] = candidate;
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
      {String err;if(!parseMessageList(idx,"e",devices[i].enc.pressMessages,devices[i].enc.pressMessageCount,devices[i].enc.releaseMessages,devices[i].enc.releaseMessageCount,err)){server.send(400,"text/plain; charset=utf-8",err);return;}if(devices[i].enc.pressMessageCount)devices[i].enc.press=devices[i].enc.pressMessages[0];if(devices[i].enc.releaseMessageCount)devices[i].enc.release=devices[i].enc.releaseMessages[0];}
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
      devices[i].joy.invertX = server.hasArg("jix_" + idx);
      devices[i].joy.invertY = server.hasArg("jiy_" + idx);
      if (server.hasArg("jo_" + idx)) devices[i].joy.map.outMin = server.arg("jo_" + idx).toFloat();
      if (server.hasArg("jO_" + idx)) devices[i].joy.map.outMax = server.arg("jO_" + idx).toFloat();
      if (server.hasArg("jt_" + idx)) devices[i].joy.map.outType = (ValueType)server.arg("jt_" + idx).toInt();
      if (server.hasArg("jm_" + idx)) devices[i].joy.clickMode = (KeyMode)server.arg("jm_" + idx).toInt();
      {String err;if(!parseMessageList(idx,"j",devices[i].joy.pressMessages,devices[i].joy.pressMessageCount,devices[i].joy.releaseMessages,devices[i].joy.releaseMessageCount,err)){server.send(400,"text/plain; charset=utf-8",err);return;}if(devices[i].joy.pressMessageCount)devices[i].joy.press=devices[i].joy.pressMessages[0];if(devices[i].joy.releaseMessageCount)devices[i].joy.release=devices[i].joy.releaseMessages[0];}
      if (server.hasArg("jk_" + idx)) devices[i].joy.clickSeq.address = server.arg("jk_" + idx);
      if (server.hasArg("jn_" + idx)) devices[i].joy.clickSeq.start = server.arg("jn_" + idx).toFloat();
      if (server.hasArg("j2_" + idx)) devices[i].joy.clickSeq.end = server.arg("j2_" + idx).toFloat();
      if (server.hasArg("j3_" + idx)) devices[i].joy.clickSeq.step = server.arg("j3_" + idx).toFloat();
      if (server.hasArg("jl_" + idx)) devices[i].joy.clickSeq.valueType = (ValueType)server.arg("jl_" + idx).toInt();
      normalizeSequence(devices[i].joy.clickSeq);
    } else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
      if (server.hasArg("fa_" + idx)) {
        String a = server.arg("fa_" + idx);
        a.trim();
        if (a.length() && a.startsWith("/")) devices[i].tof.addr = a;
      }
      if (server.hasArg("fd_" + idx)) {
        int db = server.arg("fd_" + idx).toInt();
        devices[i].tof.deadband = constrain(db, 1, 2000);
      }
      if (server.hasArg("fo_" + idx)) devices[i].tof.map.outMin = server.arg("fo_" + idx).toFloat();
      if (server.hasArg("fO_" + idx)) devices[i].tof.map.outMax = server.arg("fO_" + idx).toFloat();
      if (server.hasArg("ft_" + idx)) {
        int t = server.arg("ft_" + idx).toInt();
        devices[i].tof.map.outType = (t == TYPE_INT) ? TYPE_INT : TYPE_FLOAT;
      }
    }

    if (!isPlaceholderUid(devices[i].uid) && !saveDeviceSettings(devices[i])) {
      server.send(507, "text/html; charset=utf-8", "<h2>Save error</h2><p>The device configuration could not be written to storage. Delete messages or shorten Address and Value fields, then try again.</p><p><a href='/'>Back</a></p>");
      return;
    }
  }
  server.send(200, "text/html", "<h2>Saved!</h2><p><a href='/'>Back</a></p>");
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
  server.send(200, "text/html", "<h2>WiFi deleted</h2>");
  delay(1200);
  ESP.restart();
}

void handleDeleteDevice() {
  if (!server.hasArg("uid")) {
    server.send(400, "text/plain", "uid");
    return;
  }
  deleteDeviceSettingsByUid(server.arg("uid"));
  server.send(200, "text/html", "<h2>Deleted</h2><p><a href='/'>Back</a></p>");
}

void handleExportSettings() {
  server.sendHeader("Content-Disposition", "attachment; filename=\"M5ChainOSC-settings-v" + String(SETTINGS_SCHEMA_VERSION) + ".json\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json; charset=utf-8", "");

  String header = String("{\"format\":") + jsonString(String(SETTINGS_FORMAT_NAME)) +
                  ",\"schemaVersion\":" + String(SETTINGS_SCHEMA_VERSION) +
                  ",\"wifiCredentialsIncluded\":false" +
                  ",\"global\":{\"oscHost\":" + jsonString(osc_host) +
                  ",\"oscPort\":" + String(osc_port) +
                  ",\"displayRotation\":" + String(displayRotation) + "},\"devices\":[";
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
    first = false;
  }
  server.sendContent("]}");
  server.sendContent("");
}

void handleImportSettings() {
  String body = server.arg("plain");
  if (!body.length()) { server.send(400, "text/plain; charset=utf-8", "Import file is empty."); return; }
  if (body.length() > 49152) { server.send(413, "text/plain; charset=utf-8", "Import file exceeds 48 KiB."); return; }

  DynamicJsonDocument document(65536);
  DeserializationError parseError = deserializeJson(document, body);
  body = "";
  if (parseError) {
    server.send(400, "text/plain; charset=utf-8", "Invalid JSON: " + String(parseError.c_str())); return;
  }
  JsonObjectConst root = document.as<JsonObjectConst>();
  if (!root["format"].is<const char*>() || String(root["format"].as<const char*>()) != SETTINGS_FORMAT_NAME) {
    server.send(400, "text/plain; charset=utf-8", "This is not an M5ChainOSC settings file."); return;
  }
  if (!root["schemaVersion"].is<int>()) {
    server.send(400, "text/plain; charset=utf-8", "schemaVersion is missing."); return;
  }
  int version = root["schemaVersion"].as<int>();
  if (version != SETTINGS_SCHEMA_VERSION) {
    server.send(400, "text/plain; charset=utf-8", "Unsupported schemaVersion: " + String(version)); return;
  }
  JsonObjectConst global = root["global"].as<JsonObjectConst>();
  JsonArrayConst importedDevices = root["devices"].as<JsonArrayConst>();
  if (global.isNull() || importedDevices.isNull() || importedDevices.size() > MAX_KNOWN ||
      !global["oscHost"].is<const char*>() || !global["oscPort"].is<int>() || !global["displayRotation"].is<int>()) {
    server.send(400, "text/plain; charset=utf-8", "Global settings or device list is invalid."); return;
  }
  String importedHost = global["oscHost"].as<const char*>();
  int importedPort = global["oscPort"].as<int>();
  int importedRotation = global["displayRotation"].as<int>();
  if (!importedHost.length() || importedHost.length() > 253 || importedPort < 1 || importedPort > 65535 || importedRotation < 0 || importedRotation > 3) {
    server.send(400, "text/plain; charset=utf-8", "Global OSC or display setting is out of range."); return;
  }

  ChainDevice* candidate = new (std::nothrow) ChainDevice();
  if (!candidate) { server.send(503, "text/plain; charset=utf-8", "Not enough memory to validate settings."); return; }
  String validationError;
  int deviceNumber = 0;
  for (size_t deviceIndex = 0; deviceIndex < importedDevices.size(); deviceIndex++) {
    JsonObjectConst object = importedDevices[deviceIndex].as<JsonObjectConst>();
    deviceNumber = (int)deviceIndex + 1;
    if (!deviceFromJson(object, *candidate, validationError)) {
      delete candidate;
      server.send(400, "text/plain; charset=utf-8", "Device " + String(deviceNumber) + ": " + validationError); return;
    }
    for (size_t previousIndex = 0; previousIndex < deviceIndex; previousIndex++) {
      JsonObjectConst previous = importedDevices[previousIndex].as<JsonObjectConst>();
      if (previous["uid"].is<const char*>() && String(previous["uid"].as<const char*>()) == candidate->uid) {
        String duplicateUid = candidate->uid;
        delete candidate;
        server.send(400, "text/plain; charset=utf-8", "Duplicate device UID: " + duplicateUid); return;
      }
    }
  }

  deviceNumber = 0;
  for (JsonObjectConst object : importedDevices) {
    deviceNumber++;
    if (!deviceFromJson(object, *candidate, validationError) || !saveDeviceSettings(*candidate)) {
      delete candidate;
      server.send(507, "text/plain; charset=utf-8", "Storage write failed at device " + String(deviceNumber) + "."); return;
    }
  }
  delete candidate;

  osc_host = importedHost;
  osc_port = importedPort;
  displayRotation = (uint8_t)importedRotation;
  prefs.begin("osc", false); prefs.putString("host", osc_host); prefs.putInt("port", osc_port); prefs.end();
  saveDisplayRotation();
  applyDisplayRotation();
  refreshChainDevices(true);
  if (!isAPMode && !resetInProgress) drawMainScreen();
  server.send(200, "text/plain; charset=utf-8", "Import completed. " + String(importedDevices.size()) + " device(s) restored.");
}
