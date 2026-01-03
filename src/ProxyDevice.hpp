// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <memory>
#include <stdexcept>

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
  explicit ProxyDevice(const AudioObjectID targetObjectID,
                       std::shared_ptr<const aspl::Context> context)
      : ProxyObject<ProxyDevice>(targetObjectID,
                                 context,
                                 GetParameters(targetObjectID, context)) {
    GetContext()->Tracer->Message(
        "ProxyDevice:ProxyDevice() Creating proxy for object: %u",
        targetObjectID);

    auto targetDeviceClass = ProxyAudio::GetPropertyData<AudioClassID>(
        targetObjectID,
        {
            .mSelector = kAudioObjectPropertyClass,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    if (targetDeviceClass != kAudioDeviceClassID) {
      throw OSStatusError(kAudioHardwareBadObjectError,
                          {
                              .mSelector = kAudioObjectPropertyClass,
                              .mScope = kAudioObjectPropertyScopeGlobal,
                              .mElement = kAudioObjectPropertyElementMain,
                          });
    }
  }

  void AddProxyStreams() {
    auto inputStreams = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
        GetTargetObjectID(),
        {
            .mSelector = kAudioDevicePropertyStreams,
            .mScope = kAudioObjectPropertyScopeInput,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    auto outputStreams =
        ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
            GetTargetObjectID(),
            {
                .mSelector = kAudioDevicePropertyStreams,
                .mScope = kAudioObjectPropertyScopeOutput,
                .mElement = kAudioObjectPropertyElementMain,
            },
            {});

    GetContext()->Tracer->Message(
        "ProxyDevice:ProxyDevice() Cloning %lu input and %lu output streams.",
        inputStreams.size(), outputStreams.size());

    for (auto streamID : inputStreams) {
      auto stream = std::make_shared<ProxyStream>(
          streamID, GetContext(),
          std::static_pointer_cast<aspl::Device>(shared_from_this()));

      AddStreamAsync(std::move(stream));
    }

    for (auto streamID : outputStreams) {
      auto stream = std::make_shared<ProxyStream>(
          streamID, GetContext(),
          std::static_pointer_cast<aspl::Device>(shared_from_this()));

      AddStreamAsync(std::move(stream));
    }
  }

  virtual ~ProxyDevice() = default;
};

}  // namespace ProxyAudio
