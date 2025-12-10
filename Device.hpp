// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <mach/mach_time.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "Balance.hpp"
#include "CFStringUtils.hpp"
#include "Constants.hpp"
#include "DataDestination.hpp"
#include "DataSource.hpp"
#include "Error.hpp"
#include "Mute.hpp"
#include "Stream.hpp"
#include "Volume.hpp"

namespace ProxyAudio {

class Device : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Device(AudioObjectID id,
         AudioObjectRegistry& registry,
         AudioObjectID ownerId,
         const std::string& name,
         const std::string& modelUID)
      : AudioObjectInterface(id, kAudioDeviceClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        name_(name),
        modelUID_(modelUID),
        sampleRate_(DEFAULT_SAMPLE_RATE),
        hostTicksPerFrame_(ComputeHostTicksPerFrame(DEFAULT_SAMPLE_RATE)) {
    Log("Device constructor [id: %d, ownerId: %d]", id, ownerId);

    // Create input stream and its controls
    inputStream_ = registry.Construct<Stream>(id, Direction::Input,
                                              TerminalType::Microphone);

    inputVolume_ = registry.Construct<Volume>(id, Direction::Input);
    inputMute_ = registry.Construct<Mute>(id, Direction::Input);
    inputDataSource_ = registry.Construct<DataSource>(id, Direction::Input);

    // Create output stream and its controls
    outputStream_ = registry.Construct<Stream>(id, Direction::Output,
                                               TerminalType::Speaker);

    outputVolume_ = registry.Construct<Volume>(id, Direction::Output);
    outputMute_ = registry.Construct<Mute>(id, Direction::Output);
    outputDataSource_ = registry.Construct<DataSource>(id, Direction::Output);
    outputBalance_ = registry.Construct<Balance>(id, Direction::Output);

    // Create playthrough data destination
    playthroughDestination_ = registry.Construct<DataDestination>(id);
  }

  Device(const Device& other) noexcept = delete;
  Device(Device&& other) noexcept = delete;

  ~Device() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyName:
      case kAudioObjectPropertyManufacturer:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioDevicePropertyDeviceUID:
      case kAudioDevicePropertyModelUID:
      case kAudioDevicePropertyTransportType:
      case kAudioDevicePropertyRelatedDevices:
      case kAudioDevicePropertyClockDomain:
      case kAudioDevicePropertyDeviceIsAlive:
      case kAudioDevicePropertyDeviceIsRunning:
      case kAudioDevicePropertyNominalSampleRate:
      case kAudioDevicePropertyAvailableNominalSampleRates:
      case kAudioDevicePropertyIsHidden:
      case kAudioDevicePropertyZeroTimeStampPeriod:
      case kAudioDevicePropertyIcon:
      case kAudioDevicePropertyStreams:
      case kAudioObjectPropertyControlList:
        return true;
      case kAudioDevicePropertyDeviceCanBeDefaultDevice:
      case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
      case kAudioDevicePropertyLatency:
      case kAudioDevicePropertySafetyOffset:
      case kAudioDevicePropertyPreferredChannelsForStereo:
      case kAudioDevicePropertyPreferredChannelLayout:
        return (inAddress->mScope == kAudioObjectPropertyScopeInput) ||
               (inAddress->mScope == kAudioObjectPropertyScopeOutput);
      default:
        return false;
    }
  }

  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyName:
      case kAudioObjectPropertyManufacturer:
      case kAudioDevicePropertyDeviceUID:
      case kAudioDevicePropertyModelUID:
      case kAudioDevicePropertyTransportType:
      case kAudioDevicePropertyRelatedDevices:
      case kAudioDevicePropertyClockDomain:
      case kAudioDevicePropertyDeviceIsAlive:
      case kAudioDevicePropertyDeviceIsRunning:
      case kAudioDevicePropertyDeviceCanBeDefaultDevice:
      case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
      case kAudioDevicePropertyStreams:
      case kAudioObjectPropertyControlList:
      case kAudioDevicePropertyLatency:
      case kAudioDevicePropertySafetyOffset:
      case kAudioDevicePropertyAvailableNominalSampleRates:
      case kAudioDevicePropertyIsHidden:
      case kAudioDevicePropertyPreferredChannelsForStereo:
      case kAudioDevicePropertyPreferredChannelLayout:
      case kAudioDevicePropertyZeroTimeStampPeriod:
      case kAudioDevicePropertyIcon:
        *outIsSettable = false;
        break;

      case kAudioDevicePropertyNominalSampleRate:
        *outIsSettable = true;
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "IsPropertySettable: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
  }

  OSStatus GetPropertyDataSize(pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyName:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyManufacturer:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = GetOwnedObjectsSize(inAddress->mScope);
        break;

      case kAudioDevicePropertyDeviceUID:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioDevicePropertyModelUID:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioDevicePropertyTransportType:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyRelatedDevices:
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioDevicePropertyClockDomain:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceIsAlive:
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioDevicePropertyDeviceIsRunning:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyNominalSampleRate:
        *outDataSize = sizeof(Float64);
        break;

      case kAudioDevicePropertyAvailableNominalSampleRates:
        *outDataSize = 2 * sizeof(AudioValueRange);
        break;

      case kAudioDevicePropertyIsHidden:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyZeroTimeStampPeriod:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyIcon:
        *outDataSize = sizeof(CFURLRef);
        break;

      case kAudioDevicePropertyStreams:
        *outDataSize = GetStreamsSize(inAddress->mScope);
        break;

      case kAudioObjectPropertyControlList:
        *outDataSize = GetControlListSize(inAddress->mScope);
        break;

      case kAudioDevicePropertyLatency:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertySafetyOffset:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyPreferredChannelsForStereo:
        *outDataSize = 2 * sizeof(UInt32);
        break;

      case kAudioDevicePropertyPreferredChannelLayout:
        *outDataSize = sizeof(AudioChannelLayout);
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyDataSize: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
  }

  OSStatus GetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Device kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioObjectClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Device kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioDeviceClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Device kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyName:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Device kAudioObjectPropertyName"));
        *((CFStringRef*)outData) = StringToCFString(name_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyManufacturer:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Device kAudioObjectPropertyManufacturer"));
        *((CFStringRef*)outData) = StringToCFString("ProxyAudio");
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyOwnedObjects:
        GetOwnedObjects(inAddress->mScope, inDataSize, outDataSize, outData);
        break;

      case kAudioDevicePropertyDeviceUID:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Device kAudioDevicePropertyDeviceUID"));
        *((CFStringRef*)outData) = StringToCFString(modelUID_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioDevicePropertyModelUID:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Device kAudioDevicePropertyModelUID"));
        *((CFStringRef*)outData) = StringToCFString(modelUID_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioDevicePropertyTransportType:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertyTransportType"));
        *((UInt32*)outData) = kAudioDeviceTransportTypeVirtual;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyRelatedDevices:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Device kAudioDevicePropertyRelatedDevices"));
        *((AudioObjectID*)outData) = 0;  // No related devices
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioDevicePropertyClockDomain:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertyClockDomain"));
        *((UInt32*)outData) = 0;  // No clock domain
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceIsAlive:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Device kAudioDevicePropertyDeviceIsAlive"));
        *((AudioClassID*)outData) = kAudioDeviceClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioDevicePropertyDeviceIsRunning:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertyDeviceIsRunning"));
        {
          std::lock_guard<std::mutex> lock(ioMutex_);
          *((UInt32*)outData) = (ioClientCount_ > 0) ? 1 : 0;
        }
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceCanBeDefaultDevice:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError(
                   "Device kAudioDevicePropertyDeviceCanBeDefaultDevice"));
        *((UInt32*)outData) = 1;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
        EXPECT(
            inDataSize >= sizeof(UInt32),
            BadDataSizeError(
                "Device kAudioDevicePropertyDeviceCanBeDefaultSystemDevice"));
        *((UInt32*)outData) = 1;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyNominalSampleRate:
        EXPECT(
            inDataSize >= sizeof(Float64),
            BadDataSizeError("Device kAudioDevicePropertyNominalSampleRate"));
        {
          std::lock_guard<std::mutex> lock(ioMutex_);
          *((Float64*)outData) = sampleRate_;
        }
        *outDataSize = sizeof(Float64);
        break;

      case kAudioDevicePropertyAvailableNominalSampleRates:
        EXPECT(inDataSize >= 2 * sizeof(AudioValueRange),
               BadDataSizeError(
                   "Device kAudioDevicePropertyAvailableNominalSampleRates"));
        {
          AudioValueRange* ranges = static_cast<AudioValueRange*>(outData);
          ranges[0].mMinimum = 44100.0;
          ranges[0].mMaximum = 44100.0;
          ranges[1].mMinimum = 48000.0;
          ranges[1].mMaximum = 48000.0;
        }
        *outDataSize = 2 * sizeof(AudioValueRange);
        break;

      case kAudioDevicePropertyIsHidden:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertyIsHidden"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyZeroTimeStampPeriod:
        EXPECT(
            inDataSize >= sizeof(UInt32),
            BadDataSizeError("Device kAudioDevicePropertyZeroTimeStampPeriod"));
        *((UInt32*)outData) = static_cast<UInt32>(RING_BUFFER_SIZE);
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyIcon:
        EXPECT(inDataSize >= sizeof(CFURLRef),
               BadDataSizeError("Device kAudioDevicePropertyIcon"));
        *((CFURLRef*)outData) = nullptr;  // No icon
        *outDataSize = sizeof(CFURLRef);
        break;

      case kAudioDevicePropertyStreams:
        GetStreams(inAddress->mScope, inDataSize, outDataSize, outData);
        break;

      case kAudioObjectPropertyControlList:
        GetControlList(inAddress->mScope, inDataSize, outDataSize, outData);
        break;

      case kAudioDevicePropertyLatency:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertyLatency"));
        *((UInt32*)outData) = 0;  // No latency
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertySafetyOffset:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Device kAudioDevicePropertySafetyOffset"));
        *((UInt32*)outData) = 0;  // No safety offset
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioDevicePropertyPreferredChannelsForStereo:
        EXPECT(inDataSize >= 2 * sizeof(UInt32),
               BadDataSizeError(
                   "Device kAudioDevicePropertyPreferredChannelsForStereo"));
        {
          UInt32* channels = static_cast<UInt32*>(outData);
          channels[0] = 1;
          channels[1] = 2;
        }
        *outDataSize = 2 * sizeof(UInt32);
        break;

      case kAudioDevicePropertyPreferredChannelLayout:
        EXPECT(inDataSize >= sizeof(AudioChannelLayout),
               BadDataSizeError(
                   "Device kAudioDevicePropertyPreferredChannelLayout"));
        {
          AudioChannelLayout* layout =
              static_cast<AudioChannelLayout*>(outData);
          layout->mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
          layout->mChannelBitmap = 0;
          layout->mNumberChannelDescriptions = 0;
        }
        *outDataSize = sizeof(AudioChannelLayout);
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
  }

  OSStatus SetPropertyData(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      const void* inData,
      std::vector<AudioObjectPropertyAddress>& changedAddresses) override {
    switch (inAddress->mSelector) {
      case kAudioDevicePropertyNominalSampleRate:
        EXPECT(inDataSize == sizeof(Float64),
               BadDataSizeError("Device SetPropertyData "
                                "kAudioDevicePropertyNominalSampleRate"));
        {
          Float64 newRate = *((const Float64*)inData);
          if (newRate == 44100.0 || newRate == 48000.0) {
            Float64 oldRate;
            {
              std::lock_guard<std::mutex> lock(ioMutex_);
              oldRate = sampleRate_;
              sampleRate_ = newRate;
              hostTicksPerFrame_ = ComputeHostTicksPerFrame(sampleRate_);
            }

            if (oldRate != newRate) {
              // Notify about sample rate change
              AudioObjectPropertyAddress addr;
              addr.mSelector = kAudioDevicePropertyNominalSampleRate;
              addr.mScope = kAudioObjectPropertyScopeGlobal;
              addr.mElement = kAudioObjectPropertyElementMain;
              changedAddresses.push_back(addr);
            }
          } else {
            throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                                "Unsupported sample rate");
          }
        }
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "SetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
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
        break;
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
  UInt32 GetOwnedObjectsSize(AudioObjectPropertyScope scope) const {
    switch (scope) {
      case kAudioObjectPropertyScopeGlobal:
        return 2 * sizeof(AudioObjectID);  // input and output streams
      case kAudioObjectPropertyScopeInput:
        return sizeof(AudioObjectID);  // input stream
      case kAudioObjectPropertyScopeOutput:
        return sizeof(AudioObjectID);  // output stream
      default:
        return 0;
    }
  }

  void GetOwnedObjects(AudioObjectPropertyScope scope,
                       UInt32 inDataSize,
                       UInt32* outDataSize,
                       void* outData) const {
    AudioObjectID* objects = static_cast<AudioObjectID*>(outData);
    UInt32 count = 0;

    switch (scope) {
      case kAudioObjectPropertyScopeGlobal:
        if (inDataSize >= 2 * sizeof(AudioObjectID)) {
          objects[0] = inputStream_->Id();
          objects[1] = outputStream_->Id();
          count = 2;
        }
        break;
      case kAudioObjectPropertyScopeInput:
        if (inDataSize >= sizeof(AudioObjectID)) {
          objects[0] = inputStream_->Id();
          count = 1;
        }
        break;
      case kAudioObjectPropertyScopeOutput:
        if (inDataSize >= sizeof(AudioObjectID)) {
          objects[0] = outputStream_->Id();
          count = 1;
        }
        break;
    }

    *outDataSize = count * sizeof(AudioObjectID);
  }

  UInt32 GetStreamsSize(AudioObjectPropertyScope scope) const {
    switch (scope) {
      case kAudioObjectPropertyScopeGlobal:
        return 2 * sizeof(AudioObjectID);
      case kAudioObjectPropertyScopeInput:
      case kAudioObjectPropertyScopeOutput:
        return sizeof(AudioObjectID);
      default:
        return 0;
    }
  }

  void GetStreams(AudioObjectPropertyScope scope,
                  UInt32 inDataSize,
                  UInt32* outDataSize,
                  void* outData) const {
    AudioObjectID* streams = static_cast<AudioObjectID*>(outData);
    UInt32 count = 0;

    switch (scope) {
      case kAudioObjectPropertyScopeGlobal:
        if (inDataSize >= 2 * sizeof(AudioObjectID)) {
          streams[0] = inputStream_->Id();
          streams[1] = outputStream_->Id();
          count = 2;
        }
        break;
      case kAudioObjectPropertyScopeInput:
        if (inDataSize >= sizeof(AudioObjectID)) {
          streams[0] = inputStream_->Id();
          count = 1;
        }
        break;
      case kAudioObjectPropertyScopeOutput:
        if (inDataSize >= sizeof(AudioObjectID)) {
          streams[0] = outputStream_->Id();
          count = 1;
        }
        break;
    }

    *outDataSize = count * sizeof(AudioObjectID);
  }

  UInt32 GetControlListSize(AudioObjectPropertyScope scope) const {
    // Return all controls regardless of scope, as per AudioServerPlugIn spec
    // input (volume, mute, datasource, balance) + output (volume, mute,
    // datasource, balance) + playthrough
    return 9 * sizeof(AudioObjectID);
  }

  void GetControlList(AudioObjectPropertyScope scope,
                      UInt32 inDataSize,
                      UInt32* outDataSize,
                      void* outData) const {
    AudioObjectID* controls = static_cast<AudioObjectID*>(outData);

    // Calculate how many items can fit in the buffer
    UInt32 maxCount = inDataSize / sizeof(AudioObjectID);
    if (maxCount > 9) {
      maxCount = 9;
    }

    // Return all controls regardless of scope, as per AudioServerPlugIn spec
    // Order: input (volume, mute, datasource, balance), output (volume, mute,
    // datasource, balance), playthrough
    UInt32 index = 0;

    if (index < maxCount)
      controls[index++] = inputVolume_->Id();
    if (index < maxCount)
      controls[index++] = inputMute_->Id();
    if (index < maxCount)
      controls[index++] = inputDataSource_->Id();
    if (index < maxCount)
      controls[index++] = outputVolume_->Id();
    if (index < maxCount)
      controls[index++] = outputMute_->Id();
    if (index < maxCount)
      controls[index++] = outputDataSource_->Id();
    if (index < maxCount)
      controls[index++] = outputBalance_->Id();
    if (index < maxCount)
      controls[index++] = playthroughDestination_->Id();

    *outDataSize = index * sizeof(AudioObjectID);
  }

  const AudioObjectID ownerId_;
  std::string name_;
  std::string modelUID_;
  std::mutex ioMutex_;
  std::atomic<uint64_t> ioClientCount_ = 0;
  uint64_t numberTimeStamps_ = 0;
  uint64_t anchorSampleTime_ = 0;
  uint64_t anchorHostTime_ = 0;
  double sampleRate_;
  double hostTicksPerFrame_;

  // Streams
  std::shared_ptr<Stream> inputStream_;
  std::shared_ptr<Stream> outputStream_;

  // Controls
  std::shared_ptr<Volume> inputVolume_;
  std::shared_ptr<Mute> inputMute_;
  std::shared_ptr<DataSource> inputDataSource_;
  std::shared_ptr<Volume> outputVolume_;
  std::shared_ptr<Mute> outputMute_;
  std::shared_ptr<DataSource> outputDataSource_;
  std::shared_ptr<Balance> outputBalance_;
  std::shared_ptr<DataDestination> playthroughDestination_;
};

}  // namespace ProxyAudio
