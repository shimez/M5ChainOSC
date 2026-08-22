#pragma once

#include "types.h"

bool sendOSCValue(const String& address, ValueType type, float value, const String& strValue = "");
bool sendOSC(const OSCMessage& m);
void sendMappedOsc(const String& name, const String& addr, float mapped, ValueType type);
void handleSequencePress(SequenceConfig& seq, const String& name);
