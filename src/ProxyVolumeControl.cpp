// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyVolumeControl.hpp"

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

}  // namespace ProxyAudio
