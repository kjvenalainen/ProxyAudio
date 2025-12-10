// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFUUID.h>
#include <MacTypes.h>
#include <mach/kern_return.h>
#include <mach/mach_time.h>

#include <memory>
#include <type_traits>

#include "AudioObjectRegistry.hpp"
#include "Box.hpp"
#include "CFSharedPtr.hpp"
#include "Constants.hpp"
#include "Device.hpp"
#include "Error.hpp"
#include "PlugInDriverInterface.hpp"
#include "Utils.hpp"

// The global AudioServerPlugInDriverInterface object for ProxyAudio.
extern AudioServerPlugInDriverInterface gAudioServerPlugInDriverInterface;
extern AudioServerPlugInDriverRef gAudioServerPlugInDriverRef;

#ifdef __cplusplus
extern "C" {
#endif

void* ProxyDriverInterfaceCreate(CFAllocatorRef inAllocator,
                                 CFUUIDRef inRequestedTypeUUID);

#ifdef __cplusplus
}
#endif

namespace ProxyAudio {

class ProxyDriverInterface : public PlugInDriverInterface<ProxyDriverInterface>,
                             public AudioObjectInterface,
                             public AudioObjectRegistryRef {
 public:
  ProxyDriverInterface(AudioObjectID id, AudioObjectRegistry& registry)
      : AudioObjectInterface(id, kAudioPlugInClassID),
        AudioObjectRegistryRef(registry),
        box_(registry.Construct<Box>(this->Id(),
                                     "ProxyAudio Box",
                                     "ProxyAudioBoxModel",
                                     "ProxyAudio",
                                     "ProxyAudioSerialNumber",
                                     "ProxyAudioFirmwareVersion",
                                     "ProxyAudioBoxUID",
                                     TransportType::Virtual)) {}

  virtual ~ProxyDriverInterface() override = default;

#pragma region PlugInDriverInterface

  HRESULT QueryInterface(REFIID inUUID, LPVOID* outInterface) {
    if (outInterface == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "No place to store the returned interface");
    }

    const auto requestedUUID =
        CFSharedPtr(CFUUIDCreateFromUUIDBytes(NULL, inUUID));

    if (requestedUUID == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "Failed to create the CFUUIDRef");
    }

    if (requestedUUID == IUnknownUUID ||
        requestedUUID == kAudioServerPlugInDriverInterfaceUUID) {
      *outInterface = GetDriverRef();
    } else {
      throw ErrorWithCode(E_NOINTERFACE, "Requested interface not supported");
    }

    return S_OK;
  }

  ULONG AddRef() {
    if (refCount_ < UINT32_MAX) {
      refCount_++;
    }

    return refCount_.load();
  }

  ULONG Release() {
    if (refCount_ > 0) {
      refCount_--;
    }

    return refCount_.load();
  }

  HRESULT Initialize(AudioServerPlugInHostRef inHost) {
    // The job of this method is, as the name implies, to get the
    // driver initialized. One specific thing that needs to be done is to
    // store the AudioServerPlugInHostRef so that it can be used 	later.
    // Note that when this call returns, the HAL will scan the various lists
    // the driver maintains (such as the device list) to get the inital set of
    // objects the driver is 	publishing. So, there is no need to notifiy the
    // HAL about any objects created as part of the execution of this method.
    host_ = inHost;

    // TODO: Load settings using `Host->CopyFromStorage`.

    return S_OK;
  }

  OSStatus CreateDevice(CFDictionaryRef inDescription,
                        const AudioServerPlugInClientInfo* inClientInfo,
                        AudioObjectID* outDeviceObjectID) {
    // This method is used to tell a driver that implements the Transport
    // Manager semantics to create an AudioEndpointDevice from a set of
    // AudioEndpoints. Since this driver is not a Transport Manager, we just
    // check the arguments and return kAudioHardwareUnsupportedOperationError.

    return kAudioHardwareUnsupportedOperationError;
  }

  OSStatus DestroyDevice(AudioObjectID inDeviceObjectID) {
    // This method is used to tell a driver that implements the Transport
    // Manager semantics to destroy an AudioEndpointDevice. Since this driver is
    // not a Transport Manager, we just check the arguments and return
    // kAudioHardwareUnsupportedOperationError.

    return kAudioHardwareUnsupportedOperationError;
  }

  OSStatus AddDeviceClient(AudioObjectID inDeviceObjectID,
                           const AudioServerPlugInClientInfo* inClientInfo) {
    // This method is used to inform the driver about a new client that is using
    // the given device.
    // This allows the device to act differently depending on who the client is.
    // This driver does not need to track the clients using the device, so we
    // just check the arguments and return successfully.

    return GetDevice(inDeviceObjectID)->AddClient(inClientInfo);
  }

  OSStatus RemoveDeviceClient(AudioObjectID inDeviceObjectID,
                              const AudioServerPlugInClientInfo* inClientInfo) {
    // This method is used to inform the driver about a client that is no longer
    // using the given
    // device. This driver does not track clients, so we just check the
    // arguments and return successfully.

    return GetDevice(inDeviceObjectID)->RemoveClient(inClientInfo);
  }

  OSStatus PerformDeviceConfigurationChange(AudioObjectID inDeviceObjectID,
                                            UInt64 inChangeAction,
                                            void* inChangeInfo) {
    // This method is called to tell the device that it can perform the
    // configuation change that it
    // had requested via a call to the host method,
    // RequestDeviceConfigurationChange(). The arguments, inChangeAction and
    // inChangeInfo are the same as what was passed to
    // RequestDeviceConfigurationChange().
    //
    // The HAL guarantees that IO will be stopped while this method is in
    // progress. The HAL will also handle figuring out exactly what changed for
    // the non-control related properties. This means that the only
    // notifications that would need to be sent here would be for either custom
    // properties the HAL doesn't know about or for controls.
    //
    // For the device implemented by this driver, only sample rate changes go
    // through this process as it is the only state that can be changed for the
    // device that isn't a control. For this change, the new sample rate is
    // passed in the inChangeAction argument.

    return GetDevice(inDeviceObjectID)
        ->PerformConfigurationChange(inChangeAction, inChangeInfo);
  }

  OSStatus AbortDeviceConfigurationChange(AudioObjectID inDeviceObjectID,
                                          UInt64 inChangeAction,
                                          void* inChangeInfo) {
    // This method is called to tell the driver that a request for a config
    // change has been denied.
    // This provides the driver an opportunity to clean up any state associated
    // with the request. For this driver, an aborted config change requires no
    // action. So we just check the arguments and return

    return GetDevice(inDeviceObjectID)
        ->AbortConfigurationChange(inChangeAction, inChangeInfo);
  }

  OSStatus StartIO(AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    // This call tells the device that IO is starting for the given client. When
    // this routine
    // returns, the device's clock is running and it is ready to have data
    // read/written. It is important to note that multiple clients can have IO
    // running on the device at the same time. So, work only needs to be done
    // when the first client starts. All subsequent starts simply increment the
    // counter.

    return GetDevice(inDeviceObjectID)->StartIO(inClientID);
  }

  OSStatus StopIO(AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    // This call tells the device that IO is stopping for the given client. When
    // this routine returns, the device's clock is stopped and it is no longer
    // ready to have data read/written. It is important to note that multiple
    // clients can have IO running on the device at the same time. So, work only
    // needs to be done when the last client stops.

    return GetDevice(inDeviceObjectID)->StopIO(inClientID);
  }

  OSStatus GetZeroTimeStamp(AudioObjectID inDeviceObjectID,
                            UInt32 inClientID,
                            Float64* outSampleTime,
                            UInt64* outHostTime,
                            UInt64* outSeed) {
    // This method returns the current zero time stamp for the device. The HAL
    // models the timing of
    // a device as a series of time stamps that relate the sample time to a host
    // time. The zero time stamps are spaced such that the sample times are the
    // value of kAudioDevicePropertyZeroTimeStampPeriod apart. This is often
    // modeled using a ring buffer where the zero time stamp is updated when
    // wrapping around the ring buffer.
    //
    // For this device, the zero time stamps' sample time increments every
    // kDevice_RingBufferSize frames and the host time increments by
    // kDevice_RingBufferSize * gDevice_HostTicksPerFrame.

    return GetDevice(inDeviceObjectID)
        ->GetZeroTimeStamp(inClientID, outSampleTime, outHostTime, outSeed);
  }

  OSStatus WillDoIOOperation(AudioObjectID inDeviceObjectID,
                             UInt32 inClientID,
                             UInt32 inOperationID,
                             Boolean* outWillDo,
                             Boolean* outWillDoInPlace) {
    return GetDevice(inDeviceObjectID)
        ->WillDoIOOperation(inClientID, inOperationID, outWillDo,
                            outWillDoInPlace);
  }

  OSStatus BeginIOOperation(AudioObjectID inDeviceObjectID,
                            UInt32 inClientID,
                            UInt32 inOperationID,
                            UInt32 inIOBufferFrameSize,
                            const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return GetDevice(inDeviceObjectID)
        ->BeginIOOperation(inClientID, inOperationID, inIOBufferFrameSize,
                           inIOCycleInfo);
  }

  OSStatus DoIOOperation(AudioObjectID inDeviceObjectID,
                         AudioObjectID inStreamObjectID,
                         UInt32 inClientID,
                         UInt32 inOperationID,
                         UInt32 inIOBufferFrameSize,
                         const AudioServerPlugInIOCycleInfo* inIOCycleInfo,
                         void* ioMainBuffer,
                         void* ioSecondaryBuffer) {
    return GetDevice(inDeviceObjectID)
        ->DoIOOperation(inStreamObjectID, inClientID, inOperationID,
                        inIOBufferFrameSize, inIOCycleInfo, ioMainBuffer,
                        ioSecondaryBuffer);
  }

  OSStatus EndIOOperation(AudioObjectID inDeviceObjectID,
                          UInt32 inClientID,
                          UInt32 inOperationID,
                          UInt32 inIOBufferFrameSize,
                          const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return GetDevice(inDeviceObjectID)
        ->EndIOOperation(inClientID, inOperationID, inIOBufferFrameSize,
                         inIOCycleInfo);
  }

#pragma endregion

#pragma region AudioObjectInterface

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) override {
    if (inAddress == nullptr) {
      return false;
    }

    if (Id() != kAudioObjectPlugInObject) {
      return false;
    }

    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyManufacturer:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioPlugInPropertyBoxList:
      case kAudioPlugInPropertyTranslateUIDToBox:
      case kAudioPlugInPropertyDeviceList:
      case kAudioPlugInPropertyTranslateUIDToDevice:
      case kAudioPlugInPropertyResourceBundle:
      case kAudioObjectPropertyCustomPropertyInfoList:
        return true;
      default:
        return false;
    }
  }

  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) override {
    if (inAddress == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "IsPropertySettable: no address");
    }
    if (outIsSettable == nullptr) {
      throw ErrorWithCode(
          kAudioHardwareIllegalOperationError,
          "IsPropertySettable: no place to put the return value");
    }
    if (Id() != kAudioObjectPlugInObject) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "IsPropertySettable: not the plug-in object");
    }

    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyManufacturer:
      case kAudioObjectPropertyOwnedObjects:
      case kAudioPlugInPropertyBoxList:
      case kAudioPlugInPropertyTranslateUIDToBox:
      case kAudioPlugInPropertyDeviceList:
      case kAudioPlugInPropertyTranslateUIDToDevice:
      case kAudioPlugInPropertyResourceBundle:
      case kAudioObjectPropertyCustomPropertyInfoList:
        *outIsSettable = false;
        return S_OK;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "IsPropertySettable: unknown property");
    }
  }

  OSStatus GetPropertyDataSize(pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize) override {
    if (inAddress == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "GetPropertyDataSize: no address");
    }
    if (outDataSize == nullptr) {
      throw ErrorWithCode(
          kAudioHardwareIllegalOperationError,
          "GetPropertyDataSize: no place to put the return value");
    }
    if (Id() != kAudioObjectPlugInObject) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "GetPropertyDataSize: not the plug-in object");
    }

    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        *outDataSize = sizeof(AudioClassID);
        return S_OK;

      case kAudioObjectPropertyClass:
        *outDataSize = sizeof(AudioClassID);
        return S_OK;

      case kAudioObjectPropertyOwner:
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioObjectPropertyManufacturer:
        *outDataSize = sizeof(CFStringRef);
        return S_OK;

      case kAudioObjectPropertyOwnedObjects: {
        const bool boxAcquired = box_->Acquired();
        *outDataSize = boxAcquired
                           ? sizeof(AudioObjectID) +
                                 box_->Devices().size() * sizeof(AudioObjectID)
                           : sizeof(AudioObjectID);
        return S_OK;
      }

      case kAudioPlugInPropertyBoxList:
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioPlugInPropertyTranslateUIDToBox:
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioPlugInPropertyDeviceList: {
        const bool boxAcquired = box_->Acquired();
        *outDataSize =
            boxAcquired ? box_->Devices().size() * sizeof(AudioObjectID) : 0;
        return S_OK;
      }

      case kAudioPlugInPropertyTranslateUIDToDevice:
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioPlugInPropertyResourceBundle:
        *outDataSize = sizeof(CFStringRef);
        return S_OK;

      case kAudioObjectPropertyCustomPropertyInfoList:
        *outDataSize = sizeof(AudioServerPlugInCustomPropertyInfo);
        return S_OK;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyDataSize: unknown property");
    }
  }

  OSStatus GetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData) override {
    if (inAddress == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "GetPropertyData: no address");
    }
    if (outDataSize == nullptr) {
      throw ErrorWithCode(
          kAudioHardwareIllegalOperationError,
          "GetPropertyData: no place to put the return value size");
    }
    if (outData == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "GetPropertyData: no place to put the return value");
    }
    if (Id() != kAudioObjectPlugInObject) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "GetPropertyData: not the plug-in object");
    }

    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        if (inDataSize < sizeof(AudioClassID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioObjectPropertyBaseClass");
        }
        *static_cast<AudioClassID*>(outData) = kAudioObjectClassID;
        *outDataSize = sizeof(AudioClassID);
        return S_OK;

      case kAudioObjectPropertyClass:
        if (inDataSize < sizeof(AudioClassID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioObjectPropertyClass");
        }
        *static_cast<AudioClassID*>(outData) = kAudioPlugInClassID;
        *outDataSize = sizeof(AudioClassID);
        return S_OK;

      case kAudioObjectPropertyOwner:
        if (inDataSize < sizeof(AudioObjectID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioObjectPropertyOwner");
        }
        *static_cast<AudioObjectID*>(outData) = kAudioObjectUnknown;
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioObjectPropertyManufacturer:
        if (inDataSize < sizeof(CFStringRef)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioObjectPropertyManufacturer");
        }
        *static_cast<CFStringRef*>(outData) = CFSTR("Apple Inc.");
        *outDataSize = sizeof(CFStringRef);
        return S_OK;

      case kAudioObjectPropertyOwnedObjects: {
        const bool boxAcquired = box_->Acquired();
        const auto& devices = box_->Devices();

        // The box is always owned, and if it's acquired then it may have
        // devices.
        const UInt32 maxItems = boxAcquired ? (1 + devices.size()) : 1;
        UInt32 numberItemsToFetch = inDataSize / sizeof(AudioObjectID);
        if (numberItemsToFetch > maxItems) {
          numberItemsToFetch = maxItems;
        }

        if (numberItemsToFetch > 0) {
          static_cast<AudioObjectID*>(outData)[0] = box_->Id();
        }

        size_t i = 0;
        for (; i < devices.size() && i < numberItemsToFetch - 1; i++) {
          static_cast<AudioObjectID*>(outData)[i + 1] = devices[i]->Id();
        }

        *outDataSize = (i + 1) * sizeof(AudioObjectID);
        return S_OK;
      }

      case kAudioPlugInPropertyBoxList: {
        if (inDataSize < sizeof(AudioObjectID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioPlugInPropertyBoxList");
        }

        static_cast<AudioObjectID*>(outData)[0] = box_->Id();
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;
      }

      case kAudioPlugInPropertyTranslateUIDToBox:
        if (inDataSize < sizeof(AudioObjectID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioPlugInPropertyTranslateUIDToBox");
        }
        if (inQualifierDataSize != sizeof(CFStringRef)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: the qualifier is the wrong size for "
              "kAudioPlugInPropertyTranslateUIDToBox");
        }
        if (inQualifierData == nullptr) {
          throw ErrorWithCode(kAudioHardwareBadPropertySizeError,
                              "GetPropertyData: no qualifier for "
                              "kAudioPlugInPropertyTranslateUIDToBox");
        }
        // TODO: Implement UID matching
        *static_cast<AudioObjectID*>(outData) = kAudioObjectUnknown;
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioPlugInPropertyDeviceList: {
        const bool boxAcquired = box_->Acquired();
        const auto& devices = box_->Devices();

        if (!boxAcquired) {
          *outDataSize = 0;
          return S_OK;
        }

        UInt32 numberItemsToFetch = inDataSize / sizeof(AudioObjectID);

        size_t i = 0;
        for (; i < devices.size() && i < numberItemsToFetch; i++) {
          static_cast<AudioObjectID*>(outData)[i] = devices[i]->Id();
        }

        *outDataSize = i * sizeof(AudioObjectID);
        return S_OK;
      }

      case kAudioPlugInPropertyTranslateUIDToDevice:
        if (inDataSize < sizeof(AudioObjectID)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioPlugInPropertyTranslateUIDToDevice");
        }
        if (inQualifierDataSize != sizeof(CFStringRef)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: the qualifier is the wrong size for "
              "kAudioPlugInPropertyTranslateUIDToDevice");
        }
        if (inQualifierData == nullptr) {
          throw ErrorWithCode(kAudioHardwareBadPropertySizeError,
                              "GetPropertyData: no qualifier for "
                              "kAudioPlugInPropertyTranslateUIDToDevice");
        }
        // TODO: Implement UID matching
        *static_cast<AudioObjectID*>(outData) = kAudioObjectUnknown;
        *outDataSize = sizeof(AudioObjectID);
        return S_OK;

      case kAudioPlugInPropertyResourceBundle:
        if (inDataSize < sizeof(CFStringRef)) {
          throw ErrorWithCode(
              kAudioHardwareBadPropertySizeError,
              "GetPropertyData: not enough space for the return value of "
              "kAudioPlugInPropertyResourceBundle");
        }
        *static_cast<CFStringRef*>(outData) = CFSTR("");
        *outDataSize = sizeof(CFStringRef);
        return S_OK;

      case kAudioObjectPropertyCustomPropertyInfoList:
        *outDataSize = 0;
        return S_OK;

      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "GetPropertyData: unknown property");
    }
  }

  OSStatus SetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData) override {
    if (inAddress == nullptr) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "SetPropertyData: no address");
    }
    if (Id() != kAudioObjectPlugInObject) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "SetPropertyData: not the plug-in object");
    }

    switch (inAddress->mSelector) {
      default:
        throw ErrorWithCode(kAudioHardwareUnknownPropertyError,
                            "SetPropertyData: unknown property");
    }
  }

#pragma endregion

 private:
  std::shared_ptr<Device> GetDevice(const AudioObjectID inDeviceObjectID) {
    auto devicePtr = GetRegistry()[inDeviceObjectID];
    if (devicePtr == nullptr || devicePtr->ClassId() != kAudioDeviceClassID) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "Invalid device object ID [id:" +
                              std::to_string(inDeviceObjectID) + "]");
    }

    return std::static_pointer_cast<Device>(devicePtr);
  }

  std::atomic<ULONG> refCount_ = 1U;
  AudioServerPlugInHostRef host_ = nullptr;
  std::shared_ptr<Box> box_;
};

}  // namespace ProxyAudio
