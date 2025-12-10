// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <MacTypes.h>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "AudioObjectInterface.hpp"
#include "AudioObjectRegistry.hpp"
#include "CFStringUtils.hpp"
#include "Constants.hpp"
#include "Error.hpp"

namespace ProxyAudio {

class DataSource : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  DataSource(AudioObjectID id,
             AudioObjectRegistry& registry,
             AudioObjectID ownerId,
             Direction direction)
      : AudioObjectInterface(id, kAudioSelectorControlClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        scope_(DirectionToScope(direction)),
        currentItem_(0) {
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);

    // Initialize with 4 items
    items_.resize(4);
    for (size_t i = 0; i < items_.size(); ++i) {
      items_[i] = "ProxyAudio Source " + std::to_string(i + 1);
    }
  }

  DataSource(const DataSource& other) noexcept = delete;
  DataSource(DataSource&& other) noexcept = delete;

  ~DataSource() = default;

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioControlPropertyScope:
      case kAudioControlPropertyElement:
      case kAudioSelectorControlPropertyCurrentItem:
      case kAudioSelectorControlPropertyAvailableItems:
      case kAudioSelectorControlPropertyItemName:
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
      case kAudioSelectorControlPropertyAvailableItems:
      case kAudioSelectorControlPropertyItemName:
        *outIsSettable = false;
        break;

      case kAudioSelectorControlPropertyCurrentItem:
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

      case kAudioSelectorControlPropertyCurrentItem:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioSelectorControlPropertyAvailableItems:
        *outDataSize = static_cast<UInt32>(items_.size()) * sizeof(UInt32);
        break;

      case kAudioSelectorControlPropertyItemName:
        *outDataSize = sizeof(CFStringRef);
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
               BadDataSizeError("DataSource kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = kAudioControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("DataSource kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = kAudioSelectorControlClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("DataSource kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioControlPropertyScope:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyScope),
               BadDataSizeError("DataSource kAudioControlPropertyScope"));
        *((AudioObjectPropertyScope*)outData) = scope_;
        *outDataSize = sizeof(AudioObjectPropertyScope);
        break;

      case kAudioControlPropertyElement:
        EXPECT(inDataSize >= sizeof(AudioObjectPropertyElement),
               BadDataSizeError("DataSource kAudioControlPropertyElement"));
        *((AudioObjectPropertyElement*)outData) =
            kAudioObjectPropertyElementMain;
        *outDataSize = sizeof(AudioObjectPropertyElement);
        break;

      case kAudioSelectorControlPropertyCurrentItem:
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError(
                   "DataSource kAudioSelectorControlPropertyCurrentItem"));
        *((UInt32*)outData) = currentItem_.load();
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioSelectorControlPropertyAvailableItems: {
        UInt32 numItems = static_cast<UInt32>(items_.size());
        UInt32 itemsToCopy = std::min(
            numItems, static_cast<UInt32>(inDataSize / sizeof(UInt32)));
        EXPECT(inDataSize >= itemsToCopy * sizeof(UInt32),
               BadDataSizeError(
                   "DataSource kAudioSelectorControlPropertyAvailableItems"));
        UInt32* items = static_cast<UInt32*>(outData);
        for (UInt32 i = 0; i < itemsToCopy; ++i) {
          items[i] = static_cast<UInt32>(i);
        }
        *outDataSize = itemsToCopy * sizeof(UInt32);
      } break;

      case kAudioSelectorControlPropertyItemName:
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError(
                   "DataSource kAudioSelectorControlPropertyItemName"));
        if (inQualifierDataSize == sizeof(UInt32)) {
          UInt32 itemIndex = *((const UInt32*)inQualifierData);
          if (itemIndex < items_.size()) {
            *((CFStringRef*)outData) = StringToCFString(items_[itemIndex]);
          } else {
            throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                                "Invalid item index");
          }
        } else {
          throw ErrorWithCode(kAudioHardwareBadPropertySizeError,
                              "Invalid qualifier data size");
        }
        *outDataSize = sizeof(CFStringRef);
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
      case kAudioSelectorControlPropertyCurrentItem:
        EXPECT(inDataSize == sizeof(UInt32),
               BadDataSizeError("DataSource SetPropertyData "
                                "kAudioSelectorControlPropertyCurrentItem"));
        {
          UInt32 newItem = *((const UInt32*)inData);
          if (newItem < items_.size()) {
            UInt32 oldItem = currentItem_.load();
            if (oldItem != newItem) {
              currentItem_ = newItem;

              // Notify about current item change
              AudioObjectPropertyAddress addr;
              addr.mSelector = kAudioSelectorControlPropertyCurrentItem;
              addr.mScope = kAudioObjectPropertyScopeGlobal;
              addr.mElement = kAudioObjectPropertyElementMain;
              changedAddresses.push_back(addr);
            }
          } else {
            throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                                "Invalid item index");
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
  const AudioObjectID ownerId_;
  const AudioObjectPropertyScope scope_;
  std::atomic<UInt32> currentItem_;
  std::vector<std::string> items_;
};

}  // namespace ProxyAudio
