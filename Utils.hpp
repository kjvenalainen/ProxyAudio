// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <os/log.h>

#include <span>

namespace ProxyAudio {

// Gets the filename from a file path.
constexpr static inline const char* GetFilename(const char* filePath) {
  size_t lastSlash = 0;
  for (size_t i = 0; filePath[i] != '\0'; i++) {
    if (filePath[i] == '/') {
      lastSlash = i;
    }
  }

  if (lastSlash == 0) {
    return filePath;
  }

  return filePath + lastSlash + 1;
}

// Converts a pointer and size into a span, clamping the size to the size of the
// array.
template <typename T>
constexpr static std::span<T> SafeSpan(T* out, size_t outSize) {
  return std::span<T>(out, outSize / sizeof(T));
}

#define Log(inFormat, ...)                                 \
  os_log(OS_LOG_DEFAULT, "%{public}s:%d | " inFormat "\n", \
         ProxyAudio::GetFilename(__FILE__), __LINE__, ##__VA_ARGS__)

}  // namespace ProxyAudio
