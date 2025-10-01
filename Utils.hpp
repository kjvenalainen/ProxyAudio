// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <os/log.h>

namespace ProxyAudio {

#define Log(inFormat, ...)                                                     \
  os_log(OS_LOG_DEFAULT, "%{public}s:%d | " inFormat "\n", __func__, __LINE__, \
         ##__VA_ARGS__)

}  // namespace ProxyAudio
