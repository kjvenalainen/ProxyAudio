// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>

#include <memory>

#include "Error.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

// Whether to log the calls to the static functions.
constexpr static bool ENABLE_STATIC_LOGGING = false;
#define LogStatic(inFormat, ...)         \
  if constexpr (ENABLE_STATIC_LOGGING) { \
    Log(inFormat, ##__VA_ARGS__);        \
  }

// Implements the single instance pattern for the driver, and provides the
// static COM method routing to the driver's objects.
template <typename T>
class PlugInInterface {
 public:
  using Self = PlugInInterface<T>;
  PlugInInterface& operator=(const Self&) = delete;
  PlugInInterface& operator=(const Self&&) = delete;

  // Gets a reference to the driver from the interface pointer.
  static T& GetDriver(const AudioServerPlugInDriverRef& inDriverRef) {
    static_assert(std::is_base_of_v<PlugInInterface<T>, T>,
                  "T must be derived from PlugInInterface");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
    auto& driver = *reinterpret_cast<T*>(reinterpret_cast<UInt8*>(inDriverRef) -
                                         offsetof(T, interfacePtr_));

    return driver;
  }

  // Gets the driver ref from the driver instance.
  AudioServerPlugInDriverRef GetDriverRef() { return &interfacePtr_; }

  virtual ~PlugInInterface() = default;

 private:
  // Static C functions that implement the AudioServerPlugInInterface

  static HRESULT StaticQueryInterface(void* inDriver,
                                      REFIID inUUID,
                                      LPVOID* outInterface) {
    LogStatic("StaticQueryInterface [driver: %p]", inDriver);

    try {
      return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
          .QueryInterface(inUUID, outInterface);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static ULONG StaticAddRef(void* inDriver) {
    LogStatic("StaticAddRef [driver: %p]", inDriver);

    try {
      return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
          .AddRef();
    } catch (const ErrorWithCode& e) {
      e.log();
      return 0;
    }
  }

  static ULONG StaticRelease(void* inDriver) {
    LogStatic("StaticRelease [driver: %p]", inDriver);

    try {
      return GetDriver(reinterpret_cast<AudioServerPlugInDriverRef>(inDriver))
          .Release();
    } catch (const ErrorWithCode& e) {
      e.log();
      return 0;
    }
  }

  static HRESULT StaticInitialize(AudioServerPlugInDriverRef inDriver,
                                  AudioServerPlugInHostRef inHost) {
    LogStatic("StaticInitialize [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).Initialize(inHost);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticCreateDevice(
      AudioServerPlugInDriverRef inDriver,
      CFDictionaryRef inDescription,
      const AudioServerPlugInClientInfo* inClientInfo,
      AudioObjectID* outDeviceObjectID) {
    LogStatic("StaticCreateDevice [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).CreateDevice(inDescription, inClientInfo,
                                              outDeviceObjectID);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticDestroyDevice(AudioServerPlugInDriverRef inDriver,
                                      AudioObjectID inDeviceObjectID) {
    LogStatic("StaticDestroyDevice [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).DestroyDevice(inDeviceObjectID);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticAddDeviceClient(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      const AudioServerPlugInClientInfo* inClientInfo) {
    LogStatic("StaticAddDeviceClient [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).AddDeviceClient(inDeviceObjectID,
                                                 inClientInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticRemoveDeviceClient(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      const AudioServerPlugInClientInfo* inClientInfo) {
    LogStatic("StaticRemoveDeviceClient [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).RemoveDeviceClient(inDeviceObjectID,
                                                    inClientInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticPerformDeviceConfigurationChange(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt64 inChangeAction,
      void* inChangeInfo) {
    LogStatic("StaticPerformDeviceConfigurationChange [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).PerformDeviceConfigurationChange(
          inDeviceObjectID, inChangeAction, inChangeInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticAbortDeviceConfigurationChange(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt64 inChangeAction,
      void* inChangeInfo) {
    LogStatic("StaticAbortDeviceConfigurationChange [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).AbortDeviceConfigurationChange(
          inDeviceObjectID, inChangeAction, inChangeInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static Boolean StaticHasProperty(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress) {
    LogStatic("StaticHasProperty [driver: %p]", inDriver);

    try {
      if (inAddress == nullptr) {
        throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                            "HasProperty: no address");
      }

      auto& registry = GetDriver(inDriver).GetRegistry();
      return registry[inObjectID]->HasProperty(inClientProcessID, inAddress);
    } catch (const ErrorWithCode& e) {
      e.log();
      return false;
    }
  }

  static OSStatus StaticIsPropertySettable(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      Boolean* outIsSettable) {
    LogStatic("StaticIsPropertySettable [driver: %p]", inDriver);

    try {
      if (inAddress == nullptr) {
        throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                            "IsPropertySettable: no address");
      }
      if (outIsSettable == nullptr) {
        throw ErrorWithCode(
            kAudioHardwareIllegalOperationError,
            "IsPropertySettable: no place to put the return value");
      }

      auto& registry = GetDriver(inDriver).GetRegistry();
      return registry[inObjectID]->IsPropertySettable(inClientProcessID,
                                                      inAddress, outIsSettable);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticGetPropertyDataSize(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inObjectID,
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32* outDataSize) {
    LogStatic("StaticGetPropertyDataSize [driver: %p]", inDriver);

    try {
      if (inAddress == nullptr) {
        throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                            "GetPropertyDataSize: no address");
      }
      if (outDataSize == nullptr) {
        throw ErrorWithCode(
            kAudioHardwareIllegalOperationError,
            "GetPropertyDataSize: no place to put the return value");
      }

      auto& registry = GetDriver(inDriver).GetRegistry();
      return registry[inObjectID]->GetPropertyDataSize(
          inClientProcessID, inAddress, inQualifierDataSize, inQualifierData,
          outDataSize);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
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
    LogStatic("StaticGetPropertyData [driver: %p]", inDriver);

    try {
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
        throw ErrorWithCode(
            kAudioHardwareIllegalOperationError,
            "GetPropertyData: no place to put the return value");
      }

      auto& registry = GetDriver(inDriver).GetRegistry();
      return registry[inObjectID]->GetPropertyData(
          inClientProcessID, inAddress, inQualifierDataSize, inQualifierData,
          inDataSize, outDataSize, outData);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
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
    LogStatic("StaticSetPropertyData [driver: %p]", inDriver);

    try {
      if (inAddress == nullptr) {
        throw ErrorWithCode(kAudioHardwareIllegalOperationError,
                            "SetPropertyData: no address");
      }

      std::vector<AudioObjectPropertyAddress> changedAddresses;

      auto& driver = GetDriver(inDriver);
      auto& registry = driver.GetRegistry();
      OSStatus result = registry[inObjectID]->SetPropertyData(
          inClientProcessID, inAddress, inQualifierDataSize, inQualifierData,
          inDataSize, inData, changedAddresses);

      // Notify the host about property changes.
      if (changedAddresses.size() > 0) {
        auto host = driver.GetHost();

        host->PropertiesChanged(host, inObjectID, changedAddresses.size(),
                                changedAddresses.data());
      }

      return result;
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticStartIO(AudioServerPlugInDriverRef inDriver,
                                AudioObjectID inDeviceObjectID,
                                UInt32 inClientID) {
    LogStatic("StaticStartIO [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).StartIO(inDeviceObjectID, inClientID);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticStopIO(AudioServerPlugInDriverRef inDriver,
                               AudioObjectID inDeviceObjectID,
                               UInt32 inClientID) {
    LogStatic("StaticStopIO [driver: %p]", inDriver);

    try {
      return GetDriver(inDriver).StopIO(inDeviceObjectID, inClientID);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticGetZeroTimeStamp(AudioServerPlugInDriverRef inDriver,
                                         AudioObjectID inDeviceObjectID,
                                         UInt32 inClientID,
                                         Float64* outSampleTime,
                                         UInt64* outHostTime,
                                         UInt64* outSeed) {
    try {
      return GetDriver(inDriver).GetZeroTimeStamp(
          inDeviceObjectID, inClientID, outSampleTime, outHostTime, outSeed);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticWillDoIOOperation(AudioServerPlugInDriverRef inDriver,
                                          AudioObjectID inDeviceObjectID,
                                          UInt32 inClientID,
                                          UInt32 inOperationID,
                                          Boolean* outWillDo,
                                          Boolean* outWillDoInPlace) {
    try {
      return GetDriver(inDriver).WillDoIOOperation(inDeviceObjectID, inClientID,
                                                   inOperationID, outWillDo,
                                                   outWillDoInPlace);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticBeginIOOperation(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt32 inClientID,
      UInt32 inOperationID,
      UInt32 inIOBufferFrameSize,
      const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    try {
      return GetDriver(inDriver).BeginIOOperation(
          inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize,
          inIOCycleInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
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
    try {
      return GetDriver(inDriver).DoIOOperation(
          inDeviceObjectID, inStreamObjectID, inClientID, inOperationID,
          inIOBufferFrameSize, inIOCycleInfo, ioMainBuffer, ioSecondaryBuffer);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

  static OSStatus StaticEndIOOperation(
      AudioServerPlugInDriverRef inDriver,
      AudioObjectID inDeviceObjectID,
      UInt32 inClientID,
      UInt32 inOperationID,
      UInt32 inIOBufferFrameSize,
      const AudioServerPlugInIOCycleInfo* inIOCycleInfo) {
    try {
      return GetDriver(inDriver).EndIOOperation(
          inDeviceObjectID, inClientID, inOperationID, inIOBufferFrameSize,
          inIOCycleInfo);
    } catch (const ErrorWithCode& e) {
      e.log();
      return e.code();
    }
  }

 protected:
  PlugInInterface()
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

  // Gets the registry of audio objects for this plug-in driver.
  static PlugInDriverSingleton& GetSingleton() {
    static PlugInDriverSingleton singleton;
    return singleton;
  }

  AudioServerPlugInInterface interface_;
  AudioServerPlugInInterface* interfacePtr_;
};

}  // namespace ProxyAudio
