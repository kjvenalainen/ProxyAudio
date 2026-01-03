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
// target Stream on creation.
class ProxyStream : public ProxyObject<ProxyStream> {
  friend struct ProxyObject<ProxyStream>;

 protected:
  static aspl::StreamParameters GetParameters(
      std::shared_ptr<const aspl::Context> context,
      const AudioObjectID targetStreamID) {
    context->Tracer->Message(
        "ProxyStream:GetParameters() Getting target parameters");

    try {
      aspl::StreamParameters parameters{

      };

      return parameters;
    } catch (const OSStatusError& e) {
      context->Tracer->Message(
          "ProxyStream:GetParameters() Failed to get target parameters: "
          "%s",
          e.what());

      throw e;
    }
  }

 public:
  explicit ProxyStream(const AudioObjectID targetStreamID,
                       std::shared_ptr<const aspl::Context> context,
                       std::shared_ptr<aspl::Device> device,
                       aspl::Direction direction)
      : ProxyObject<ProxyStream>(targetStreamID, context, device) {}

  virtual ~ProxyStream() = default;
};

}  // namespace ProxyAudio
