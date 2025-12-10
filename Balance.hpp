// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include <atomic>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Balance : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Balance(AudioObjectID id,
          AudioObjectRegistry& registry,
          AudioObjectID ownerId,
          AudioObjectPropertyScope scope,
          AudioObjectPropertyElement element)
      : AudioObjectInterface(id, kAudioStereoPanControlClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        scope_(scope),
        element_(element),
        value_(0.5f) {  // Default to center (0.5 = balanced)
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);
  }

  Balance(const Balance& other) noexcept = delete;
  Balance(Balance&& other) noexcept = delete;

  ~Balance() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioControlPropertyScope:
      case kAudioControlPropertyElement:
      case kAudioStereoPanControlPropertyValue:
      case kAudioStereoPanControlPropertyPanningChannels:
        return true;
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
      case kAudioObjectPropertyOwnedObjects:
      case kAudioControlPropertyScope:
      case kAudioControlPropertyElement:
      case kAudioStereoPanControlPropertyPanningChannels:
        *outIsSettable = false;
        break;

      case kAudioStereoPanControlPropertyValue:
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

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioControlPropertyScope:
        *outDataSize = sizeof(AudioObjectPropertyScope);
        break;

      case kAudioControlPropertyElement:
        *outDataSize = sizeof(AudioObjectPropertyElement);
        break;

      case kAudioStereoPanControlPropertyValue:
        *outDataSize = sizeof(Float32);
        break;

      case kAudioStereoPanControlPropertyPanningChannels:
        *outDataSize = 2 * sizeof(UInt32);
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
               BadDataSizeError("Balance kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioLevelControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Balance kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioStereoPanControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Balance kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioControlPropertyScope:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyScope),
               BadDataSizeError("Balance kAudioControlPropertyScope"));
        *((AudioObjectPropertyScope*)outData) = scope_;
        *outDataSize = sizeof(AudioObjectPropertyScope);
        break;

      case kAudioControlPropertyElement:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyElement),
               BadDataSizeError("Balance kAudioControlPropertyElement"));
        *((AudioObjectPropertyElement*)outData) = element_;
        *outDataSize = sizeof(AudioObjectPropertyElement);
        break;

      case kAudioStereoPanControlPropertyValue:
        EXPECT(inDataSize >= sizeof(Float32),
               BadDataSizeError("Balance kAudioStereoPanControlPropertyValue"));
        *((Float32*)outData) = value_.load();
        *outDataSize = sizeof(Float32);
        break;

      case kAudioStereoPanControlPropertyPanningChannels:
        EXPECT(inDataSize >= 2 * sizeof(UInt32),
               BadDataSizeError("Balance kAudioStereoPanControlPropertyPanningChannels"));
        {
          UInt32* channels = static_cast<UInt32*>(outData);
          channels[0] = 1;  // Left channel
          channels[1] = 2;  // Right channel
        }
        *outDataSize = 2 * sizeof(UInt32);
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
  }

  OSStatus SetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData) override {
    switch (inAddress->mSelector) {
      case kAudioStereoPanControlPropertyValue:
        EXPECT(inDataSize == sizeof(Float32),
               BadDataSizeError("Balance SetPropertyData kAudioStereoPanControlPropertyValue"));
        {
          Float32 newValue = *((const Float32*)inData);
          // Clamp to 0.0 (full left) to 1.0 (full right), 0.5 is center
          if (newValue < 0.0f) {
            newValue = 0.0f;
          } else if (newValue > 1.0f) {
            newValue = 1.0f;
          }
          value_ = newValue;
        }
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "SetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    }

    return S_OK;
  }

 private:
  const AudioObjectID ownerId_;
  AudioObjectPropertyScope scope_;
  AudioObjectPropertyElement element_;
  std::atomic<Float32> value_;
};

}  // namespace ProxyAudio

