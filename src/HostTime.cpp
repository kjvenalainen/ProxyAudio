// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "HostTime.hpp"

#include <mach/mach_time.h>

namespace ProxyAudio {

UInt64 CurrentHostTime() {
  return mach_absolute_time();
}

}  // namespace ProxyAudio
