#pragma once

#include <Arduino.h>
#include "types.h"

void applyDisplayRotation();
void showMessage(const char* a, const char* b = "");
void showWifiConnected(const String& ipAddress);
void drawMainScreen();
void showOscFeedback(const String& name, const String& address, const String& value);
void queueOscFeedback(const String& name, const OSCMessage* messages, uint8_t count);
void updateOscFeedbackDisplay();
void showResetProgress(unsigned long heldMs);
