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
                               GetParameters(targetObjectID, context)),
      latencyProxy_(
          targetObjectID,
          context,
          DeviceLatencyAddress,
          [this](const UInt32& value) { this->SetLatencyAsync(value); },
          [this]() { return this->GetLatency(); }),
      sampleRateProxy_(
          targetObjectID,
          context,
          DeviceSampleRateAddress,
          [this](const Float64& value) {
            // TODO: Recreate streams with the new rates.
            RequestConfigurationChange([this, value]() {
              this->RemoveStreams();

              const auto status = this->SetNominalSampleRateAsync(value);
              if (status != noErr) {
                ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
                    ->Message(ProxyAudio::Tracer::Error,
                              "ProxyDevice:sampleRateProxy_() Failed to set "
                              "nominal sample rate: %s",
                              status);
              }

              this->AddProxyStreams();
            });
          },
          [this]() { return this->GetNominalSampleRate(); }) {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Creating proxy for object: %u",
                targetObjectID);

  const auto targetDeviceClass = GetClassIdProperty(targetObjectID);
  if (targetDeviceClass != kAudioDeviceClassID) {
    throw OSStatusError(kAudioHardwareBadObjectError,
                        {
                            .mSelector = kAudioObjectPropertyClass,
                            .mScope = kAudioObjectPropertyScopeGlobal,
                            .mElement = kAudioObjectPropertyElementMain,
                        });
  }

  const auto availableSampleRates =
      ProxyAudio::GetPropertyData<std::vector<AudioValueRange>>(
          GetTargetObjectID(), DeviceAvailableSampleRatesAddress, {});

  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:ProxyDevice() Available sample rates: %s",
                ToString(availableSampleRates).c_str());

  auto status = SetAvailableSampleRatesAsync(availableSampleRates);
  if (status != noErr) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyDevice:ProxyDevice() Failed to set available sample "
                  "rates: %s",
                  status);
  }

  SetLatencyAsync(latencyProxy_.GetValue());
  SetNominalSampleRateAsync(sampleRateProxy_.GetValue());
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

  // Proxy device handles its own I/O and control requests.
  SetIOHandler(std::static_pointer_cast<ProxyDevice>(shared_from_this()));
  SetControlHandler(std::static_pointer_cast<ProxyDevice>(shared_from_this()));
}

void ProxyDevice::RemoveStreams() {
  ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyDevice:RemoveStreams() Removing streams");

  while (GetStreamCount(aspl::Direction::Input) > 0) {
    auto stream = GetStreamByIndex(aspl::Direction::Input, 0);
    if (stream) {
      RemoveStreamAsync(stream);
    }
  }

  while (GetStreamCount(aspl::Direction::Output) > 0) {
    auto stream = GetStreamByIndex(aspl::Direction::Output, 0);
    if (stream) {
      RemoveStreamAsync(stream);
    }
  }

  // Remove all volume and mute controls.
  while (GetVolumeControlCount(kAudioObjectPropertyScopeInput) > 0) {
    auto volumeControl =
        GetVolumeControlByIndex(kAudioObjectPropertyScopeInput, 0);
    if (volumeControl) {
      RemoveVolumeControlAsync(volumeControl);
    }
  }

  while (GetVolumeControlCount(kAudioObjectPropertyScopeOutput) > 0) {
    auto volumeControl =
        GetVolumeControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    if (volumeControl) {
      RemoveVolumeControlAsync(volumeControl);
    }
  }

  while (GetMuteControlCount(kAudioObjectPropertyScopeInput) > 0) {
    auto muteControl = GetMuteControlByIndex(kAudioObjectPropertyScopeInput, 0);
    if (muteControl) {
      RemoveMuteControlAsync(muteControl);
    }
  }

  while (GetMuteControlCount(kAudioObjectPropertyScopeOutput) > 0) {
    auto muteControl =
        GetMuteControlByIndex(kAudioObjectPropertyScopeOutput, 0);
    if (muteControl) {
      RemoveMuteControlAsync(muteControl);
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
