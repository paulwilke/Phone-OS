#pragma once

// The memory the firmware has left, for log lines that look for a squeeze: the free heap and the largest block
// that can still be allocated in one piece (a fragmented heap fails big allocations while "free" looks fine).

#include <esp_heap_caps.h>

#include <cstdio>
#include <string>

inline std::string heapSummary() {
  char text[64];
  std::snprintf(text, sizeof(text), "heap %u free, largest %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  return text;
}
