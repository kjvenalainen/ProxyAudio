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
}

OSStatus ProxyMuteControl::SetIsMutedImpl(bool value) {
  mutedProxy_.SetValue(value);

  return aspl::MuteControl::SetIsMutedImpl(value);
}

void ProxyMuteControl::ApplyProcessing(Float32* frames,
                                       UInt32 frameCount,
                                       UInt32 channelCount) const {
  // This is a proxy, so we don't need to do anything.
  // The actual processing is done by the target mute control.
}

}  // namespace ProxyAudio
