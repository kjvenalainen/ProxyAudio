// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyDevice.hpp"

#include <CoreAudio/AudioHardware.h>


#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Error.hpp"
#include "ProxyMuteControl.hpp"
#include "ProxyStream.hpp"
#include "ProxyVolumeControl.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

aspl::DeviceParameters ProxyDevice::GetParameters(
    const AudioObjectID targetDeviceID,
    std::shared_ptr<const aspl::Context> context) {
  ProxyAudio::Tracer::FromTracer(context->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
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
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyDevice:GetParameters() Failed to get target parameters: "
            "%s",
            e.what());

    throw e;
  }
}

ProxyDevice::ProxyDevice(const AudioObjectID targetObjectID,
                         std::shared_ptr<const aspl::Context> context)
    : ProxyObject<ProxyDevice>(targetObjectID,
                               context,
                               GetParameters(targetObjectID, context)) {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Creating proxy for object: %u",
                targetObjectID);

  auto targetDeviceClass = GetClassIdProperty(targetObjectID);

  if (targetDeviceClass != kAudioDeviceClassID) {
    throw OSStatusError(kAudioHardwareBadObjectError,
                        {
                            .mSelector = kAudioObjectPropertyClass,
                            .mScope = kAudioObjectPropertyScopeGlobal,
                            .mElement = kAudioObjectPropertyElementMain,
                        });
  }
}

void ProxyDevice::AddProxyStreams() {
  auto inputStreams = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
      GetTargetObjectID(),
      {
          .mSelector = kAudioDevicePropertyStreams,
          .mScope = kAudioObjectPropertyScopeInput,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});

  auto outputStreams = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
      GetTargetObjectID(),
      {
          .mSelector = kAudioDevicePropertyStreams,
          .mScope = kAudioObjectPropertyScopeOutput,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});

  auto controls = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
      GetTargetObjectID(),
      {
          .mSelector = kAudioObjectPropertyControlList,
          .mScope = kAudioObjectPropertyScopeInput,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});

  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Cloning %lu input and %lu "
                "output "
                "streams.",
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

    AddStreamAsync(stream);

    const auto volumeControlId =
        GetControlId(kAudioVolumeControlClassID,
                     kAudioDevicePropertyScopeOutput, controls);
    if (volumeControlId != kAudioObjectUnknown) {
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ProxyDevice() Cloning volume control for "
                    "stream: %u",
                    streamID);

      auto volume =
          std::make_shared<ProxyVolumeControl>(volumeControlId, GetContext());

      AddVolumeControlAsync(volume);
      stream->AttachVolumeControl(std::move(volume));
    }

    const auto muteControlId = GetControlId(
        kAudioMuteControlClassID, kAudioDevicePropertyScopeOutput, controls);
    if (muteControlId != kAudioObjectUnknown) {
      ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyDevice:ProxyDevice() Cloning mute control for "
                    "stream: %u",
                    streamID);

      auto mute =
          std::make_shared<ProxyMuteControl>(muteControlId, GetContext());

      AddMuteControlAsync(mute);
      stream->AttachMuteControl(std::move(mute));
    }
  }
}

void ProxyDevice::OnWriteMixedOutput(
    const std::shared_ptr<aspl::Stream>& stream,
    Float64 zeroTimestamp,
    Float64 timestamp,
    const void* bytes,
    UInt32 bytesCount) {
  // TODO: Pass data to the target device.
}

}  // namespace ProxyAudio
