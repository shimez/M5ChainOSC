#include "web_ui.h"
#include "globals.h"
#include "storage.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Small HTML helpers
// ---------------------------------------------------------------------------
static String typeSelectHtml(const String& name, ValueType cur) {
  String s = "<select name='" + name + "'>";
  s += "<option value='0'" + String(cur == TYPE_FLOAT ? " selected" : "") + ">Float</option>";
  s += "<option value='1'" + String(cur == TYPE_INT ? " selected" : "") + ">Int</option>";
  s += "<option value='2'" + String(cur == TYPE_STRING ? " selected" : "") + ">String</option></select>";
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

// ---------------------------------------------------------------------------
// Route registration
// ---------------------------------------------------------------------------
void registerWebRoutes() {
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/delete_wifi", handleDeleteWifi);
  server.on("/delete_device", handleDeleteDevice);
  server.on("/set_rotation", handleSetRotation);
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
.chk{display:flex;align-items:center;gap:8px;margin-top:6px;margin-bottom:4px}
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
      html += "<div class='chk'><input type='checkbox' name='jix_" + idx + "' value='1'" + String(devices[i].joy.invertX ? " checked" : "") + ">";
      html += "<span>Invert X (+/- 反転)</span></div>";
      html += "<label>Y Address</label><input name='jy_" + idx + "' value='" + htmlEscape(devices[i].joy.yAddr) + "'>";
      html += "<div class='chk'><input type='checkbox' name='jiy_" + idx + "' value='1'" + String(devices[i].joy.invertY ? " checked" : "") + ">";
      html += "<span>Invert Y (+/- 反転)</span></div>";
      html += "<label>Deadband</label><input type='number' name='jd_" + idx + "' value='" + String(devices[i].joy.deadband) + "'>";
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
