// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <CoreFoundation/CFUUID.h>
#include <MacTypes.h>

#include "CFSharedPtr.hpp"
#include "Error.hpp"
#include "PlugInDriverInterface.hpp"

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

class ProxyDriverInterface
    : public PlugInDriverInterface<ProxyDriverInterface> {
  friend PlugInDriverInterface<ProxyDriverInterface>;

 public:
  virtual ~ProxyDriverInterface() override = default;

  HRESULT QueryInterface(REFIID inUUID, LPVOID* outInterface) {
    try {
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

      // TODO: Remove this.
      gAudioServerPlugInDriverInterface.QueryInterface(
          gAudioServerPlugInDriverRef, inUUID, outInterface);

      return S_OK;
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  ULONG AddRef() {
    gAudioServerPlugInDriverInterface.AddRef(gAudioServerPlugInDriverRef);

    Log("Count: %u", refCount_.load());

    if (refCount_ < UINT32_MAX) {
      refCount_++;
    }

    return refCount_.load();
  }

  ULONG Release() {
    gAudioServerPlugInDriverInterface.Release(gAudioServerPlugInDriverRef);

    Log("Count: %u", refCount_.load());

    if (refCount_ > 0) {
      refCount_--;
    }

    return refCount_.load();
  }

  HRESULT Initialize(AudioServerPlugInHostRef inHost) {
    return gAudioServerPlugInDriverInterface.Initialize(
        gAudioServerPlugInDriverRef, inHost);
  }

  OSStatus CreateDevice(CFDictionaryRef inDescription,
                        const AudioServerPlugInClientInfo* inClientInfo,
                        AudioObjectID* outDeviceObjectID) {
    return gAudioServerPlugInDriverInterface.CreateDevice(
        gAudioServerPlugInDriverRef, inDescription, inClientInfo,
        outDeviceObjectID);
  }

  OSStatus DestroyDevice(AudioObjectID inDeviceObjectID) {
    return gAudioServerPlugInDriverInterface.DestroyDevice(
        gAudioServerPlugInDriverRef, inDeviceObjectID);
  }

  OSStatus AddDeviceClient(AudioObjectID inDeviceObjectID,
                           const AudioServerPlugInClientInfo* inClientInfo) {
    return gAudioServerPlugInDriverInterface.AddDeviceClient(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientInfo);
  }

  OSStatus RemoveDeviceClient(AudioObjectID inDeviceObjectID,
                              const AudioServerPlugInClientInfo* inClientInfo) {
    return gAudioServerPlugInDriverInterface.RemoveDeviceClient(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientInfo);
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
    return gAudioServerPlugInDriverInterface.StartIO(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID);
  }

  OSStatus StopIO(AudioObjectID inDeviceObjectID, UInt32 inClientID) {
    return gAudioServerPlugInDriverInterface.StopIO(
        gAudioServerPlugInDriverRef, inDeviceObjectID, inClientID);
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

 protected:
  ProxyDriverInterface() : refCount_(1U) {}

 private:
  std::atomic<ULONG> refCount_;
};

}  // namespace ProxyAudio
