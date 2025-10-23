// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <mach/mach_time.h>

#include <atomic>

#include "AudioObjectInterface.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Device : public AudioObjectInterface {
 public:
  Device(AudioObjectID id) : AudioObjectInterface(id, kAudioDeviceClassID) {}

  Device(const Device& other)
      : AudioObjectInterface(other),
        ioClientCount_(other.ioClientCount_.load()),
        numberTimeStamps_(other.numberTimeStamps_),
        anchorSampleTime_(other.anchorSampleTime_),
        anchorHostTime_(other.anchorHostTime_) {}

  Device(Device&& other) noexcept
      : AudioObjectInterface(other),
        ioClientCount_(other.ioClientCount_.load()),
        numberTimeStamps_(other.numberTimeStamps_),
        anchorSampleTime_(other.anchorSampleTime_),
        anchorHostTime_(other.anchorHostTime_) {}

  ~Device() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress);
  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable);
  OSStatus GetPropertyDataSize(pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize);
  OSStatus GetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData);
  OSStatus SetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData);
  OSStatus StartIO(UInt32 inClientID) {
    if (ioClientCount_ == std::numeric_limits<uint64_t>::max()) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "IO client count overflow");
    }

    if (ioClientCount_++ == 0U) {
      // We need to start the hardware, which in this case is just anchoring the
      // time line.
      ioClientCount_ = 1U;
      numberTimeStamps_ = 0U;
      anchorSampleTime_ = 0U;
      anchorHostTime_ = mach_absolute_time();
    }

    return S_OK;
  }

  OSStatus StopIO(UInt32 inClientID) {
    if (ioClientCount_ == 0U) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "IO client count underflow");
    }

    if (ioClientCount_-- == 1U) {
      // We need to stop the hardware, which in this case means that there's
      // nothing to do.
    }

    return S_OK;
  }

 private:
  std::atomic<uint64_t> ioClientCount_ = 0;
  uint64_t numberTimeStamps_ = 0;
  uint64_t anchorSampleTime_ = 0;
  uint64_t anchorHostTime_ = 0;
};

}  // namespace ProxyAudio
