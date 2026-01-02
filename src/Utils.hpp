// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <cstdint>
#include <vector>

#include "AudioObjectUtils.hpp"
#include "aspl/Context.hpp"

namespace ProxyAudio {

std::vector<AudioObjectID> EnumerateAudioOutputDevices(
    std::shared_ptr<aspl::Context> context) {
  try {
    auto devices = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
        kAudioObjectSystemObject,
        {
            .mSelector = kAudioHardwarePropertyDevices,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    return devices;
  } catch (const OSStatusError& e) {
    context->Tracer->Message("Failed to get devices: %s", e.what());

    return {};
  }
}

std::string GetDeviceName(AudioObjectID deviceID) {
  return ProxyAudio::GetPropertyData<std::string>(
      deviceID,
      {
          .mSelector = kAudioObjectPropertyName,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

}  // namespace ProxyAudio
