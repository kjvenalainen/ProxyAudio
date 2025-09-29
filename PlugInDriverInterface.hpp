// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

namespace ProxyAudio {

template <class T>
class PlugInDriverInterface {
 public:
  using Self = PlugInDriverInterface<T>;
  PlugInDriverInterface& operator=(const Self&) = delete;
  PlugInDriverInterface& operator=(const Self&&) = delete;

  static T& GetInstance() {
    static Derived instance;
    return instance;
  }

  AudioServerPlugInDriverRef GetDriverRef() { return &interfacePtr_; }

  virtual ~PlugInDriverInterface() = 0;

 protected:
  // Helper to access protected constructor.
  struct Derived : public T {
    Derived() : T() {}
  };

  PlugInDriverInterface() {
    interface_ = {
        ._reserved = nullptr,
        .QueryInterface = static_cast<T*>(this)->QueryInterface,
        .AddRef = static_cast<T*>(this)->AddRef,
        .Release = static_cast<T*>(this)->Release,
        .Initialize = static_cast<T*>(this)->Initialize,
        .CreateDevice = static_cast<T*>(this)->CreateDevice,
        .DestroyDevice = static_cast<T*>(this)->DestroyDevice,
        .AddDeviceClient = static_cast<T*>(this)->AddDeviceClient,
        .RemoveDeviceClient = static_cast<T*>(this)->RemoveDeviceClient,
        .PerformDeviceConfigurationChange =
            static_cast<T*>(this)->PerformDeviceConfigurationChange,
        .AbortDeviceConfigurationChange =
            static_cast<T*>(this)->AbortDeviceConfigurationChange,
        .HasProperty = static_cast<T*>(this)->HasProperty,
        .IsPropertySettable = static_cast<T*>(this)->IsPropertySettable,
        .GetPropertyDataSize = static_cast<T*>(this)->GetPropertyDataSize,
        .GetPropertyData = static_cast<T*>(this)->GetPropertyData,
        .SetPropertyData = static_cast<T*>(this)->SetPropertyData,
        .StartIO = static_cast<T*>(this)->StartIO,
        .StopIO = static_cast<T*>(this)->StopIO,
        .GetZeroTimeStamp = static_cast<T*>(this)->GetZeroTimeStamp,
        .WillDoIOOperation = static_cast<T*>(this)->WillDoIOOperation,
        .BeginIOOperation = static_cast<T*>(this)->BeginIOOperation,
        .DoIOOperation = static_cast<T*>(this)->DoIOOperation,
        .EndIOOperation = static_cast<T*>(this)->EndIOOperation,
    };

    interfacePtr_ = &interface_;
  }

  static AudioServerPlugInDriverInterface interface_;
  static AudioServerPlugInDriverInterface* interfacePtr_;
};

}  // namespace ProxyAudio
