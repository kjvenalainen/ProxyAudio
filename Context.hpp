// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include <atomic>
#include <memory>

#include "Registry.hpp"

namespace ProxyAudio {

struct Context {
  // Reference to the host object, which is null until the driver is
  // initialized.
  std::atomic<AudioServerPlugInHostRef> Host = nullptr;

  // Object registry providing dispatch to audio objects.
  std::shared_ptr<ProxyAudio::Registry> Registry =
      std::make_shared<ProxyAudio::Registry>();
};

}  // namespace ProxyAudio
