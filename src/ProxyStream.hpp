// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Stream.hpp>
#include <memory>

#include "CommonProperties.hpp"
#include "Error.hpp"
#include "ProxyObject.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

struct ProxyStream;

// Explicit specialization MUST come before ProxyStream inherits from
// ProxyObject<ProxyStream> Otherwise BaseTraits<ProxyStream> gets instantiated
// with the default template.
template <>
struct BaseTraits<ProxyStream> {
  typedef aspl::Stream BaseType;
  typedef aspl::StreamParameters ParametersType;
};

// A aspl::Stream which clones all of the Audio Object properties from the
// target device on creation.
class ProxyStream : public ProxyObject<ProxyStream> {
  friend struct ProxyObject<ProxyStream>;

 protected:
  static aspl::StreamParameters GetParameters(
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

 public:
  explicit ProxyStream(const AudioObjectID targetObjectID,
                       std::shared_ptr<const aspl::Context> context,
                       std::shared_ptr<aspl::Device> parentDevice)
      : ProxyObject<ProxyStream>(targetObjectID,
                                 context,
                                 parentDevice,
                                 GetParameters(targetObjectID, context)) {
    ProxyAudio::Tracer::FromTracer(GetContext()->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyStream:ProxyStream() Creating proxy for object: %u",
                  targetObjectID);
  }

  virtual ~ProxyStream() = default;
};

}  // namespace ProxyAudio
