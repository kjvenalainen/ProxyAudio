// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyMuteControl.hpp"

#include <CoreAudio/AudioHardware.h>

#include "CommonProperties.hpp"
#include "Error.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

aspl::MuteControlParameters ProxyMuteControl::GetParameters(
    const AudioObjectID targetMuteControlID,
    std::shared_ptr<const aspl::Context> context) {
  try {
    // Direction, starting channel, and format, latency.
    aspl::MuteControlParameters parameters{
        .Scope = GetScopeProperty(targetMuteControlID),
    };

    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyMuteControl:GetParameters() Target mute control: %u",
                  targetMuteControlID);

    return parameters;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Warn,
                  "ProxyMuteControl:GetParameters() Failed to get target "
                  "parameters: "
                  "%s",
                  e.what());

    throw e;
  }
}

ProxyMuteControl::ProxyMuteControl(const AudioObjectID targetObjectID,
                                   std::shared_ptr<const aspl::Context> context)
    : ProxyObject<ProxyMuteControl>(targetObjectID,
                                    context,
                                    GetParameters(targetObjectID, context)),
      mutedProxy_(
          targetObjectID,
          context,
          MuteAddress,
          [this](const bool& value) { this->SetIsMuted(value); },
          [this]() { return this->GetIsMuted(); }) {
  ProxyAudio::Tracer::FromTracer(context->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyMuteControl:ProxyMuteControl() Creating proxy for "
                "object: %u",
                targetObjectID);

  // Set the initial value of the mute control.
  SetIsMuted(mutedProxy_.GetValue());
}

OSStatus ProxyMuteControl::SetIsMutedImpl(bool value) {
  const auto status = aspl::MuteControl::SetIsMutedImpl(value);
  if (status != noErr) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Error,
                  "ProxyMuteControl:SetIsMutedImpl() Failed to set target "
                  "mute control: %s",
                  status);
    return status;
  }

  mutedProxy_.SetValue(value);
  return noErr;
}

void ProxyMuteControl::ApplyProcessing(Float32* frames,
                                       UInt32 frameCount,
                                       UInt32 channelCount) const {
  // This is a proxy, so we don't need to do anything.
  // The actual processing is done by the target mute control.
}

}  // namespace ProxyAudio
