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

class Mute : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Mute(AudioObjectID id,
       AudioObjectRegistry& registry,
       AudioObjectID ownerId,
       AudioObjectPropertyScope scope,
       AudioObjectPropertyElement element)
      : AudioObjectInterface(id, kAudioBooleanControlClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        scope_(scope),
        element_(element),
        value_(false) {
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);
  }

  Mute(const Mute& other) noexcept = delete;
  Mute(Mute&& other) noexcept = delete;

  ~Mute() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioControlPropertyScope:
      case kAudioControlPropertyElement:
      case kAudioBooleanControlPropertyValue:
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
        *outIsSettable = false;
        break;

      case kAudioBooleanControlPropertyValue:
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

      case kAudioBooleanControlPropertyValue:
        *outDataSize = sizeof(UInt32);
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
               BadDataSizeError("Mute kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Mute kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioBooleanControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Mute kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioControlPropertyScope:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyScope),
               BadDataSizeError("Mute kAudioControlPropertyScope"));
        *((AudioObjectPropertyScope*)outData) = scope_;
        *outDataSize = sizeof(AudioObjectPropertyScope);
        break;

      case kAudioControlPropertyElement:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyElement),
               BadDataSizeError("Mute kAudioControlPropertyElement"));
        *((AudioObjectPropertyElement*)outData) = element_;
        *outDataSize = sizeof(AudioObjectPropertyElement);
        break;

      case kAudioBooleanControlPropertyValue:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Mute kAudioBooleanControlPropertyValue"));
        *((UInt32*)outData) = value_.load() ? 1 : 0;
        *outDataSize = sizeof(UInt32);
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
      case kAudioBooleanControlPropertyValue:
        EXPECT(inDataSize == sizeof(UInt32),
               BadDataSizeError(
                   "Mute SetPropertyData kAudioBooleanControlPropertyValue"));
        value_ = (*((const UInt32*)inData) != 0);
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
  std::atomic<bool> value_;
};

}  // namespace ProxyAudio
