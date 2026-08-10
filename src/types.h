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

// ---------------------------------------------------------------------------
// Per-device OSC config
// ---------------------------------------------------------------------------
struct EncoderOscConfig {
  String         rotAddr       = "/avatar/parameters/Encoder";
  bool           sendIncrement = false;
  float          absInMin      = 0;
  float          absInMax      = 20;
  float          incScale      = 0.05f;
  RangeMap       map;
  KeyMode        clickMode     = MODE_PRESS_RELEASE;
  OSCMessage     press;
  OSCMessage     release;
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
  SequenceConfig clickSeq;
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
  SequenceConfig seq;

  EncoderOscConfig  enc;
  AngleOscConfig    angle;
  JoystickOscConfig joy;

  // runtime state
  uint8_t lastButtonStatus = 0;
  int16_t lastEncAbs       = 0;
  bool    encInited        = false;
  int     lastAngle        = -99999;
  int16_t lastJoyX         = 0;
  int16_t lastJoyY         = 0;
  bool    joyInited        = false;
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
