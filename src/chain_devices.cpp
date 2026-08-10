#include "chain_devices.h"
#include "globals.h"
#include "storage.h"
#include "osc_send.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Poll helpers
// ---------------------------------------------------------------------------
void pollEncoder(ChainDevice& d) {
  int16_t absv = 0;
  if (M5Chain.getEncoderValue(d.chainId, &absv) == CHAIN_OK) {
    if (!d.encInited) {
      d.lastEncAbs = absv;
      d.encInited = true;
    } else if (absv != d.lastEncAbs) {
      int16_t delta = absv - d.lastEncAbs;
      d.lastEncAbs = absv;
      float mapped;
      if (d.enc.sendIncrement) {
        mapped = clampf((float)delta * d.enc.incScale, d.enc.map.outMin, d.enc.map.outMax);
      } else {
        float span = d.enc.absInMax - d.enc.absInMin;
        float x = (float)absv;
        if (fabsf(span) > 1e-6f) {
          x = fmodf(x - d.enc.absInMin, span);
          if (x < 0) x += span;
          x += d.enc.absInMin;
        }
        mapped = mapClamped(x, d.enc.absInMin, d.enc.absInMax,
                            d.enc.map.outMin, d.enc.map.outMax);
      }
      sendMappedOsc(deviceDisplayName(d), d.enc.rotAddr, mapped, d.enc.map.outType);
    }
  }

  uint8_t st = 0;
  if (M5Chain.getEncoderButtonStatus(d.chainId, &st) == CHAIN_OK && st != d.lastButtonStatus) {
    if (d.enc.clickMode == MODE_SEQUENCE) {
      if (st == 1) {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_green, 3, &operation_status);
        handleSequencePress(d.enc.clickSeq, deviceDisplayName(d));
      } else {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      }
    } else if (st == 1) {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_red, 3, &operation_status);
      sendOSC(d.enc.press);
      showOscFeedback(deviceDisplayName(d), d.enc.press.address, d.enc.press.valueStr);
    } else {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      sendOSC(d.enc.release);
      showOscFeedback(deviceDisplayName(d), d.enc.release.address, d.enc.release.valueStr);
    }
    d.lastButtonStatus = st;
  }
}

void pollAngle(ChainDevice& d) {
  int val = -1;
  if (d.angle.use12bit) {
    uint16_t v12 = 0;
    if (M5Chain.getAngle12BitAdc(d.chainId, &v12) == CHAIN_OK) val = (int)v12;
  } else {
    uint8_t v8 = 0;
    if (M5Chain.getAngle8BitAdc(d.chainId, &v8) == CHAIN_OK) val = (int)v8;
  }
  if (val < 0) return;
  if (d.lastAngle < -99990) {
    d.lastAngle = val;
    return;
  }
  if (abs(val - d.lastAngle) >= max(1, d.angle.deadband)) {
    d.lastAngle = val;
    d.angle.map.inMin = 0;
    d.angle.map.inMax = d.angle.use12bit ? 4095.f : 255.f;
    float mapped = mapClamped((float)val, d.angle.map.inMin, d.angle.map.inMax,
                              d.angle.map.outMin, d.angle.map.outMax);
    sendMappedOsc(deviceDisplayName(d), d.angle.addr, mapped, d.angle.map.outType);
  }
}

void pollJoystick(ChainDevice& d) {
  int8_t x = 0, y = 0;
  if (M5Chain.getJoystickMappedInt8Value(d.chainId, &x, &y) == CHAIN_OK) {
    if (!d.joyInited) {
      d.lastJoyX = x;
      d.lastJoyY = y;
      d.joyInited = true;
    } else {
      bool cx = abs((int)x - (int)d.lastJoyX) >= max(1, d.joy.deadband);
      bool cy = abs((int)y - (int)d.lastJoyY) >= max(1, d.joy.deadband);
      if (cx || cy) {
        d.lastJoyX = x;
        d.lastJoyY = y;
        if (cx) {
          float xin = d.joy.invertX ? -(float)x : (float)x;
          float mx = mapClamped(xin, -127, 127, d.joy.map.outMin, d.joy.map.outMax);
          sendMappedOsc(deviceDisplayName(d), d.joy.xAddr, mx, d.joy.map.outType);
        }
        if (cy) {
          float yin = d.joy.invertY ? -(float)y : (float)y;
          float my = mapClamped(yin, -127, 127, d.joy.map.outMin, d.joy.map.outMax);
          sendMappedOsc(deviceDisplayName(d), d.joy.yAddr, my, d.joy.map.outType);
        }
      }
    }
  }

  uint8_t st = 0;
  if (M5Chain.getJoystickButtonStatus(d.chainId, &st) == CHAIN_OK && st != d.lastButtonStatus) {
    if (d.joy.clickMode == MODE_SEQUENCE) {
      if (st == 1) {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_green, 3, &operation_status);
        handleSequencePress(d.joy.clickSeq, deviceDisplayName(d));
      } else {
        M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      }
    } else if (st == 1) {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_red, 3, &operation_status);
      sendOSC(d.joy.press);
      showOscFeedback(deviceDisplayName(d), d.joy.press.address, d.joy.press.valueStr);
    } else {
      M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
      sendOSC(d.joy.release);
      showOscFeedback(deviceDisplayName(d), d.joy.release.address, d.joy.release.valueStr);
    }
    d.lastButtonStatus = st;
  }
}

void pollAllDevices() {
  for (int i = 0; i < deviceCount; i++) {
    if (!devices[i].active) continue;
    if (devices[i].type == CHAIN_KEY_TYPE_CODE) {
      uint8_t st = 0;
      if (M5Chain.getKeyButtonStatus(devices[i].chainId, &st) != CHAIN_OK) continue;
      if (st == devices[i].lastButtonStatus) continue;
      if (devices[i].mode == MODE_SEQUENCE) {
        if (st == 1) {
          M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_green, 3, &operation_status);
          handleSequencePress(devices[i].seq, deviceDisplayName(devices[i]));
        } else {
          M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
        }
      } else if (st == 1) {
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_red, 3, &operation_status);
        sendOSC(devices[i].press);
        showOscFeedback(deviceDisplayName(devices[i]), devices[i].press.address, devices[i].press.valueStr);
      } else {
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
        sendOSC(devices[i].release);
        showOscFeedback(deviceDisplayName(devices[i]), devices[i].release.address, devices[i].release.valueStr);
      }
      devices[i].lastButtonStatus = st;
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      pollEncoder(devices[i]);
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      pollAngle(devices[i]);
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      pollJoystick(devices[i]);
    }
  }
}

// ---------------------------------------------------------------------------
// Enumerate / hot-swap
// ---------------------------------------------------------------------------
static String buildFingerprint(const ChainDevice* list, int count) {
  String fp = String(count) + "|";
  for (int i = 0; i < count; i++)
    fp += list[i].uid + ":" + String((int)list[i].type) + ";";
  return fp;
}

bool refreshChainDevices(bool force) {
  if (!chainBusReady) return false;

  if (!M5Chain.isDeviceConnected()) {
    if (deviceCount != 0 || lastKnownDeviceCount != 0) {
      deviceCount = 0;
      lastKnownDeviceCount = 0;
      lastDeviceFingerprint = "";
      for (int i = 0; i < MAX_DEVICES; i++) devices[i] = ChainDevice();
      if (!isAPMode && !resetInProgress) drawMainScreen();
      return true;
    }
    return false;
  }

  uint16_t count = 0;
  if (M5Chain.getDeviceNum(&count) != CHAIN_OK) return false;
  if (count > MAX_DEVICES) count = MAX_DEVICES;

  static ChainDevice tmp[MAX_DEVICES];
  int tmpCount = 0;
  for (int i = 0; i < MAX_DEVICES; i++) tmp[i] = ChainDevice();

  if (count > 0) {
    device_info_t infoList[MAX_DEVICES];
    device_list_t list = {count, infoList};
    if (!M5Chain.getDeviceList(&list)) return false;

    for (uint16_t i = 0; i < count; i++) {
      ChainDevice& d = tmp[tmpCount];
      d = ChainDevice();
      d.active  = true;
      d.chainId = infoList[i].id;
      d.type    = infoList[i].device_type;
      d.lastButtonStatus = 0;

      uint8_t uid[12] = {0};
      if (M5Chain.getUID(d.chainId, UID_TYPE_12_BYTE, uid, 12, &operation_status) == CHAIN_OK) {
        d.uid = uidToString(uid, 12);
        d.uidShort = d.uid.substring(d.uid.length() - 8);
      } else {
        d.uid = "POS_" + String(d.chainId);
        d.uidShort = d.uid;
      }

      if (!isPlaceholderUid(d.uid)) {
        loadDeviceSettings(d);
      } else {
        setDefaultDeviceMessages(d);
        d.type = infoList[i].device_type;
      }
      tmpCount++;
    }
  }

  String fp = buildFingerprint(tmp, tmpCount);
  bool changed = (fp != lastDeviceFingerprint) || force;

  if (changed) {
    deviceCount = tmpCount;
    lastKnownDeviceCount = tmpCount;
    lastDeviceFingerprint = fp;
    for (int i = 0; i < MAX_DEVICES; i++) devices[i] = ChainDevice();
    for (int i = 0; i < tmpCount; i++) devices[i] = tmp[i];

    for (int i = 0; i < deviceCount; i++) {
      if (!devices[i].active) continue;
      if (devices[i].type == CHAIN_KEY_TYPE_CODE ||
          devices[i].type == CHAIN_ENCODER_TYPE_CODE ||
          devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
        M5Chain.setRGBLight(devices[i].chainId, 80, &operation_status);
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
      }
      if (devices[i].type == CHAIN_KEY_TYPE_CODE)
        M5Chain.getKeyButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE)
        M5Chain.getEncoderButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE)
        M5Chain.getJoystickButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
    }
    if (!isAPMode && !resetInProgress) drawMainScreen();
  }
  return changed;
}

bool initChainBus() {
  M5Chain.begin(&Serial2, 115200, RXD_PIN, TXD_PIN);
  delay(300);
  chainBusReady = true;
  refreshChainDevices(true);
  return true;
}
