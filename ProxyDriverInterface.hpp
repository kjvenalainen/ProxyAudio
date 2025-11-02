// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CFUUID.h>
#include <MacTypes.h>
#include <mach/kern_return.h>
#include <mach/mach_time.h>

#include <type_traits>

#include "AudioObjectRegistry.hpp"
#include "Box.hpp"
#include "CFSharedPtr.hpp"
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

constexpr float SAMPLE_RATE = 48000.0f;

class ProxyDriverInterface : public PlugInDriverInterface<ProxyDriverInterface>,
                             public AudioObjectInterface {
 public:
  ProxyDriverInterface(AudioObjectID id)
      : AudioObjectInterface(id, kAudioPlugInClassID) {}

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

    // TODO: Remove this.
    gAudioServerPlugInDriverInterface.QueryInterface(
        gAudioServerPlugInDriverRef, inUUID, outInterface);

    if (requestedUUID == IUnknownUUID ||
        requestedUUID == kAudioServerPlugInDriverInterfaceUUID) {
      *outInterface = GetDriverRef();
    } else {
      throw ErrorWithCode(E_NOINTERFACE, "Requested interface not supported");
    }

    return S_OK;
  }

  ULONG AddRef() {
    gAudioServerPlugInDriverInterface.AddRef(gAudioServerPlugInDriverRef);

    if (refCount_ < UINT32_MAX) {
      refCount_++;
    }

    return refCount_.load();
  }

  ULONG Release() {
    gAudioServerPlugInDriverInterface.Release(gAudioServerPlugInDriverRef);

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

    // TODO: Do we really need a box acquired property?

    // TODO: Load settings using `Host->CopyFromStorage`.

    // Calculate the host ticks per frame
    struct mach_timebase_info timebaseInfo;
    if (KERN_SUCCESS != mach_timebase_info(&timebaseInfo)) {
      throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                          "Failed to get mach_timebase_info");
    }

    const auto theHostClockFrequency = static_cast<double>(timebaseInfo.denom) /
                                       static_cast<double>(timebaseInfo.numer) *
                                       1000000000.0;
    hostTicksPerFrame_ = theHostClockFrequency / SAMPLE_RATE;

    // TODO: Remove this.
    gAudioServerPlugInDriverInterface.Initialize(gAudioServerPlugInDriverRef,
                                                 inHost);

    Log("Initialize Success");

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

    // TODO: Validate that the inDeviceObjectID corresponds to one of our
    // devices.

    return S_OK;
  }

  OSStatus RemoveDeviceClient(AudioObjectID inDeviceObjectID,
                              const AudioServerPlugInClientInfo* inClientInfo) {
    // This method is used to inform the driver about a client that is no longer
    // using the given
    // device. This driver does not track clients, so we just check the
    // arguments and return successfully.

    // TODO: Validate that the inDeviceObjectID corresponds to one of our
    // devices.

    return S_OK;
  }

  OSStatus PerformDeviceConfigurationChange(AudioObjectID inDeviceObjectID,
                                            UInt64 inChangeAction,
                                            void* inChangeInfo) {
    return gAudioServerPlugInDriverInterface.PerformDeviceConfigurationChange(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inChangeAction,
        inChangeInfo);
  }

  OSStatus AbortDeviceConfigurationChange(AudioObjectID inDeviceObjectID,
                                          UInt64 inChangeAction,
                                          void* inChangeInfo) {
    return gAudioServerPlugInDriverInterface.AbortDeviceConfigurationChange(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inChangeAction,
        inChangeInfo);
  }

  Boolean HasProperty(AudioObjectID inObjectID,
                      pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) {
    return gAudioServerPlugInDriverInterface.HasProperty(
        gAudioServerPlugInDriverRef, inObjectID, inClientProcessID, inAddress);
  }

  OSStatus IsPropertySettable(AudioObjectID inObjectID,
                              pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) {
    return gAudioServerPlugInDriverInterface.IsPropertySettable(
        gAudioServerPlugInDriverRef, inObjectID, inClientProcessID, inAddress,
        outIsSettable);
  }

  OSStatus GetPropertyDataSize(AudioObjectID inObjectID,
                               pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize) {
    return gAudioServerPlugInDriverInterface.GetPropertyDataSize(
        gAudioServerPlugInDriverRef, inObjectID, inClientProcessID, inAddress,
        inQualifierDataSize, inQualifierData, outDataSize);
  }

  OSStatus GetPropertyData(AudioObjectID inObjectID,
                           pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData) {
    return gAudioServerPlugInDriverInterface.GetPropertyData(
        gAudioServerPlugInDriverRef, inObjectID, inClientProcessID, inAddress,
        inQualifierDataSize, inQualifierData, inDataSize, outDataSize, outData);
  }

  OSStatus SetPropertyData(AudioObjectID inObjectID,
                           pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           const void* inData) {
    return gAudioServerPlugInDriverInterface.SetPropertyData(
        gAudioServerPlugInDriverRef, inObjectID, inClientProcessID, inAddress,
        inQualifierDataSize, inQualifierData, inDataSize, inData);
  }

  OSStatus StartIO(AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    // This call tells the device that IO is starting for the given client. When
    // this routine
    // returns, the device's clock is running and it is ready to have data
    // read/written. It is important to note that multiple clients can have IO
    // running on the device at the same time. So, work only needs to be done
    // when the first client starts. All subsequent starts simply increment the
    // counter.

    // TODO: Remove this.
    gAudioServerPlugInDriverInterface.StartIO(gAudioServerPlugInDriverRef,
                                              inDeviceObjectID, inClientID);

    auto devicePtr = GetRegistry()[inDeviceObjectID];
    if (devicePtr == nullptr || devicePtr->ClassId() != kAudioDeviceClassID) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "Invalid device object ID [id:" +
                              std::to_string(inDeviceObjectID) + "]");
    }

    Device& device = static_cast<Device&>(*devicePtr);
    return device.StartIO(inClientID);
  }

  OSStatus StopIO(AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    // This call tells the device that IO is stopping for the given client. When
    // this routine returns, the device's clock is stopped and it is no longer
    // ready to have data read/written. It is important to note that multiple
    // clients can have IO running on the device at the same time. So, work only
    // needs to be done when the last client stops.

    // TODO: Remove this.
    gAudioServerPlugInDriverInterface.StopIO(gAudioServerPlugInDriverRef,
                                             inDeviceObjectID, inClientID);

    auto devicePtr = GetRegistry()[inDeviceObjectID];
    if (devicePtr == nullptr || devicePtr->ClassId() != kAudioDeviceClassID) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "Invalid device object ID [id:" +
                              std::to_string(inDeviceObjectID) + "]");
    }

    Device& device = static_cast<Device&>(*devicePtr);
    return device.StopIO(inClientID);
  }

  OSStatus GetZeroTimeStamp(AudioObjectID inDeviceObjectID,
                            UInt32 inClientID,
                            Float64* outSampleTime,
                            UInt64* outHostTime,
                            UInt64* outSeed) {
    return gAudioServerPlugInDriverInterface.GetZeroTimeStamp(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID,
        outSampleTime, outHostTime, outSeed);
  }

  OSStatus WillDoIOOperation(AudioObjectID inDeviceObjectID,
                             UInt32 inClientID,
                             UInt32 inOperationID,
                             Boolean* outWillDo,
                             Boolean* outWillDoInPlace) {
    return gAudioServerPlugInDriverInterface.WillDoIOOperation(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID,
        inOperationID, outWillDo, outWillDoInPlace);
  }

  OSStatus BeginIOOperation(AudioObjectID inDeviceObjectID,
                            UInt32 inClientID,
                            UInt32 inOperationID,
                            UInt32 inIOBufferFrameSize,
                            const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return gAudioServerPlugInDriverInterface.BeginIOOperation(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID,
        inOperationID, inIOBufferFrameSize, inIOCycleInfo);
  }

  OSStatus DoIOOperation(AudioObjectID inDeviceObjectID,
                         AudioObjectID inStreamObjectID,
                         UInt32 inClientID,
                         UInt32 inOperationID,
                         UInt32 inIOBufferFrameSize,
                         const AudioServerPlugInIOCycleInfo* inIOCycleInfo,
                         void* ioMainBuffer,
                         void* ioSecondaryBuffer) {
    return gAudioServerPlugInDriverInterface.DoIOOperation(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inStreamObjectID,
        inClientID, inOperationID, inIOBufferFrameSize, inIOCycleInfo,
        ioMainBuffer, ioSecondaryBuffer);
  }

  OSStatus EndIOOperation(AudioObjectID inDeviceObjectID,
                          UInt32 inClientID,
                          UInt32 inOperationID,
                          UInt32 inIOBufferFrameSize,
                          const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return gAudioServerPlugInDriverInterface.EndIOOperation(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID,
        inOperationID, inIOBufferFrameSize, inIOCycleInfo);
  }

#pragma endregion

#pragma region AudioObjectInterface

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

#pragma endregion

 protected:
 private:
  std::atomic<ULONG> refCount_ = 1U;
  AudioServerPlugInHostRef host_ = nullptr;
  double hostTicksPerFrame_ = 0.0f;
};

}  // namespace ProxyAudio
