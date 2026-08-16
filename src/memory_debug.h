#pragma once

#include <Arduino.h>

#include "config.h"

#if M5CHAINOSC_MEMORY_DEBUG
void memoryDebugLog(const char* phase, size_t payloadBytes = 0,
                    size_t jsonUsage = 0, size_t jsonCapacity = 0,
                    bool jsonOverflow = false);

#define MEMORY_DEBUG_LOG(phase, payloadBytes) \
  memoryDebugLog((phase), (payloadBytes))
#define MEMORY_DEBUG_JSON(phase, payloadBytes, document) \
  memoryDebugLog((phase), (payloadBytes), (document).memoryUsage(), \
                 (document).capacity(), (document).overflowed())
#else
#define MEMORY_DEBUG_LOG(phase, payloadBytes) do {} while (0)
#define MEMORY_DEBUG_JSON(phase, payloadBytes, document) do {} while (0)
#endif
