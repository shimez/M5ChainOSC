#pragma once

#include "config.h"

// ---------------------------------------------------------------------------
// OSC message
// ---------------------------------------------------------------------------
struct OSCMessage {
  String    address   = "";
  String    valueStr  = "1.0";
  ValueType valueType = TYPE_FLOAT;
};

struct SequenceConfig {
  String    address   = "/seq";
  ValueType valueType = TYPE_FLOAT;
  float     start     = 0;
  float     end       = 10;
  float     step      = 1;
  float     current   = 0;
};

struct RangeMap {
  float     inMin   = 0;
  float     inMax   = 4095;
  float     outMin  = 0;
  float     outMax  = 1;
  ValueType outType = TYPE_FLOAT;
};

enum EncoderSettingsModel : uint8_t {
  ENCODER_SETTINGS_LEGACY = 0,
  ENCODER_SETTINGS_V2     = 1
};

enum EncoderRotationMode : uint8_t {
  ENCODER_ROTATION_AMOUNT    = 0,
  ENCODER_ROTATION_DIRECTION = 1
};

// ---------------------------------------------------------------------------
// Per-device OSC config
// ---------------------------------------------------------------------------
struct EncoderOscConfig {
  // Persistent Encoder model. D1/D2 settings that cannot be represented by
  // Device Preset v2 remain LEGACY without correction or implicit migration.
  EncoderSettingsModel settingsModel = ENCODER_SETTINGS_LEGACY;

  // D3 / Device Preset v2 semantic settings. Runtime-only state such as the
  // logical position and input baseline deliberately does not live here.
  EncoderRotationMode rotationMode = ENCODER_ROTATION_AMOUNT;
  uint16_t       rangeSteps = 20;
  bool           clockwiseIncreases = true;
  float          outputMin = 0;
  float          outputMax = 1;
  ValueType      outputType = TYPE_FLOAT;
  String         clockwiseValue = "0.05";
  String         counterClockwiseValue = "-0.05";
  KeyMode        pushMode = MODE_PRESS_RELEASE;

  // Phase 1 compatibility bridge for the released D1/D2 model and current
  // runtime/Web UI. Phase 2 will stop using these fields for v2 semantics;
  // they must remain available for the product-internal Legacy path.
  String         rotAddr       = "/avatar/parameters/Encoder";
  bool           sendIncrement = false;
  bool           wrapAround    = true;
  float          absInMin      = 0;
  float          absInMax      = 20;
  float          incScale      = 0.05f;
  RangeMap       map;
  KeyMode        clickMode     = MODE_PRESS_RELEASE;
  OSCMessage     press;
  OSCMessage     release;
  OSCMessage     pressMessages[MAX_KEY_OSC_MESSAGES];
  OSCMessage     releaseMessages[MAX_KEY_OSC_MESSAGES];
  uint8_t        pressMessageCount = 1;
  uint8_t        releaseMessageCount = 1;
  SequenceConfig clickSeq;
};

struct AngleOscConfig {
  String   addr     = "/avatar/parameters/Angle";
  bool     use12bit = true;
  int      deadband = 8;
  RangeMap map;
};

struct JoystickOscConfig {
  String         xAddr     = "/avatar/parameters/JoyX";
  String         yAddr     = "/avatar/parameters/JoyY";
  int            deadband  = 3;
  bool           invertX   = false;
  bool           invertY   = false;
  RangeMap       map;
  KeyMode        clickMode = MODE_PRESS_RELEASE;
  OSCMessage     press;
  OSCMessage     release;
  OSCMessage     pressMessages[MAX_KEY_OSC_MESSAGES];
  OSCMessage     releaseMessages[MAX_KEY_OSC_MESSAGES];
  uint8_t        pressMessageCount = 1;
  uint8_t        releaseMessageCount = 1;
  SequenceConfig clickSeq;
};

struct TofOscConfig {
  String   addr          = "/avatar/parameters/ToF";
  int      deadband      = 5;     // mm
  int      maxDistanceMm = 2000;  // exclusive active-range upper bound
  bool     nearValueHigh = false; // false: near=Out Min, true: near=Out Max
  RangeMap map;                   // in: 30–maxDistanceMm → out: configurable
};

// ---------------------------------------------------------------------------
// Live Chain device slot
// ---------------------------------------------------------------------------
struct ChainDevice {
  bool                active    = false;
  uint16_t            chainId   = 0;
  chain_device_type_t type      = CHAIN_UNKNOWN_TYPE_CODE;
  String              uid       = "";
  String              uidShort  = "";
  String              displayName = "";

  KeyMode        mode = MODE_PRESS_RELEASE;
  OSCMessage     press;
  OSCMessage     release;
  OSCMessage     pressMessages[MAX_KEY_OSC_MESSAGES];
  OSCMessage     releaseMessages[MAX_KEY_OSC_MESSAGES];
  uint8_t        pressMessageCount = 1;
  uint8_t        releaseMessageCount = 1;
  SequenceConfig seq;

  EncoderOscConfig  enc;
  AngleOscConfig    angle;
  JoystickOscConfig joy;
  TofOscConfig      tof;

  // runtime state
  uint8_t lastButtonStatus = 0;
  int16_t lastEncAbs       = 0;
  bool    encInited        = false;
  float   boundedEncAbs    = 0;
  bool    boundedEncInited = false;

  // Device Preset v2 Encoder runtime state. These values are volatile and
  // must never be serialized to LittleFS. UID continuity is added in Phase 3.
  int32_t encV2LogicalPosition = 0;
  bool encV2SemanticsObserved = false;
  EncoderRotationMode encV2ObservedMode = ENCODER_ROTATION_AMOUNT;
  bool encV2AmountSnapshotValid = false;
  uint16_t encV2RangeSteps = 0;
  bool encV2Wrap = false;
  bool encV2ClockwiseIncreases = true;
  float encV2OutputMin = 0;
  float encV2OutputMax = 0;
  ValueType encV2OutputType = TYPE_FLOAT;

  int     lastAngle        = -99999;
  int16_t lastJoyX         = 0;
  int16_t lastJoyY         = 0;
  bool    joyInited        = false;
  int      lastTofMm       = -1;
  bool     tofInited       = false;
  bool     tofConfigured   = false;
  uint32_t lastTofPollMs   = 0;
  uint32_t lastTofConfigMs = 0;
  uint8_t  tofReadFailures = 0;
  uint32_t identifyUntilMs = 0;
};

// ---------------------------------------------------------------------------
// Known (saved) device list entry
// ---------------------------------------------------------------------------
struct KnownDevice {
  bool                used        = false;
  String              uid         = "";
  String              displayName = "";
  chain_device_type_t type        = CHAIN_UNKNOWN_TYPE_CODE;
};
