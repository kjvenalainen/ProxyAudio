// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <expected>
#include <memory>
#include <span>
#include <vector>

OSStatus GetPropertyDataSize(AudioObjectID objectID,
                             const AudioObjectPropertyAddress& address,
                             void* inputData,
                             uint32_t inputDataSize,
                             uint32_t& dataSize) {
  return AudioObjectGetPropertyDataSize(objectID, &address, inputDataSize,
                                        inputData, &dataSize);
}

OSStatus GetPropertyData(AudioObjectID objectID,
                         const AudioObjectPropertyAddress& address,
                         void* inputData,
                         uint32_t inputDataSize,
                         void* outputData,
                         uint32_t& outputDataSize) {
  return AudioObjectGetPropertyData(objectID, &address, inputDataSize,
                                    inputData, &outputDataSize, outputData);
}

std::vector<AudioObjectID> EnumerateAudioOutputDevices(
    std::shared_ptr<aspl::Context> context) {
  uint32_t size = 0;
  OSStatus status =
      GetPropertyDataSize(kAudioObjectSystemObject,
                          {
                              .mSelector = kAudioHardwarePropertyDevices,
                              .mScope = kAudioObjectPropertyScopeGlobal,
                              .mElement = kAudioObjectPropertyElementMain,
                          },
                          nullptr, 0, size);

  if (size != noErr) {
    context->Tracer->Message("Failed to get devices size: %d", size);

    return {};
  }

  std::vector<AudioObjectID> devices(size);
  uint32_t ioDataSize = size;

  status = GetPropertyData(kAudioObjectSystemObject,
                           {
                               .mSelector = kAudioHardwarePropertyDevices,
                               .mScope = kAudioObjectPropertyScopeGlobal,
                               .mElement = kAudioObjectPropertyElementMain,
                           },
                           nullptr, 0, devices.data(), ioDataSize);

  return devices;
}
