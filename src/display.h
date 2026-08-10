#pragma once

#include <Arduino.h>

void applyDisplayRotation();
void showMessage(const char* a, const char* b = "");
void drawMainScreen();
void showOscFeedback(const String& name, const String& address, const String& value);
void showResetProgress(unsigned long heldMs);
