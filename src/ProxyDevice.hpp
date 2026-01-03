// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <memory>

#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Error.hpp"
#include "ProxyObject.hpp"
#include "ProxyStream.hpp"

namespace ProxyAudio {

struct ProxyDevice;

// Explicit specialization MUST come before ProxyDevice inherits from
// ProxyObject<ProxyDevice> Otherwise BaseTraits<ProxyDevice> gets instantiated
// with the default template.
template <>
struct BaseTraits<ProxyDevice> {
  typedef aspl::Device BaseType;
  typedef aspl::DeviceParameters ParametersType;
};

// A aspl::Device which clones all of the Audio Object properties from the
// target device on creation.
class ProxyDevice : public ProxyObject<ProxyDevice> {
  friend struct ProxyObject<ProxyDevice>;

 protected:
  static aspl::DeviceParameters GetParameters(
      const AudioObjectID targetDeviceID,
      std::shared_ptr<const aspl::Context> context) {
    context->Tracer->Message(
        "ProxyDevice:GetParameters() Getting target parameters");

    try {
      aspl::DeviceParameters parameters{
          .Name = GetDeviceNameProperty(targetDeviceID) + " (Proxy)",
          .DeviceUID = GetDeviceUIDProperty(targetDeviceID) + "_proxy",
          .ModelUID = GetDeviceModelUIDProperty(targetDeviceID) + "_proxy",
          .CanBeDefault = GetDeviceCanBeDefaultProperty(targetDeviceID),
          .CanBeDefaultForSystemSounds =
              GetDeviceCanBeDefaultForSystemSoundsProperty(targetDeviceID),
          .SampleRate = GetDeviceSampleRateProperty(targetDeviceID),
      };

      return parameters;
    } catch (const OSStatusError& e) {
      context->Tracer->Message(
          "ProxyDevice:GetParameters() Failed to get target parameters: "
          "%s",
          e.what());

      throw e;
    }
  }

 public:
  explicit ProxyDevice(const AudioObjectID targetDeviceID,
                       std::shared_ptr<const aspl::Context> context)
      : ProxyObject<ProxyDevice>(targetDeviceID,
                                 context,
                                 GetParameters(targetDeviceID, context)) {
    GetContext()->Tracer->Message(
        "ProxyDevice:ProxyDevice() Creating proxy device for target device: %u",
        targetDeviceID);

    auto numberOfOutputStreams = ProxyAudio::GetPropertyDataSize(
        targetDeviceID,
        {
            .mSelector = kAudioDevicePropertyStreams,
            .mScope = kAudioObjectPropertyScopeOutput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    auto numberOfInputStreams = ProxyAudio::GetPropertyDataSize(
        targetDeviceID,
        {
            .mSelector = kAudioDevicePropertyStreams,
            .mScope = kAudioObjectPropertyScopeInput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    GetContext()->Tracer->Message(
        "ProxyDevice:ProxyDevice() Cloning %u input and %u output streams.",
        numberOfInputStreams, numberOfOutputStreams);
  }

  virtual ~ProxyDevice() = default;
};

}  // namespace ProxyAudio
