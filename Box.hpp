// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <mach/mach_time.h>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "Device.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Box : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Box(AudioObjectID id, AudioObjectRegistry& registry)
      : AudioObjectInterface(id, kAudioBoxClassID),
        AudioObjectRegistryRef(registry),
        devices_() {
    // Temporary: Add a single device to the box. Eventually we will create
    // these dynamically.
    devices_.push_back(
        registry.Construct<Device>("ProxyAudio Device", "ProxyAudioDeviceUID"));
  }

  Box(const Box& other) noexcept = delete;
  Box(Box&& other) noexcept = delete;

  ~Box() = default;

  const bool Acquired() const { return acquired_; }

  void SetAcquired(bool acquired) { acquired_ = acquired; }

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

  // Readonly access to the devices.
  const std::vector<std::shared_ptr<Device>>& Devices() const {
    return devices_;
  }

 private:
  bool acquired_ = false;
  std::vector<std::shared_ptr<Device>> devices_;
};

}  // namespace ProxyAudio
