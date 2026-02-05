// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyStream.hpp"

#include "CommonProperties.hpp"
#include "Error.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

aspl::StreamParameters ProxyStream::GetParameters(
    const AudioObjectID targetStreamID,
    std::shared_ptr<const aspl::Context> context) {
  try {
    // Direction, starting channel, and format, latency.
    aspl::StreamParameters parameters{
        .Direction = GetDirectionProperty(targetStreamID),
        .StartingChannel = GetStartingChannelProperty(targetStreamID),
        .Format = GetFormatProperty(targetStreamID),
        .Latency = GetLatencyProperty(targetStreamID),
    };

    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyStream:GetParameters() Target stream: %u, Direction: %s, "
            "StartingChannel: %u, "
            "Format: %s, Latency: %u",
            targetStreamID,
            parameters.Direction == aspl::Direction::Output ? "Output"
                                                            : "Input",
            parameters.StartingChannel, ToString(parameters.Format).c_str(),
            parameters.Latency);

    return parameters;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyStream:GetParameters() Failed to get target parameters: "
            "%s",
            e.what());

    throw e;
  }
}

ProxyStream::ProxyStream(const AudioObjectID targetObjectID,
                         std::shared_ptr<const aspl::Context> context,
                         std::shared_ptr<aspl::Device> parentDevice)
    : ProxyObject<ProxyStream>(targetObjectID,
                               context,
                               parentDevice,
                               GetParameters(targetObjectID, context)),
      latencyProxy_(
          targetObjectID,
          context,
          StreamLatencyAddress,
          [this](const UInt32& value) {
            const auto status = this->SetLatencyAsync(value);
            if (status != noErr) {
              ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
                  ->Message(
                      ProxyAudio::Tracer::Error,
                      "ProxyStream:latencyProxy_() Failed to set latency: %s",
                      status);
            }
          },
          [this]() { return this->GetLatency(); }),
      formatProxy_(
          targetObjectID,
          context,
          StreamFormatAddress,
          [this](const AudioStreamBasicDescription& value) {
            ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
                ->Message(
                    ProxyAudio::Tracer::Info,
                    "ProxyStream:formatProxy_() Setting physical format: %s",
                    ToString(value).c_str());

            const auto status = this->SetPhysicalFormatAsync(value);
            if (status != noErr) {
              ProxyAudio::Tracer::FromTracer(this->GetContext()->Tracer)
                  ->Message(ProxyAudio::Tracer::Error,
                            "ProxyStream:formatProxy_() Failed to set physical "
                            "format: %s",
                            status);
            }
          },
          [this]() { return this->GetPhysicalFormat(); }) {
  ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
      ->Message(ProxyAudio::Tracer::Info,
                "ProxyStream:ProxyStream() Creating proxy for object: %u",
                targetObjectID);

  SetLatencyAsync(latencyProxy_.GetValue());
  SetPhysicalFormatAsync(formatProxy_.GetValue());
}

}  // namespace ProxyAudio
