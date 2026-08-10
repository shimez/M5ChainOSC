#pragma once

#include "types.h"

bool initChainBus();
bool refreshChainDevices(bool force = false);

void pollEncoder(ChainDevice& d);
void pollAngle(ChainDevice& d);
void pollJoystick(ChainDevice& d);
void pollTof(ChainDevice& d);
void pollAllDevices();
