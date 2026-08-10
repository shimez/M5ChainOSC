#include "osc_send.h"
#include "globals.h"
#include "display.h"

void sendOSCValue(const String& address, ValueType type, float value, const String& strValue) {
  if (WiFi.status() != WL_CONNECTED || !address.length()) return;
  if (type == TYPE_FLOAT)
    OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), value);
  else if (type == TYPE_INT)
    OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), (int)lroundf(value));
  else
    OscWiFi.send(osc_host.c_str(), osc_port, address.c_str(), strValue.c_str());
}

void sendOSC(const OSCMessage& m) {
  if (m.valueType == TYPE_STRING)
    sendOSCValue(m.address, TYPE_STRING, 0, m.valueStr);
  else if (m.valueType == TYPE_INT)
    sendOSCValue(m.address, TYPE_INT, (float)m.valueStr.toInt());
  else
    sendOSCValue(m.address, TYPE_FLOAT, m.valueStr.toFloat());
}

void sendMappedOsc(const String& name, const String& addr, float mapped, ValueType type) {
  sendOSCValue(addr, type, mapped);
  String vs = (type == TYPE_INT) ? String((int)lroundf(mapped)) : String(mapped, 3);
  showOscFeedback(name, addr, vs);
}

void handleSequencePress(SequenceConfig& seq, const String& name) {
  float v = seq.current;
  sendOSCValue(seq.address, seq.valueType, v);
  float next = v + seq.step;
  if (seq.step >= 0) {
    if (next > seq.end + 1e-6f) next = seq.start;
  } else {
    if (next < seq.end - 1e-6f) next = seq.start;
  }
  seq.current = next;
  showOscFeedback(name, seq.address,
                  seq.valueType == TYPE_INT ? String((int)v) : String(v, 2));
}
