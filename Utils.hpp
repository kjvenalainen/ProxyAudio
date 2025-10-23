// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <os/log.h>

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

#define Log(inFormat, ...)                                 \
  os_log(OS_LOG_DEFAULT, "%{public}s:%d | " inFormat "\n", \
         ProxyAudio::GetFilename(__FILE__), __LINE__, ##__VA_ARGS__)

}  // namespace ProxyAudio
