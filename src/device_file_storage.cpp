#include "device_file_storage.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

namespace {
constexpr char ROOT_DIR[] = "/device-settings";
constexpr char FILE_FORMAT[] = "M5ChainOSC-device-setting";
constexpr uint8_t FILE_VERSION = 1;
constexpr size_t JSON_CAPACITY = 8192;
bool mounted = false;

int typeSlot(chain_device_type_t type) {
  switch (type) {
    case CHAIN_KEY_TYPE_CODE: return 0;
    case CHAIN_ENCODER_TYPE_CODE: return 1;
    case CHAIN_ANGLE_TYPE_CODE: return 2;
    case CHAIN_JOYSTICK_TYPE_CODE: return 3;
    case CHAIN_TOF_TYPE_CODE: return 4;
    default: return -1;
  }
}

String fileStem(String uid) {
  uid.replace("/", "_");
  uid.replace("\\", "_");
  uid.replace(":", "_");
  return uid;
}

String settingPath(const String& uid) {
  return String(ROOT_DIR) + "/" + fileStem(uid) + ".json";
}

void logResult(const char* operation, const String& uid, const String& path,
               size_t fileBytes, const char* result, const char* reason) {
  const size_t total = mounted ? LittleFS.totalBytes() : 0;
  const size_t used = mounted ? LittleFS.usedBytes() : 0;
  Serial.printf(
      "[M5OSC][LITTLEFS] operation=%s uid=%s path=%s file_bytes=%u "
      "total_bytes=%u used_bytes=%u free_bytes=%u result=%s reason=%s\n",
      operation, uid.c_str(), path.c_str(), (unsigned)fileBytes,
      (unsigned)total, (unsigned)used,
      (unsigned)(total >= used ? total - used : 0), result, reason);
}

bool prepare() {
  if (!deviceFileStorageBegin()) return false;
  return LittleFS.exists(ROOT_DIR) || LittleFS.mkdir(ROOT_DIR);
}

bool validHeader(JsonDocument& document, const String& uid) {
  return String(document["format"] | "") == FILE_FORMAT &&
         document["version"].as<int>() == FILE_VERSION &&
         String(document["uid"] | "") == uid &&
         document["type"].is<int>() &&
         document["displayName"].is<const char*>() &&
         document["config"].is<const char*>();
}
}  // namespace

bool deviceFileStorageBegin() {
  if (mounted) return true;
  mounted = LittleFS.begin(true);
  if (!mounted) {
    Serial.println("[M5OSC][LITTLEFS] mount result=failed");
    return false;
  }
  if (!LittleFS.exists(ROOT_DIR)) LittleFS.mkdir(ROOT_DIR);
  deviceFileStorageLogUsage("mount");
  return true;
}

void deviceFileStorageLogUsage(const char* phase) {
  if (!mounted) return;
  const size_t total = LittleFS.totalBytes();
  const size_t used = LittleFS.usedBytes();
  Serial.printf(
      "[M5OSC][LITTLEFS] phase=%s total_bytes=%u used_bytes=%u free_bytes=%u\n",
      phase, (unsigned)total, (unsigned)used,
      (unsigned)(total >= used ? total - used : 0));
}

bool deviceFileStorageExists(const String& uid) {
  return prepare() && LittleFS.exists(settingPath(uid));
}

DeviceFileLoadResult deviceFileStorageLoad(ChainDevice& device, String& config) {
  if (!prepare()) return DeviceFileLoadResult::Error;
  const String path = settingPath(device.uid);
  if (!LittleFS.exists(path)) return DeviceFileLoadResult::NotFound;
  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    logResult("load", device.uid, path, 0, "failed", "open_failed");
    return DeviceFileLoadResult::Error;
  }
  const size_t bytes = file.size();
  DynamicJsonDocument document(JSON_CAPACITY);
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error || !validHeader(document, device.uid)) {
    logResult("load", device.uid, path, bytes, "failed",
              error ? "parse_failed" : "header_invalid");
    return DeviceFileLoadResult::Error;
  }
  const int type = document["type"].as<int>();
  if (device.type != CHAIN_UNKNOWN_TYPE_CODE && type != (int)device.type) {
    logResult("load", device.uid, path, bytes, "failed", "type_mismatch");
    return DeviceFileLoadResult::Error;
  }
  device.type = (chain_device_type_t)type;
  device.displayName = document["displayName"].as<const char*>();
  config = document["config"].as<const char*>();
  if (!config.length()) {
    logResult("load", device.uid, path, bytes, "failed", "config_empty");
    return DeviceFileLoadResult::Error;
  }
  logResult("load", device.uid, path, bytes, "ok", "none");
  return DeviceFileLoadResult::Loaded;
}

bool deviceFileStorageSave(const ChainDevice& device, const String& config) {
  if (!prepare() || !device.uid.length() || !config.length()) return false;
  DynamicJsonDocument document(JSON_CAPACITY);
  document["format"] = FILE_FORMAT;
  document["version"] = FILE_VERSION;
  document["uid"] = device.uid;
  document["type"] = (int)device.type;
  document["displayName"] = device.displayName;
  document["config"] = config;

  const String path = settingPath(device.uid);
  const String temporary = path + ".tmp";
  const String backup = path + ".bak";
  LittleFS.remove(temporary);
  File file = LittleFS.open(temporary, FILE_WRITE);
  if (!file) {
    logResult("save", device.uid, path, 0, "failed", "temp_open_failed");
    return false;
  }
  const size_t written = serializeJson(document, file);
  file.flush();
  const size_t bytes = file.size();
  file.close();
  if (!written || bytes != written) {
    LittleFS.remove(temporary);
    logResult("save", device.uid, path, bytes, "failed", "short_write");
    return false;
  }

  File verifyFile = LittleFS.open(temporary, FILE_READ);
  DynamicJsonDocument verify(JSON_CAPACITY);
  const DeserializationError error = deserializeJson(verify, verifyFile);
  verifyFile.close();
  if (error || !validHeader(verify, device.uid) ||
      String(verify["config"] | "") != config ||
      verify["type"].as<int>() != (int)device.type) {
    LittleFS.remove(temporary);
    logResult("save", device.uid, path, bytes, "failed", "temp_verify_failed");
    return false;
  }

  LittleFS.remove(backup);
  const bool hadCurrent = LittleFS.exists(path);
  if (hadCurrent && !LittleFS.rename(path, backup)) {
    LittleFS.remove(temporary);
    logResult("save", device.uid, path, bytes, "failed", "backup_failed");
    return false;
  }
  if (!LittleFS.rename(temporary, path)) {
    if (hadCurrent) LittleFS.rename(backup, path);
    LittleFS.remove(temporary);
    logResult("save", device.uid, path, bytes, "failed", "replace_failed");
    return false;
  }
  LittleFS.remove(backup);
  logResult("save", device.uid, path, bytes, "ok", "none");
  return true;
}

bool deviceFileStorageRemove(const String& uid) {
  if (!prepare()) return false;
  const String path = settingPath(uid);
  const bool removed = !LittleFS.exists(path) || LittleFS.remove(path);
  logResult("remove", uid, path, 0, removed ? "ok" : "failed",
            removed ? "none" : "remove_failed");
  return removed;
}

size_t deviceFileStorageList(KnownDevice* devices, size_t capacity) {
  if (!prepare()) return 0;
  File directory = LittleFS.open(ROOT_DIR);
  if (!directory || !directory.isDirectory()) return 0;
  size_t count = 0;
  size_t typeCounts[SAVED_DEVICE_TYPE_COUNT] = {};
  size_t skippedByTypeLimit = 0;
  File file = directory.openNextFile();
  while (file) {
    if (!file.isDirectory() && String(file.name()).endsWith(".json")) {
      DynamicJsonDocument document(JSON_CAPACITY);
      if (!deserializeJson(document, file) &&
          String(document["format"] | "") == FILE_FORMAT &&
          document["version"].as<int>() == FILE_VERSION &&
          document["uid"].is<const char*>() && document["type"].is<int>()) {
        chain_device_type_t type =
            (chain_device_type_t)document["type"].as<int>();
        const int slot = typeSlot(type);
        if (slot >= 0 && typeCounts[slot] < MAX_KNOWN_PER_TYPE &&
            count < capacity) {
          devices[count].used = true;
          devices[count].uid = document["uid"].as<const char*>();
          devices[count].displayName = document["displayName"] | "";
          devices[count].type = type;
          typeCounts[slot]++;
          count++;
        } else {
          skippedByTypeLimit++;
        }
      }
    }
    file.close();
    file = directory.openNextFile();
  }
  directory.close();
  Serial.printf(
      "[M5OSC][LITTLEFS] operation=list files=%u skipped_by_type_limit=%u\n",
      (unsigned)count, (unsigned)skippedByTypeLimit);
  return count;
}

bool deviceFileStorageClear() {
  if (!prepare()) return false;
  File directory = LittleFS.open(ROOT_DIR);
  if (directory && directory.isDirectory()) {
    File file = directory.openNextFile();
    while (file) {
      const String path = file.path();
      file.close();
      LittleFS.remove(path);
      file = directory.openNextFile();
    }
    directory.close();
  }
  deviceFileStorageLogUsage("clear");
  return true;
}
