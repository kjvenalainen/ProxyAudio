// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include "Error.hpp"

namespace ProxyAudio {

template <class T>
class PlugInDriverInterface {
 public:
  using Self = PlugInDriverInterface<T>;
  PlugInDriverInterface& operator=(const Self&) = delete;
  PlugInDriverInterface& operator=(const Self&&) = delete;

  // Gets the driver instance.
  static T& GetInstance() {
    static Derived instance;
    return instance;
  }

  // Gets the driver instance from an AudioServerPlugInDriverRef.
  static T& GetDriver(const AudioServerPlugInDriverRef& inDriverRef) {
    AudioServerPlugInDriverInterface* driverInterface = *inDriverRef;

    return *reinterpret_cast<T*>(reinterpret_cast<UInt8*>(driverInterface) -
                                 offsetof(Self, interfacePtr_));
  }

  // Gets the driver ref from the driver instance.
  AudioServerPlugInDriverRef GetDriverRef() { return &interfacePtr_; }

  virtual ~PlugInDriverInterface() = 0;

 private:
  // Static C functions that implement the AudioServerPlugInDriverInterface

  static HRESULT StaticQueryInterface(void* inDriver,
                                      REFIID inUUID,
                                      LPVOID* outInterface) {
    try {
      return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
          .QueryInterface(inUUID, outInterface);
    } catch (const ErrorWithCode& e) {
      printf("Error %s\n", e.what());

      return e.code();
    }
  }

  static ULONG StaticAddRef(void* inDriver) {
    return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
        .AddRef();
  }

  static ULONG StaticRelease(void* inDriver) {
    return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
        .Release();
  }

  static HRESULT StaticInitialize(AudioServerPlugInDriverRef inDriver,
                                  AudioServerPlugInHostRef inHost) {
    return GetDriver(inDriver).Initialize(inHost);
  }

  static OSStatus StaticCreateDevice(
      AudioServerPlugInDriverRef inDriver,
      CFDictionaryRef inDescription,
      const AudioServerPlugInClientInfo* inClientInfo,
      AudioObjectID* outDeviceObjectID) {
    return GetDriver(inDriver).CreateDevice(inDescription, inClientInfo,
                                            outDeviceObjectID);
  }

  static OSStatus StaticDestroyDevice(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inDeviceObjectID) {
    return GetDriver(inDriver).DestroyDevice(inDeviceObjectID);
  }

  static OSStatus StaticAddDeviceClient(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      const AudioServerPlugInClientInfo* inClientInfo) {
    return GetDriver(inDriver).AddDeviceClient(inDeviceObjectID, inClientInfo);
  }

  static OSStatus StaticRemoveDeviceClient(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      const AudioServerPlugInClientInfo* inClientInfo) {
    return GetDriver(inDriver).RemoveDeviceClient(inDeviceObjectID,
                                                  inClientInfo);
  }

  static OSStatus StaticPerformDeviceConfigurationChange(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt64 inChangeAction,
      void* inChangeInfo) {
    return GetDriver(inDriver).PerformDeviceConfigurationChange(
        inDeviceObjectID, inChangeAction, inChangeInfo);
  }

  static OSStatus StaticAbortDeviceConfigurationChange(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt64 inChangeAction,
      void* inChangeInfo) {
    return GetDriver(inDriver).AbortDeviceConfigurationChange(
        inDeviceObjectID, inChangeAction, inChangeInfo);
  }

  static Boolean StaticHasProperty(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress) {
    return GetDriver(inDriver).HasProperty(inObjectID, inClientProcessID,
                                           inAddress);
  }

  static OSStatus StaticIsPropertySettable(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      Boolean* outIsSettable) {
    return GetDriver(inDriver).IsPropertySettable(inObjectID, inClientProcessID,
                                                  inAddress, outIsSettable);
  }

  static OSStatus StaticGetPropertyDataSize(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32* outDataSize) {
    return GetDriver(inDriver).GetPropertyDataSize(
        inObjectID, inClientProcessID, inAddress, inQualifierDataSize,
        inQualifierData, outDataSize);
  }

  static OSStatus StaticGetPropertyData(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      UInt32* outDataSize,
      void* outData) {
    return GetDriver(inDriver).GetPropertyData(
        inObjectID, inClientProcessID, inAddress, inQualifierDataSize,
        inQualifierData, inDataSize, outDataSize, outData);
  }

  static OSStatus StaticSetPropertyData(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      const void* inData) {
    return GetDriver(inDriver).SetPropertyData(
        inObjectID, inClientProcessID, inAddress, inQualifierDataSize,
        inQualifierData, inDataSize, inData);
  }

  static OSStatus StaticStartIO(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inDeviceObjectID,
                                UInt32 inClientID) {
    return GetDriver(inDriver).StartIO(inDeviceObjectID, inClientID);
  }

  static OSStatus StaticStopIO(AudioServerPlugInDriverRef inDriver,
                               AudioObjectID inDeviceObjectID,
                               UInt32 inClientID) {
    return GetDriver(inDriver).StopIO(inDeviceObjectID, inClientID);
  }

  static OSStatus StaticGetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                                         AudioObjectID inDeviceObjectID,
                                         UInt32 inClientID,
                                         Float64* outSampleTime,
                                         UInt64* outHostTime,
                                         UInt64* outSeed) {
    return GetDriver(inDriver).GetZeroTimeStamp(
        inDeviceObjectID, inClientID, outSampleTime, outHostTime, outSeed);
  }

  static OSStatus StaticWillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                                          AudioObjectID inDeviceObjectID,
                                          UInt32 inClientID,
                                          UInt32 inOperationID,
                                          Boolean* outWillDo,
                                          Boolean* outWillDoInPlace) {
    return GetDriver(inDriver).WillDoIOOperation(inDeviceObjectID, inClientID,
                                                 inOperationID, outWillDo,
                                                 outWillDoInPlace);
  }

  static OSStatus StaticBeginIOOperation(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt32 inClientID,
      UInt32 inOperationID,
      UInt32 inIOBufferFrameSize,
      const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return GetDriver(inDriver).BeginIOOperation(
        inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize,
        inIOCycleInfo);
  }

  static OSStatus StaticDoIOOperation(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      AudioObjectID inStreamObjectID,
      UInt32 inClientID,
      UInt32 inOperationID,
      UInt32 inIOBufferFrameSize,
      const AudioServerPlugInIOCycleInfo* inIOCycleInfo,
      void* ioMainBuffer,
      void* ioSecondaryBuffer) {
    return GetDriver(inDriver).DoIOOperation(
        inDeviceObjectID, inStreamObjectID, inClientID, inOperationID,
        inIOBufferFrameSize, inIOCycleInfo, ioMainBuffer, ioSecondaryBuffer);
  }

  static OSStatus StaticEndIOOperation(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt32 inClientID,
      UInt32 inOperationID,
      UInt32 inIOBufferFrameSize,
      const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    return GetDriver(inDriver).EndIOOperation(
        inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize,
        inIOCycleInfo);
  }

 protected:
  // Helper to access protected constructor.
  struct Derived : public T {
    Derived() : T() {}
  };

  PlugInDriverInterface()
      : interface_{
            ._reserved = nullptr,
            .QueryInterface = StaticQueryInterface,
            .AddRef = StaticAddRef,
            .Release = StaticRelease,
            .Initialize = StaticInitialize,
            .CreateDevice = StaticCreateDevice,
            .DestroyDevice = StaticDestroyDevice,
            .AddDeviceClient = StaticAddDeviceClient,
            .RemoveDeviceClient = StaticRemoveDeviceClient,
            .PerformDeviceConfigurationChange = StaticPerformDeviceConfigurationChange,
            .AbortDeviceConfigurationChange = StaticAbortDeviceConfigurationChange,
            .HasProperty = StaticHasProperty,
            .IsPropertySettable = StaticIsPropertySettable,
            .GetPropertyDataSize = StaticGetPropertyDataSize,
            .GetPropertyData = StaticGetPropertyData,
            .SetPropertyData = StaticSetPropertyData,
            .StartIO = StaticStartIO,
            .StopIO = StaticStopIO,
            .GetZeroTimeStamp = StaticGetZeroTimeStamp,
            .WillDoIOOperation = StaticWillDoIOOperation,
            .BeginIOOperation = StaticBeginIOOperation,
            .DoIOOperation = StaticDoIOOperation,
            .EndIOOperation = StaticEndIOOperation,
        },
        interfacePtr_(&interface_) {}

  AudioServerPlugInDriverInterface interface_;
  AudioServerPlugInDriverInterface* interfacePtr_;
};

// Pure virtual destructor must be defined
template <class T>
PlugInDriverInterface<T>::~PlugInDriverInterface() {}

}  // namespace ProxyAudio
