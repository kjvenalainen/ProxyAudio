// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyVolumeControl.hpp"

#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Error.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

aspl::VolumeControlParameters ProxyVolumeControl::GetParameters(
    const AudioObjectID targetVolumeControlID,
    std::shared_ptr<const aspl::Context> context) {
  try {
    const auto decibelRange = GetDecibelRangeProperty(targetVolumeControlID);
    const auto decibelRangeInteger = static_cast<SInt32>(
        std::abs(decibelRange.mMaximum - decibelRange.mMinimum));

    // Direction, starting channel, and format, latency.
    aspl::VolumeControlParameters parameters{
        .Scope = GetScopeProperty(targetVolumeControlID),
        .MinRawVolume = 0,                    // Always 0.
        .MaxRawVolume = decibelRangeInteger,  // Always the absolute value of
                                              // the decibel range.
        .MinDecibelVolume = static_cast<Float32>(decibelRange.mMinimum),
        .MaxDecibelVolume = static_cast<Float32>(decibelRange.mMaximum),
    };

    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyVolumeControl:GetParameters() Target volume control: %u, "
            "MinRawVolume: %d, MaxRawVolume: %d, MinDecibelVolume: %f, "
            "MaxDecibelVolume: %f",
            targetVolumeControlID, parameters.MinRawVolume,
            parameters.MaxRawVolume, parameters.MinDecibelVolume,
            parameters.MaxDecibelVolume);

    return parameters;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyVolumeControl:GetParameters() Failed to get target "
                  "parameters: "
                  "%s",
                  e.what());

    throw e;
  }
}

ProxyVolumeControl::ProxyVolumeControl(
    const AudioObjectID targetObjectID,
    std::shared_ptr<const aspl::Context> context)
    : ProxyObject<ProxyVolumeControl>(
          targetObjectID,
          context,
          GetParameters(targetObjectID, context)) {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyVolumeControl:ProxyVolumeControl() Creating proxy for "
                "object: %u",
                targetObjectID);
}

void ProxyVolumeControl::ApplyProcessing(Float32* frames,
                                         UInt32 frameCount,
                                         UInt32 channelCount) const {
  // This is a proxy, so we don't need to do anything.
  // The actual processing is done by the target volume control.
}

OSStatus ProxyVolumeControl::SetRawValueImpl(SInt32 value) {
  const auto status = aspl::VolumeControl::SetRawValueImpl(value);
  if (status != noErr) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyVolumeControl:SetRawValueImpl() Failed to set basic "
                  "volume control: %s",
                  status);
    return status;
  }

  try {
    float scalarValue = GetScalarValue();

    ProxyAudio::SetPropertyData(
        GetTargetObjectID(),
        {
            .mSelector = kAudioLevelControlPropertyScalarValue,
            .mScope = GetScope(),
            .mElement = GetElement(),
        },
        {},
        {
            .ptr = &scalarValue,
            .size = sizeof(scalarValue),
        });

    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyVolumeControl:SetRawValueImpl() Set volume to %f",
                  scalarValue);

    return noErr;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyVolumeControl:SetRawValueImpl() Failed to set target "
                  "volume control: %s",
                  e.what());

    return e.GetStatus();
  }
}

}  // namespace ProxyAudio
