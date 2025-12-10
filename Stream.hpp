// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include <atomic>
#include <memory>
#include <string>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "CFStringUtils.hpp"
#include "Constants.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class Stream : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Stream(AudioObjectID id,
         AudioObjectRegistry& registry,
         AudioObjectID ownerId,
         Direction direction,
         TerminalType terminalType)
      : AudioObjectInterface(id, kAudioStreamClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        direction_(direction),
        terminalType_(terminalType),
        isActive_(true),
        sampleRate_(48000.0),
        format_() {
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);

    // Initialize format: 2-channel, 32-bit float, LPCM
    format_.mSampleRate = sampleRate_;
    format_.mFormatID = kAudioFormatLinearPCM;
    format_.mFormatFlags =
        kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
        kAudioFormatFlagIsNonInterleaved;
    format_.mBytesPerPacket = sizeof(Float32);
    format_.mFramesPerPacket = 1;
    format_.mBytesPerFrame = sizeof(Float32);
    format_.mChannelsPerFrame = 2;
    format_.mBitsPerChannel = 32;
  }

  Stream(const Stream& other) noexcept = delete;
  Stream(Stream&& other) noexcept = delete;

  ~Stream() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioObjectPropertyName:
      case kAudioStreamPropertyIsActive:
      case kAudioStreamPropertyDirection:
      case kAudioStreamPropertyTerminalType:
      case kAudioStreamPropertyStartingChannel:
      case kAudioStreamPropertyLatency:
      case kAudioStreamPropertyVirtualFormat:
      case kAudioStreamPropertyPhysicalFormat:
      case kAudioStreamPropertyAvailableVirtualFormats:
      case kAudioStreamPropertyAvailablePhysicalFormats:
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
      case kAudioObjectPropertyName:
      case kAudioStreamPropertyDirection:
      case kAudioStreamPropertyTerminalType:
      case kAudioStreamPropertyStartingChannel:
      case kAudioStreamPropertyLatency:
      case kAudioStreamPropertyAvailableVirtualFormats:
      case kAudioStreamPropertyAvailablePhysicalFormats:
        *outIsSettable = false;
        break;

      case kAudioStreamPropertyIsActive:
      case kAudioStreamPropertyVirtualFormat:
      case kAudioStreamPropertyPhysicalFormat:
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

      case kAudioObjectPropertyName:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioStreamPropertyIsActive:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyDirection:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyTerminalType:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyStartingChannel:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyLatency:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyVirtualFormat:
        *outDataSize = sizeof(AudioStreamBasicDescription);
        break;

      case kAudioStreamPropertyPhysicalFormat:
        *outDataSize = sizeof(AudioStreamBasicDescription);
        break;

      case kAudioStreamPropertyAvailableVirtualFormats:
        *outDataSize = 2 * sizeof(AudioStreamRangedDescription);
        break;

      case kAudioStreamPropertyAvailablePhysicalFormats:
        *outDataSize = 2 * sizeof(AudioStreamRangedDescription);
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
               BadDataSizeError("Stream kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioObjectClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Stream kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioStreamClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Stream kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioObjectPropertyName:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Stream kAudioObjectPropertyName"));
        {
          std::string name = (direction_ == Direction::Input) ? "Input Stream"
                                                               : "Output Stream";
          *((CFStringRef*)outData) = StringToCFString(name);
        }
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioStreamPropertyIsActive:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Stream kAudioStreamPropertyIsActive"));
        *((UInt32*)outData) = isActive_ ? 1 : 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyDirection:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Stream kAudioStreamPropertyDirection"));
        *((UInt32*)outData) = static_cast<UInt32>(direction_);
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyTerminalType:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Stream kAudioStreamPropertyTerminalType"));
        *((UInt32*)outData) = static_cast<UInt32>(terminalType_);
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyStartingChannel:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Stream kAudioStreamPropertyStartingChannel"));
        *((UInt32*)outData) = 1;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyLatency:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Stream kAudioStreamPropertyLatency"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioStreamPropertyVirtualFormat:
        EXPECT(inDataSize >= sizeof(AudioStreamBasicDescription),
               BadDataSizeError("Stream kAudioStreamPropertyVirtualFormat"));
        *((AudioStreamBasicDescription*)outData) = format_;
        *outDataSize = sizeof(AudioStreamBasicDescription);
        break;

      case kAudioStreamPropertyPhysicalFormat:
        EXPECT(inDataSize >= sizeof(AudioStreamBasicDescription),
               BadDataSizeError("Stream kAudioStreamPropertyPhysicalFormat"));
        *((AudioStreamBasicDescription*)outData) = format_;
        *outDataSize = sizeof(AudioStreamBasicDescription);
        break;

      case kAudioStreamPropertyAvailableVirtualFormats:
        EXPECT(inDataSize >= 2 * sizeof(AudioStreamRangedDescription),
               BadDataSizeError(
                   "Stream kAudioStreamPropertyAvailableVirtualFormats"));
        GetAvailableFormats(static_cast<AudioStreamRangedDescription*>(outData));
        *outDataSize = 2 * sizeof(AudioStreamRangedDescription);
        break;

      case kAudioStreamPropertyAvailablePhysicalFormats:
        EXPECT(inDataSize >= 2 * sizeof(AudioStreamRangedDescription),
               BadDataSizeError(
                   "Stream kAudioStreamPropertyAvailablePhysicalFormats"));
        GetAvailableFormats(static_cast<AudioStreamRangedDescription*>(outData));
        *outDataSize = 2 * sizeof(AudioStreamRangedDescription);
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
      case kAudioStreamPropertyIsActive:
        EXPECT(inDataSize == sizeof(UInt32),
               BadDataSizeError("Stream SetPropertyData kAudioStreamPropertyIsActive"));
        isActive_ = (*((const UInt32*)inData) != 0);
        break;

      case kAudioStreamPropertyVirtualFormat:
      case kAudioStreamPropertyPhysicalFormat:
        EXPECT(inDataSize == sizeof(AudioStreamBasicDescription),
               BadDataSizeError("Stream SetPropertyData format"));
        {
          const AudioStreamBasicDescription* newFormat =
              static_cast<const AudioStreamBasicDescription*>(inData);
          // Validate format
          if (newFormat->mFormatID == kAudioFormatLinearPCM &&
              newFormat->mChannelsPerFrame == 2 &&
              (newFormat->mSampleRate == 44100.0 ||
               newFormat->mSampleRate == 48000.0)) {
            format_ = *newFormat;
            sampleRate_ = format_.mSampleRate;
          } else {
            throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                                "Unsupported stream format");
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

 private:
  void GetAvailableFormats(AudioStreamRangedDescription* formats) const {
    // Format 1: 44.1kHz
    formats[0].mFormat.mSampleRate = 44100.0;
    formats[0].mFormat.mFormatID = kAudioFormatLinearPCM;
    formats[0].mFormat.mFormatFlags =
        kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
        kAudioFormatFlagIsNonInterleaved;
    formats[0].mFormat.mBytesPerPacket = sizeof(Float32);
    formats[0].mFormat.mFramesPerPacket = 1;
    formats[0].mFormat.mBytesPerFrame = sizeof(Float32);
    formats[0].mFormat.mChannelsPerFrame = 2;
    formats[0].mFormat.mBitsPerChannel = 32;
    formats[0].mSampleRateRange.mMinimum = 44100.0;
    formats[0].mSampleRateRange.mMaximum = 44100.0;

    // Format 2: 48kHz
    formats[1].mFormat.mSampleRate = 48000.0;
    formats[1].mFormat.mFormatID = kAudioFormatLinearPCM;
    formats[1].mFormat.mFormatFlags =
        kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
        kAudioFormatFlagIsNonInterleaved;
    formats[1].mFormat.mBytesPerPacket = sizeof(Float32);
    formats[1].mFormat.mFramesPerPacket = 1;
    formats[1].mFormat.mBytesPerFrame = sizeof(Float32);
    formats[1].mFormat.mChannelsPerFrame = 2;
    formats[1].mFormat.mBitsPerChannel = 32;
    formats[1].mSampleRateRange.mMinimum = 48000.0;
    formats[1].mSampleRateRange.mMaximum = 48000.0;
  }

  const AudioObjectID ownerId_;
  Direction direction_;
  TerminalType terminalType_;
  std::atomic<bool> isActive_;
  double sampleRate_;
  AudioStreamBasicDescription format_;
};

}  // namespace ProxyAudio
