#pragma once

#include "types.h"

enum class DeviceFileLoadResult : uint8_t { NotFound, Loaded, Error };

bool deviceFileStorageBegin();
void deviceFileStorageLogUsage(const char* phase);
bool deviceFileStorageExists(const String& uid);
DeviceFileLoadResult deviceFileStorageLoad(ChainDevice& device, String& config);
bool deviceFileStorageSave(const ChainDevice& device, const String& config);
bool deviceFileStorageRemove(const String& uid);
size_t deviceFileStorageList(KnownDevice* devices, size_t capacity);
bool deviceFileStorageClear();
