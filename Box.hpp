// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <mach/mach_time.h>

#include "AudioObjectInterface.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Box : public AudioObjectInterface {
 public:
  Box(AudioObjectID id) : AudioObjectInterface(id, kAudioBoxClassID) {}

  Box(const Box& other) = default;

  Box(Box&& other) noexcept = default;

  ~Box() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    return false;
  }

  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) override {
    return kAudioHardwareBadPropertySizeError;
  }

  OSStatus GetPropertyDataSize(pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize) override {
    return kAudioHardwareBadPropertySizeError;
  }

  OSStatus GetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData) override {
    return kAudioHardwareBadPropertySizeError;
  }

  OSStatus SetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData) override {
    return kAudioHardwareBadPropertySizeError;
  }

 private:
};

}  // namespace ProxyAudio
