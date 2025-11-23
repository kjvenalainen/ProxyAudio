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
#include "AudioObjectRegistry.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Device : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Device(AudioObjectID id,
         AudioObjectRegistry& registry,
         const std::string& name,
         const std::string& modelUID)
      : AudioObjectInterface(id, kAudioDeviceClassID),
        AudioObjectRegistryRef(registry),
        name_(name),
        modelUID_(modelUID) {}

  Device(const Device& other) noexcept = delete;
  Device(Device&& other) noexcept = delete;

  ~Device() = default;

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

  OSStatus AddClient(const AudioServerPlugInClientInfo* inClientInfo) {
    // We don't currently track clients, so we just check the arguments and
    // return successfully.
    if (inClientInfo == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "Invalid client info");
    }

    return S_OK;
  }

  OSStatus RemoveClient(const AudioServerPlugInClientInfo* inClientInfo) {
    if (inClientInfo == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "Invalid client info");
    }

    return S_OK;
  }

  OSStatus PerformConfigurationChange(UInt64 inChangeAction,
                                      void* inChangeInfo) {
    return S_OK;
  }

  OSStatus AbortConfigurationChange(UInt64 inChangeAction, void* inChangeInfo) {
    return S_OK;
  }

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

  OSStatus GetZeroTimeStamp(UInt32 inClientID,
                            Float64* outSampleTime,
                            UInt64* outHostTime,
                            UInt64* outSeed) {
    std::lock_guard<std::mutex> lock(ioMutex_);

    const auto currentHostTime = mach_absolute_time();

    const auto hostTicksPerRingBuffer = hostTicksPerFrame_ * RING_BUFFER_SIZE;
    const auto hostTickOffset =
        (numberTimeStamps_ + 1) * hostTicksPerRingBuffer;
    const auto nextHostTime =
        anchorHostTime_ + static_cast<uint64_t>(hostTickOffset);

    if (nextHostTime <= currentHostTime) {
      numberTimeStamps_++;
    }

    *outSampleTime = numberTimeStamps_ * RING_BUFFER_SIZE;
    *outHostTime = anchorHostTime_ + numberTimeStamps_ * hostTicksPerRingBuffer;
    *outSeed = 1;

    return S_OK;
  }

  OSStatus WillDoIOOperation(UInt32 inClientID,
                             UInt32 inOperationID,
                             Boolean* outWillDo,
                             Boolean* outWillDoInPlace) {
    switch (inOperationID) {
      case kAudioServerPlugInIOOperationReadInput:
        *outWillDo = true;
        *outWillDoInPlace = true;
        break;
      case kAudioServerPlugInIOOperationWriteMix:
        *outWillDo = true;
        *outWillDoInPlace = true;
    }

    return S_OK;
  }

  OSStatus BeginIOOperation(UInt32 inClientID,
                            UInt32 inOperationID,
                            UInt32 inIOBufferFrameSize,
                            const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return S_OK;
  }

  OSStatus DoIOOperation(AudioObjectID inStreamObjectID,
                         UInt32 inClientID,
                         UInt32 inOperationID,
                         UInt32 inIOBufferFrameSize,
                         const AudioServerPlugInIOCycleInfo* inIOCycleInfo,
                         void* ioMainBuffer,
                         void* ioSecondaryBuffer) {
    // clear the buffer if this iskAudioServerPlugInIOOperationReadInput

    if (inOperationID == kAudioServerPlugInIOOperationReadInput) {
      memset(ioMainBuffer, 0, inIOBufferFrameSize * 8);
    }

    return S_OK;
  }

  OSStatus EndIOOperation(UInt32 inClientID,
                          UInt32 inOperationID,
                          UInt32 inIOBufferFrameSize,
                          const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return S_OK;
  }

 private:
  static double ComputeHostTicksPerFrame(const double sampleRate) {
    // Calculate the host ticks per frame
    struct mach_timebase_info timebaseInfo;
    if (KERN_SUCCESS != mach_timebase_info(&timebaseInfo)) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "Failed to get mach_timebase_info");
    }

    const auto theHostClockFrequency = static_cast<double>(timebaseInfo.denom) /
                                       static_cast<double>(timebaseInfo.numer) *
                                       1000000000.0;
    return theHostClockFrequency / sampleRate;
  }

  static constexpr double DEFAULT_SAMPLE_RATE = 48000.0;
  static constexpr size_t RING_BUFFER_SIZE = 16384U;

  std::string name_;
  std::string modelUID_;
  std::mutex ioMutex_;
  std::atomic<uint64_t> ioClientCount_ = 0;
  uint64_t numberTimeStamps_ = 0;
  uint64_t anchorSampleTime_ = 0;
  uint64_t anchorHostTime_ = 0;
  double hostTicksPerFrame_ = ComputeHostTicksPerFrame(DEFAULT_SAMPLE_RATE);
};

}  // namespace ProxyAudio
