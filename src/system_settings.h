#pragma once

#include <Arduino.h>

bool systemSettingsSetup();

const String& systemSettingsWifiSsid();
const String& systemSettingsWifiPassword();
const String& systemSettingsOscHost();
int systemSettingsOscPort();
uint8_t systemSettingsDisplayRotation();
bool systemSettingsHasUiLanguage();
uint8_t systemSettingsUiLanguage();

bool systemSettingsSaveWifi(const String& ssid, const String& password);
bool systemSettingsClearWifi();
bool systemSettingsSaveOsc(const String& host, int port);
bool systemSettingsSaveDisplayRotation(uint8_t rotation);
bool systemSettingsSaveUiLanguage(uint8_t language);
bool systemSettingsSaveCommon(const String& host, int port,
                              uint8_t rotation, bool languageConfigured,
                              uint8_t language);
bool systemSettingsClearAll();
