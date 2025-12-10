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
#include "CFStringUtils.hpp"
#include "Constants.hpp"
#include "Device.hpp"
#include "Error.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

class Box : public AudioObjectInterface, public AudioObjectRegistryRef {
 public:
  Box(AudioObjectID id,
      AudioObjectRegistry& registry,
      AudioObjectID ownerId,
      const std::string& name,
      const std::string& modelName,
      const std::string& manufacturerName,
      const std::string& serialNumber,
      const std::string& firmwareVersion,
      const std::string& boxUID,
      TransportType transportType)
      : AudioObjectInterface(id, kAudioBoxClassID),
        AudioObjectRegistryRef(registry),
        ownerId_(ownerId),
        name_(name),
        modelName_(modelName),
        manufacturerName_(manufacturerName),
        serialNumber_(serialNumber),
        firmwareVersion_(firmwareVersion),
        boxUID_(boxUID),
        transportType_(transportType),
        devices_() {
    Log("constructor [id: %d, ownerId: %d]", id, ownerId);

    // Temporary: Add a single device to the box. Eventually we will create
    // these dynamically. Devices have the same owner as the box.
    devices_.push_back(registry.Construct<Device>(ownerId_, "ProxyAudio Device",
                                                  "ProxyAudioDeviceUID"));
  }

  Box(const Box& other) noexcept = delete;
  Box(Box&& other) noexcept = delete;

  ~Box() = default;

  const bool Acquired() const { return acquired_; }

  void SetAcquired(bool acquired) { acquired_ = acquired; }

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyName:
      case kAudioObjectPropertyModelName:
      case kAudioObjectPropertyManufacturer:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioObjectPropertyIdentify:
      case kAudioObjectPropertySerialNumber:
      case kAudioObjectPropertyFirmwareVersion:
      case kAudioBoxPropertyBoxUID:
      case kAudioBoxPropertyTransportType:
      case kAudioBoxPropertyHasAudio:
      case kAudioBoxPropertyHasVideo:
      case kAudioBoxPropertyHasMIDI:
      case kAudioBoxPropertyIsProtected:
      case kAudioBoxPropertyAcquired:
      case kAudioBoxPropertyAcquisitionFailed:
      case kAudioBoxPropertyDeviceList:
        return true;
      default:
        return false;
    };
  }

  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyModelName:
      case kAudioObjectPropertyManufacturer:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioObjectPropertySerialNumber:
      case kAudioObjectPropertyFirmwareVersion:
      case kAudioBoxPropertyBoxUID:
      case kAudioBoxPropertyTransportType:
      case kAudioBoxPropertyHasAudio:
      case kAudioBoxPropertyHasVideo:
      case kAudioBoxPropertyHasMIDI:
      case kAudioBoxPropertyIsProtected:
      case kAudioBoxPropertyAcquisitionFailed:
      case kAudioBoxPropertyDeviceList:
        *outIsSettable = false;
        break;

      case kAudioObjectPropertyName:
      case kAudioObjectPropertyIdentify:
      case kAudioBoxPropertyAcquired:
        *outIsSettable = true;
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "IsPropertySettable: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    };

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

      case kAudioObjectPropertyModelName:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyManufacturer:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyOwnedObjects:
        *outDataSize = 0;
        break;

      case kAudioObjectPropertyIdentify:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioObjectPropertySerialNumber:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyFirmwareVersion:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioBoxPropertyBoxUID:
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioBoxPropertyTransportType:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasAudio:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasVideo:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasMIDI:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyIsProtected:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyAcquired:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyAcquisitionFailed:
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyDeviceList:
        *outDataSize = acquired_ ? static_cast<UInt32>(devices_.size()) *
                                       sizeof(AudioObjectID)
                                 : 0;
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyDataSize: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
        break;
    };

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
        // The base class for kAudioBoxClassID is kAudioObjectClassID
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Box kAudioObjectPropertyBaseClass"));

        *((AudioClassID*)outData) = kAudioObjectClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyClass:
        // The class is always kAudioBoxClassID for regular drivers
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Box kAudioObjectPropertyClass"));

        *((AudioClassID*)outData) = kAudioBoxClassID;
        *outDataSize = sizeof(AudioClassID);
        break;

      case kAudioObjectPropertyOwner:
        // The owner is the plug-in object
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Box kAudioObjectPropertyOwner"));

        *((AudioObjectID*)outData) = ownerId_;
        *outDataSize = sizeof(AudioObjectID);
        break;

      case kAudioObjectPropertyName:
        // This is the human readable name of the maker of the box.
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyName"));

        *((CFStringRef*)outData) = StringToCFString(name_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyModelName:
        // This is the human readable name of the maker of the box.
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyModelName"));

        *((CFStringRef*)outData) = StringToCFString(modelName_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyManufacturer:
        // This is the human readable name of the maker of the box.
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyManufacturer"));

        *((CFStringRef*)outData) = StringToCFString(manufacturerName_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyOwnedObjects:
        // This returns the objects directly owned by the object. Boxes don't
        // own anything in this API (internally boxes do in fact own devices).
        *outDataSize = 0;
        break;

      case kAudioObjectPropertyIdentify:
        // This is used to highling the device in the UI, but it's value has no
        // meaning
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioObjectPropertyIdentify"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioObjectPropertySerialNumber:
        // This is the human readable serial number of the box.
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertySerialNumber"));

        *((CFStringRef*)outData) = StringToCFString(serialNumber_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioObjectPropertyFirmwareVersion:
        // This is the human readable firmware version of the box.
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyFirmwareVersion"));

        *((CFStringRef*)outData) = StringToCFString(firmwareVersion_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioBoxPropertyBoxUID:
        // Boxes have UIDs the same as devices
        EXPECT(inDataSize >= sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyBoxUID"));

        *((CFStringRef*)outData) = StringToCFString(boxUID_);
        *outDataSize = sizeof(CFStringRef);
        break;

      case kAudioBoxPropertyTransportType:
        // This value represents how the device is attached to the system. This
        // can be any 32 bit integer, but common values for this property are
        // defined in <CoreAudio/AudioHardwareBase.h>
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyTransportType"));

        *((UInt32*)outData) = static_cast<UInt32>(transportType_);
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasAudio:
        // Indicates whether or not the box has audio capabilities
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyHasAudio"));

        *((UInt32*)outData) = 1;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasVideo:
        // Indicates whether or not the box has video capabilities
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyHasVideo"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyHasMIDI:
        // Indicates whether or not the box has MIDI capabilities
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyHasMIDI"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyIsProtected:
        // Indicates whether or not the box has requires authentication to use
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyIsProtected"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyAcquired:
        // When set to a non-zero value, the device is acquired for use by the
        // local machine
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyAcquired"));
        *((UInt32*)outData) = acquired_ ? 1 : 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyAcquisitionFailed:
        // This is used for notifications to say when an attempt to acquire a
        // device has failed.
        EXPECT(inDataSize >= sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyAcquisitionFailed"));
        *((UInt32*)outData) = 0;
        *outDataSize = sizeof(UInt32);
        break;

      case kAudioBoxPropertyDeviceList:
        // This is used to indicate which devices came from this box
        if (acquired_) {
          const auto numDevicesToFetch =
              std::min(inDataSize / sizeof(AudioObjectID), devices_.size());

          for (size_t i = 0; i < numDevicesToFetch; i++) {
            static_cast<AudioObjectID*>(outData)[i] = devices_[i]->Id();
          }

          *outDataSize =
              static_cast<UInt32>(numDevicesToFetch * sizeof(AudioObjectID));
        } else {
          *outDataSize = 0;
        }
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    };

    return S_OK;
  }

  OSStatus SetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData) override {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyName:
        // Allow setting the box name
        EXPECT(inDataSize == sizeof(CFStringRef),
               BadDataSizeError("Box kAudioObjectPropertyName"));
        name_ = CFStringToString(*((CFStringRef*)inData));
        break;

      case kAudioObjectPropertyIdentify:
        // This property is used to trigger identification of the device in the
        // UI We accept the value but don't need to do anything with it
        EXPECT(inDataSize == sizeof(UInt32),
               BadDataSizeError("Box kAudioObjectPropertyIdentify"));
        break;

      case kAudioBoxPropertyAcquired:
        // Setting acquired state
        EXPECT(inDataSize == sizeof(UInt32),
               BadDataSizeError("Box kAudioBoxPropertyAcquired"));
        acquired_ = (*((UInt32*)inData) != 0);
        break;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "SetPropertyData: unknown property [" +
                                std::to_string(inAddress->mSelector) + "]");
    };

    return S_OK;
  }

  // Readonly access to the devices.
  const std::vector<std::shared_ptr<Device>>& Devices() const {
    return devices_;
  }

 private:
  const AudioObjectID ownerId_;
  std::string name_;
  std::string modelName_;
  std::string manufacturerName_;
  std::string serialNumber_;
  std::string firmwareVersion_;
  std::string boxUID_;
  TransportType transportType_;
  std::atomic<bool> acquired_ = true;
  std::vector<std::shared_ptr<Device>> devices_;
};

}  // namespace ProxyAudio
