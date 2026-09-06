#pragma once

#include "types.h"

bool sendOSCValue(const String& address, ValueType type, float value, const String& strValue = "");
bool sendOSCInt32Value(const String& address, int32_t value);
bool sendOSC(const OSCMessage& m);
void sendMappedOsc(const String& name, const String& addr, float mapped, ValueType type);
void handleSequencePress(SequenceConfig& seq, const String& name);
