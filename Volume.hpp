// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <cmath>

#include <atomic>
#include <mutex>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Volume : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Volume(AudioObjectID id,
         AudioObjectRegistry& registry,
         AudioObjectID ownerId,
         AudioObjectPropertyScope scope,
         AudioObjectPropertyElement element)
      : AudioObjectInterface(id, kAudioVolumeControlClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        scope_(scope),
        element_(element),
        scalarValue_(1.0f) {  // Default to 1.0 scalar = 0dB (unity gain)
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);
  }

  Volume(const Volume& other) noexcept = delete;
  Volume(Volume&& other) noexcept = delete;

  ~Volume() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    Boolean result = false;
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioControlPropertyScope:
      case kAudioControlPropertyElement:
      case kAudioLevelControlPropertyScalarValue:
      case kAudioLevelControlPropertyDecibelValue:
      case kAudioLevelControlPropertyDecibelRange:
      case kAudioLevelControlPropertyConvertScalarToDecibels:
      case kAudioLevelControlPropertyConvertDecibelsToScalar:
        result = true;
        break;
      default:
        result = false;
        break;
    }

    return result;
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
      case kAudioLevelControlPropertyDecibelRange:
      case kAudioLevelControlPropertyConvertScalarToDecibels:
      case kAudioLevelControlPropertyConvertDecibelsToScalar:
        *outIsSettable = false;
        break;

      case kAudioLevelControlPropertyScalarValue:
      case kAudioLevelControlPropertyDecibelValue:
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

      case kAudioLevelControlPropertyScalarValue:
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyDecibelValue:
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyDecibelRange:
        *outDataSize = sizeof(AudioValueRange);
        break;

      case kAudioLevelControlPropertyConvertScalarToDecibels:
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyConvertDecibelsToScalar:
        *outDataSize = sizeof(Float32);
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
               BadDataSizeError("Volume kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioLevelControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Volume kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioVolumeControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Volume kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioControlPropertyScope:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyScope),
               BadDataSizeError("Volume kAudioControlPropertyScope"));
        *((AudioObjectPropertyScope*)outData) = scope_;
        *outDataSize = sizeof(AudioObjectPropertyScope);
        break;

      case kAudioControlPropertyElement:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyElement),
               BadDataSizeError("Volume kAudioControlPropertyElement"));
        *((AudioObjectPropertyElement*)outData) = element_;
        *outDataSize = sizeof(AudioObjectPropertyElement);
        break;

      case kAudioLevelControlPropertyScalarValue:
        EXPECT(inDataSize >= sizeof(Float32),
               BadDataSizeError("Volume kAudioLevelControlPropertyScalarValue"));
        *((Float32*)outData) = scalarValue_.load();
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyDecibelValue:
        EXPECT(inDataSize >= sizeof(Float32),
               BadDataSizeError("Volume kAudioLevelControlPropertyDecibelValue"));
        *((Float32*)outData) = ScalarToDecibels(scalarValue_.load());
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyDecibelRange:
        EXPECT(inDataSize >= sizeof(AudioValueRange),
               BadDataSizeError("Volume kAudioLevelControlPropertyDecibelRange"));
        {
          AudioValueRange* range = static_cast<AudioValueRange*>(outData);
          range->mMinimum = MIN_DB;
          range->mMaximum = MAX_DB;
        }
        *outDataSize = sizeof(AudioValueRange);
        break;

      case kAudioLevelControlPropertyConvertScalarToDecibels:
        EXPECT(inDataSize >= sizeof(Float32),
               BadDataSizeError(
                   "Volume kAudioLevelControlPropertyConvertScalarToDecibels"));
        if (inQualifierDataSize == sizeof(Float32)) {
          Float32 scalar = *((const Float32*)inQualifierData);
          *((Float32*)outData) = ScalarToDecibels(scalar);
        } else {
          throw ErrorWithCode(kAudioHardwareBadPropertySizeError,
                              "Invalid qualifier data size");
        }
        *outDataSize = sizeof(Float32);
        break;

      case kAudioLevelControlPropertyConvertDecibelsToScalar:
        EXPECT(inDataSize >= sizeof(Float32),
               BadDataSizeError(
                   "Volume kAudioLevelControlPropertyConvertDecibelsToScalar"));
        if (inQualifierDataSize == sizeof(Float32)) {
          Float32 db = *((const Float32*)inQualifierData);
          *((Float32*)outData) = DecibelsToScalar(db);
        } else {
          throw ErrorWithCode(kAudioHardwareBadPropertySizeError,
                              "Invalid qualifier data size");
        }
        *outDataSize = sizeof(Float32);
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
      case kAudioLevelControlPropertyScalarValue:
        EXPECT(inDataSize == sizeof(Float32),
               BadDataSizeError("Volume SetPropertyData kAudioLevelControlPropertyScalarValue"));
        {
          Float32 newValue = *((const Float32*)inData);
          if (newValue < 0.0f) {
            newValue = 0.0f;
          } else if (newValue > 1.0f) {
            newValue = 1.0f;
          }
          scalarValue_ = newValue;
        }
        break;

      case kAudioLevelControlPropertyDecibelValue:
        EXPECT(inDataSize == sizeof(Float32),
               BadDataSizeError("Volume SetPropertyData kAudioLevelControlPropertyDecibelValue"));
        {
          Float32 db = *((const Float32*)inData);
          if (db < MIN_DB) {
            db = MIN_DB;
          } else if (db > MAX_DB) {
            db = MAX_DB;
          }
          scalarValue_ = DecibelsToScalar(db);
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
  static Float32 ScalarToDecibels(Float32 scalar) {
    // Use squared curve for better UI control
    // dB = MIN_DB + (MAX_DB - MIN_DB) * scalar^2
    if (scalar <= 0.0f) {
      return MIN_DB;
    }
    Float32 squared = scalar * scalar;
    return MIN_DB + (MAX_DB - MIN_DB) * squared;
  }

  static Float32 DecibelsToScalar(Float32 db) {
    // Inverse of ScalarToDecibels
    if (db <= MIN_DB) {
      return 0.0f;
    }
    if (db >= MAX_DB) {
      return 1.0f;
    }
    Float32 normalized = (db - MIN_DB) / (MAX_DB - MIN_DB);
    return std::sqrt(normalized);
  }

  static constexpr Float32 MIN_DB = -96.0f;
  static constexpr Float32 MAX_DB = 0.0f;  // Scalar 1.0 = 0dB (unity gain)

  const AudioObjectID ownerId_;
  AudioObjectPropertyScope scope_;
  AudioObjectPropertyElement element_;
  std::atomic<Float32> scalarValue_;
};

}  // namespace ProxyAudio
