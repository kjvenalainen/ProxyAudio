// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <MacTypes.h>

namespace ProxyAudio {

// Kept as a free function so unit tests can provide a deterministic clock
// without changing the production object model.
UInt64 CurrentHostTime();

}  // namespace ProxyAudio
