#pragma once

#include "types.h"

// NVS key helpers
uint32_t uidHash32(const String& uid);
String deviceCfgKey(const String& uid);
String deviceNameKey(const String& uid);
String deviceCfgKeyLegacy(const String& uid);
String deviceNameKeyLegacy(const String& uid);

// Defaults / serialize
void setDefaultDeviceMessages(ChainDevice& d);
String serializeDeviceConfig(const ChainDevice& d);
size_t deviceConfigStorageBytes(const ChainDevice& d);
void applySerializedConfig(ChainDevice& d, const String& blob);

// Per-device load / save / delete
String loadDeviceNameOnly(const String& uid);
void saveDeviceNameOnly(const String& uid, const String& name);
void applyKnownDisplayName(ChainDevice& d);
void loadDeviceSettings(ChainDevice& d);
bool saveDeviceSettings(const ChainDevice& d);
void deleteDeviceSettingsByUid(const String& uid);

// Known list
void clearKnownInMemory();
void saveKnownList();
void loadKnownList();
int  findKnownIndex(const String& uid);
bool isUidConnected(const String& uid);
void registerKnownDevice(const String& uid, const String& displayName, chain_device_type_t type);
void unregisterKnownDevice(const String& uid);

// Global settings
void loadWifiAndOscCommon();
void saveDisplayRotation();
void saveUiLanguage();
void resetAllSettings();
