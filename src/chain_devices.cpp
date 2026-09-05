#include "chain_devices.h"
#include "globals.h"
#include "storage.h"
#include "osc_send.h"
#include "display.h"

static void sendMessagesWithFeedback(const ChainDevice& device,
                                     const OSCMessage* messages,
                                     uint8_t count) {
  OSCMessage sentMessages[MAX_KEY_OSC_MESSAGES];
  uint8_t sentCount = 0;
  for (uint8_t i = 0; i < count; i++) {
    if (sendOSC(messages[i])) sentMessages[sentCount++] = messages[i];
  }
  if (sentCount > 0)
    queueOscFeedback(deviceDisplayName(device), sentMessages, sentCount);
}

// ---------------------------------------------------------------------------
// Poll helpers
// ---------------------------------------------------------------------------
static bool hasStatusLed(chain_device_type_t type) {
  return type == CHAIN_KEY_TYPE_CODE ||
         type == CHAIN_ENCODER_TYPE_CODE ||
         type == CHAIN_ANGLE_TYPE_CODE ||
         type == CHAIN_JOYSTICK_TYPE_CODE ||
         type == CHAIN_TOF_TYPE_CODE;
}

static bool isIdentifyActive(const ChainDevice& d, uint32_t now) {
  return d.identifyUntilMs != 0 && (int32_t)(d.identifyUntilMs - now) > 0;
}

static void setOperationalLed(ChainDevice& d, uint8_t color[3]) {
  if (!hasStatusLed(d.type) || isIdentifyActive(d, millis())) return;
  M5Chain.setRGBValue(d.chainId, 0, 1, color, 3, &operation_status);
}

bool identifyChainDevice(int index, const String& uid) {
  if (index < 0 || index >= deviceCount || !devices[index].active ||
      !hasStatusLed(devices[index].type) || devices[index].uid != uid) return false;
  M5Chain.setRGBLight(devices[index].chainId, 80, &operation_status);
  if (M5Chain.setRGBValue(devices[index].chainId, 0, 1, color_orange, 3,
                          &operation_status) != CHAIN_OK) return false;
  devices[index].identifyUntilMs = millis() + 10000UL;
  return true;
}

static void updateIdentifyLeds() {
  const uint32_t now = millis();
  for (int i = 0; i < deviceCount; i++) {
    ChainDevice& d = devices[i];
    if (!d.active || d.identifyUntilMs == 0 || isIdentifyActive(d, now)) continue;
    d.identifyUntilMs = 0;
    M5Chain.setRGBValue(d.chainId, 0, 1, color_blue, 3, &operation_status);
  }
}

static void pollLegacyEncoderRotation(ChainDevice& d) {
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
          if (d.enc.wrapAround) {
            d.boundedEncInited = false;
            x = fmodf(x - d.enc.absInMin, span);
            if (x < 0) x += span;
            x += d.enc.absInMin;
          } else {
            if (!d.boundedEncInited) {
              d.boundedEncAbs = clampf(x, d.enc.absInMin, d.enc.absInMax);
              d.boundedEncInited = true;
            } else {
              d.boundedEncAbs = clampf(d.boundedEncAbs + delta,
                                      d.enc.absInMin, d.enc.absInMax);
            }
            x = d.boundedEncAbs;
          }
        }
        mapped = mapClamped(x, d.enc.absInMin, d.enc.absInMax,
                            d.enc.map.outMin, d.enc.map.outMax);
      }
      sendMappedOsc(deviceDisplayName(d), d.enc.rotAddr, mapped, d.enc.map.outType);
    }
  }
}

static bool amountSnapshotMatches(const ChainDevice& d) {
  return d.encV2AmountSnapshotValid &&
         d.encV2RangeSteps == d.enc.rangeSteps &&
         d.encV2Wrap == d.enc.wrapAround &&
         d.encV2ClockwiseIncreases == d.enc.clockwiseIncreases &&
         d.encV2OutputMin == d.enc.outputMin &&
         d.encV2OutputMax == d.enc.outputMax &&
         d.encV2OutputType == d.enc.outputType;
}

static void captureAmountSnapshot(ChainDevice& d) {
  d.encV2AmountSnapshotValid = true;
  d.encV2RangeSteps = d.enc.rangeSteps;
  d.encV2Wrap = d.enc.wrapAround;
  d.encV2ClockwiseIncreases = d.enc.clockwiseIncreases;
  d.encV2OutputMin = d.enc.outputMin;
  d.encV2OutputMax = d.enc.outputMax;
  d.encV2OutputType = d.enc.outputType;
}

void resetEncoderV2Runtime(ChainDevice& d) {
  d.encV2LogicalPosition = 0;
  d.encV2SemanticsObserved = false;
  d.encV2AmountSnapshotValid = false;
}

static void observeV2Semantics(ChainDevice& d) {
  if (d.enc.rotationMode == ENCODER_ROTATION_AMOUNT) {
    const bool enteringAmount = !d.encV2SemanticsObserved ||
                                d.encV2ObservedMode != ENCODER_ROTATION_AMOUNT;
    if (enteringAmount || !amountSnapshotMatches(d))
      d.encV2LogicalPosition = 0;
    captureAmountSnapshot(d);
  }
  d.encV2ObservedMode = d.enc.rotationMode;
  d.encV2SemanticsObserved = true;
}

static String amountStringValue(float value) {
  String result(value, 3);
  return result == "-0.000" ? String("0.000") : result;
}

static bool sendV2AmountValue(const ChainDevice& d, float mapped) {
  const String& address = d.enc.rotAddr;
  const String name = deviceDisplayName(d);
  if (d.enc.outputType == TYPE_INT) {
    const int32_t value = (int32_t)lroundf(mapped);
    if (!sendOSCInt32Value(address, value)) return false;
    showOscFeedback(name, address, String(value));
    return true;
  }
  if (d.enc.outputType == TYPE_STRING) {
    const String value = amountStringValue(mapped);
    if (!sendOSCValue(address, TYPE_STRING, 0, value)) return false;
    showOscFeedback(name, address, value);
    return true;
  }
  if (!sendOSCValue(address, TYPE_FLOAT, mapped)) return false;
  showOscFeedback(name, address, String(mapped, 3));
  return true;
}

static bool sendV2DirectionValue(const ChainDevice& d, int16_t delta) {
  if (delta == 0) return false;
  const String& address = d.enc.rotAddr;
  const String& value = delta > 0 ? d.enc.clockwiseValue
                                  : d.enc.counterClockwiseValue;
  bool sent = false;
  if (d.enc.outputType == TYPE_INT) {
    sent = sendOSCInt32Value(address, (int32_t)strtol(value.c_str(), nullptr, 10));
  } else if (d.enc.outputType == TYPE_FLOAT) {
    sent = sendOSCValue(address, TYPE_FLOAT, value.toFloat());
  } else {
    sent = sendOSCValue(address, TYPE_STRING, 0, value);
  }
  if (sent) showOscFeedback(deviceDisplayName(d), address, value);
  return sent;
}

static void applyV2EncoderDelta(ChainDevice& d, int16_t delta) {
  if (d.enc.rotationMode < ENCODER_ROTATION_AMOUNT ||
      d.enc.rotationMode > ENCODER_ROTATION_DIRECTION) return;
  if (d.enc.rotationMode == ENCODER_ROTATION_AMOUNT &&
      (d.enc.rangeSteps < 1 || !isfinite(d.enc.outputMin) ||
       !isfinite(d.enc.outputMax) || !(d.enc.outputMin < d.enc.outputMax)))
    return;
  observeV2Semantics(d);
  if (delta == 0) return;

  if (d.enc.rotationMode == ENCODER_ROTATION_DIRECTION) {
    sendV2DirectionValue(d, delta);
    return;
  }

  // D3 validation guarantees rangeSteps >= 1 and finite ordered outputs.
  const int32_t rangeSteps = d.enc.rangeSteps;
  const int32_t amountDelta = d.enc.clockwiseIncreases
                                  ? (int32_t)delta
                                  : -(int32_t)delta;
  int64_t next = (int64_t)d.encV2LogicalPosition + amountDelta;
  if (d.enc.wrapAround) {
    const int64_t positionCount = (int64_t)rangeSteps + 1;
    next %= positionCount;
    if (next < 0) next += positionCount;
  } else {
    if (next < 0) next = 0;
    if (next > rangeSteps) next = rangeSteps;
  }
  d.encV2LogicalPosition = (int32_t)next;

  const float ratio = (float)d.encV2LogicalPosition / (float)rangeSteps;
  const float mapped = d.enc.outputMin +
                       ratio * (d.enc.outputMax - d.enc.outputMin);
  // A nonzero input always sends, including an outward step at a Stop endpoint.
  sendV2AmountValue(d, mapped);
}

static void pollV2EncoderRotation(ChainDevice& d) {
  int16_t delta = 0;
  if (M5Chain.getEncoderIncValue(d.chainId, &delta) != CHAIN_OK) return;
  applyV2EncoderDelta(d, delta);
}

void pollEncoder(ChainDevice& d) {
  if (d.enc.settingsModel == ENCODER_SETTINGS_V2)
    pollV2EncoderRotation(d);
  else
    pollLegacyEncoderRotation(d);

  uint8_t st = 0;
  if (M5Chain.getEncoderButtonStatus(d.chainId, &st) == CHAIN_OK && st != d.lastButtonStatus) {
    const KeyMode pushMode = d.enc.settingsModel == ENCODER_SETTINGS_V2
                                 ? d.enc.pushMode
                                 : d.enc.clickMode;
    if (pushMode == MODE_SEQUENCE) {
      if (st == 1) {
        setOperationalLed(d, color_green);
        handleSequencePress(d.enc.clickSeq, deviceDisplayName(d));
      } else {
        setOperationalLed(d, color_blue);
      }
    } else if (st == 1) {
      setOperationalLed(d, color_red);
      sendMessagesWithFeedback(d, d.enc.pressMessages, d.enc.pressMessageCount);
    } else {
      setOperationalLed(d, color_blue);
      sendMessagesWithFeedback(d, d.enc.releaseMessages, d.enc.releaseMessageCount);
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
        setOperationalLed(d, color_green);
        handleSequencePress(d.joy.clickSeq, deviceDisplayName(d));
      } else {
        setOperationalLed(d, color_blue);
      }
    } else if (st == 1) {
      setOperationalLed(d, color_red);
      sendMessagesWithFeedback(d, d.joy.pressMessages, d.joy.pressMessageCount);
    } else {
      setOperationalLed(d, color_blue);
      sendMessagesWithFeedback(d, d.joy.releaseMessages, d.joy.releaseMessageCount);
    }
    d.lastButtonStatus = st;
  }
}

static bool configureTof(ChainDevice& d) {
  uint8_t status = 0;
  chain_status_t result =
      M5Chain.setToFMeasureMode(d.chainId, CHAIN_TOF_MODE_CONTINUOUS, &status);
  if (result != CHAIN_OK || status == 0) return false;

  status = 0;
  result = M5Chain.setToFMeasureTime(d.chainId, 50, &status);
  return (result == CHAIN_OK && status != 0);
}

void pollTof(ChainDevice& d) {
  const uint32_t now = millis();

  // Retry configuration if previous attempt failed (every 2s)
  if (!d.tofConfigured) {
    if (d.lastTofConfigMs != 0 && (now - d.lastTofConfigMs) < 2000) return;
    d.lastTofConfigMs = now;
    if (!configureTof(d)) return;
    d.tofConfigured = true;
    d.tofInited = false;
  }

  // Match ~50ms measurement period; avoid flooding the Chain UART
  static const uint32_t TOF_POLL_INTERVAL_MS = 50;
  if (d.lastTofPollMs != 0 && (now - d.lastTofPollMs) < TOF_POLL_INTERVAL_MS) return;
  d.lastTofPollMs = now;

  uint16_t mm = 0;
  // Default API timeout is 100ms; keep short so a stuck read does not block the loop
  constexpr unsigned long TOF_READ_TIMEOUT_MS = 30;
  if (M5Chain.getToFDistance(d.chainId, &mm, TOF_READ_TIMEOUT_MS) != CHAIN_OK) {
    if (++d.tofReadFailures >= 5) {
      d.tofReadFailures = 0;
      d.tofConfigured = false;
      d.tofInited = false;
    }
    return;
  }
  d.tofReadFailures = 0;

  // Outside the configured active range means "no target" for OSC.
  // Reset initialization so the first valid value after re-entry is sent.
  const int maxDistanceMm = constrain(d.tof.maxDistanceMm, 31, 2000);
  if (mm < 30 || mm >= maxDistanceMm) {
    d.tofInited = false;
    d.lastTofMm = -1;
    return;
  }
  const int val = static_cast<int>(mm);

  bool firstValue = !d.tofInited;
  if (firstValue) {
    d.tofInited = true;
  } else if (abs(val - d.lastTofMm) < max(1, d.tof.deadband)) {
    return;
  }
  d.lastTofMm = val;

  d.tof.map.inMin = 30;
  d.tof.map.inMax = maxDistanceMm;
  const float mapped = d.tof.nearValueHigh
      ? mapClamped((float)val, d.tof.map.inMin, d.tof.map.inMax,
                   d.tof.map.outMax, d.tof.map.outMin)
      : mapClamped((float)val, d.tof.map.inMin, d.tof.map.inMax,
                   d.tof.map.outMin, d.tof.map.outMax);
  sendMappedOsc(deviceDisplayName(d), d.tof.addr, mapped, d.tof.map.outType);
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
          setOperationalLed(devices[i], color_green);
          handleSequencePress(devices[i].seq, deviceDisplayName(devices[i]));
        } else {
          setOperationalLed(devices[i], color_blue);
        }
      } else if (st == 1) {
        setOperationalLed(devices[i], color_red);
        sendMessagesWithFeedback(devices[i], devices[i].pressMessages,
                                 devices[i].pressMessageCount);
      } else {
        setOperationalLed(devices[i], color_blue);
        sendMessagesWithFeedback(devices[i], devices[i].releaseMessages,
                                 devices[i].releaseMessageCount);
      }
      devices[i].lastButtonStatus = st;
    } else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE) {
      pollEncoder(devices[i]);
    } else if (devices[i].type == CHAIN_ANGLE_TYPE_CODE) {
      pollAngle(devices[i]);
    } else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE) {
      pollJoystick(devices[i]);
    } else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
      pollTof(devices[i]);
    }
  }
  updateIdentifyLeds();
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
#if M5CHAINOSC_STORAGE_DEBUG
        Serial.printf("[M5OSC][CHAIN] ENUM id=%u type=%d uid=%s\n", d.chainId, (int)d.type, d.uid.c_str());
#endif
      } else {
        d.uid = "POS_" + String(d.chainId);
        d.uidShort = d.uid;
#if M5CHAINOSC_STORAGE_DEBUG
        Serial.printf("[M5OSC][CHAIN] ENUM UID FAILED id=%u type=%d placeholder=%s status=%d\n", d.chainId, (int)d.type, d.uid.c_str(), (int)operation_status);
#endif
      }

      tmpCount++;
    }
  }

  String fp = buildFingerprint(tmp, tmpCount);
  bool changed = (fp != lastDeviceFingerprint) || force;

  if (changed) {
    // Enumeration runs periodically for hot-swap detection. Read persistent
    // settings only after the topology fingerprint changes; otherwise every
    // scan would reopen every LittleFS device file.
    for (int i = 0; i < tmpCount; i++) {
      if (!isPlaceholderUid(tmp[i].uid)) {
        loadDeviceSettings(tmp[i]);
      } else {
        chain_device_type_t liveType = tmp[i].type;
        setDefaultDeviceMessages(tmp[i]);
        tmp[i].type = liveType;
      }
    }

    deviceCount = tmpCount;
    lastKnownDeviceCount = tmpCount;
    lastDeviceFingerprint = fp;
    for (int i = 0; i < MAX_DEVICES; i++) devices[i] = ChainDevice();
    for (int i = 0; i < tmpCount; i++) devices[i] = tmp[i];

    for (int i = 0; i < deviceCount; i++) {
      if (!devices[i].active) continue;
      if (hasStatusLed(devices[i].type)) {
        M5Chain.setRGBLight(devices[i].chainId, 80, &operation_status);
        M5Chain.setRGBValue(devices[i].chainId, 0, 1, color_blue, 3, &operation_status);
      }
      if (devices[i].type == CHAIN_KEY_TYPE_CODE)
        M5Chain.getKeyButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_ENCODER_TYPE_CODE)
        M5Chain.getEncoderButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_JOYSTICK_TYPE_CODE)
        M5Chain.getJoystickButtonStatus(devices[i].chainId, &devices[i].lastButtonStatus);
      else if (devices[i].type == CHAIN_TOF_TYPE_CODE) {
        devices[i].tofInited = false;
        devices[i].tofConfigured = false;
        devices[i].lastTofMm = -1;
        devices[i].lastTofPollMs = 0;
        devices[i].lastTofConfigMs = 0;
        devices[i].tofReadFailures = 0;
        // Actual setToFMeasureMode/Time runs in pollTof with retry on failure
      }
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
