#pragma once

#include "types.h"

// Legacy NVS key helpers used during migration to LittleFS.
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
// Builds a v2 candidate for a future explicit user-requested migration.
// Loading or normally saving a Legacy D1/D2 setting never calls this helper.
bool buildEncoderV2MigrationCandidate(const EncoderOscConfig& legacy,
                                      EncoderOscConfig& candidate);

// Per-device load / save / delete
String loadDeviceNameOnly(const String& uid);
void saveDeviceNameOnly(const String& uid, const String& name);
void applyKnownDisplayName(ChainDevice& d);
void loadDeviceSettings(ChainDevice& d);
bool saveDeviceSettings(const ChainDevice& d);
void deleteDeviceSettingsByUid(const String& uid);

// Known list. LittleFS files are authoritative; NVS is migration-only.
void clearKnownInMemory();
void saveKnownList();
void loadKnownList();
int  findKnownIndex(const String& uid);
bool isUidConnected(const String& uid);
int  knownDeviceCountForType(chain_device_type_t type);
bool canRegisterKnownDevice(const String& uid, chain_device_type_t type);
bool registerKnownDevice(const String& uid, const String& displayName, chain_device_type_t type);
void unregisterKnownDevice(const String& uid);

// Global settings
void loadWifiAndOscCommon();
bool saveDisplayRotation();
bool saveUiLanguage();
void resetAllSettings();
