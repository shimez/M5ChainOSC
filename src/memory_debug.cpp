#include "memory_debug.h"

#if M5CHAINOSC_MEMORY_DEBUG

#include <esp_heap_caps.h>

namespace {
unsigned fragmentationPercent(size_t freeBytes, size_t largestBlock) {
  if (freeBytes == 0) return 0;
  return 100U - static_cast<unsigned>((largestBlock * 100U) / freeBytes);
}
}  // namespace

void memoryDebugLog(const char* phase, size_t payloadBytes,
                    size_t jsonUsage, size_t jsonCapacity,
                    bool jsonOverflow) {
  const uint32_t internalCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  const uint32_t psramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
  const size_t internalFree = heap_caps_get_free_size(internalCaps);
  const size_t internalMinimum = heap_caps_get_minimum_free_size(internalCaps);
  const size_t internalLargest =
      heap_caps_get_largest_free_block(internalCaps);
  const size_t psramFree = heap_caps_get_free_size(psramCaps);
  const size_t psramMinimum = heap_caps_get_minimum_free_size(psramCaps);
  const size_t psramLargest = heap_caps_get_largest_free_block(psramCaps);

  Serial.printf(
      "[M5OSC][MEM] phase=%s payload=%u internal_free=%u "
      "internal_min=%u internal_largest=%u internal_frag=%u%% "
      "psram_free=%u psram_min=%u psram_largest=%u psram_frag=%u%% "
      "json_used=%u json_capacity=%u json_overflow=%d\n",
      phase ? phase : "UNKNOWN", static_cast<unsigned>(payloadBytes),
      static_cast<unsigned>(internalFree),
      static_cast<unsigned>(internalMinimum),
      static_cast<unsigned>(internalLargest),
      fragmentationPercent(internalFree, internalLargest),
      static_cast<unsigned>(psramFree), static_cast<unsigned>(psramMinimum),
      static_cast<unsigned>(psramLargest),
      fragmentationPercent(psramFree, psramLargest),
      static_cast<unsigned>(jsonUsage), static_cast<unsigned>(jsonCapacity),
      jsonOverflow ? 1 : 0);
}

#endif
