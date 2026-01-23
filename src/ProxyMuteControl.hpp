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
#include "aspl/MuteControl.hpp"

namespace ProxyAudio {

struct ProxyMuteControl;

// Explicit specialization MUST come before ProxyMuteControl inherits from
// ProxyObject<ProxyMuteControl> Otherwise BaseTraits<ProxyMuteControl> gets
// instantiated with the default template.
template <>
struct BaseTraits<ProxyMuteControl> {
  typedef aspl::MuteControl BaseType;
  typedef aspl::MuteControlParameters ParametersType;
};

// A aspl::MuteControl which clones all of the Audio Object properties from
// the target device on creation.
class ProxyMuteControl : public ProxyObject<ProxyMuteControl> {
  friend struct ProxyObject<ProxyMuteControl>;

 protected:
  static aspl::MuteControlParameters GetParameters(
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

 public:
  explicit ProxyMuteControl(const AudioObjectID targetObjectID,
                            std::shared_ptr<const aspl::Context> context)
      : ProxyObject<ProxyMuteControl>(targetObjectID,
                                      context,
                                      GetParameters(targetObjectID, context)) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyMuteControl:ProxyMuteControl() Creating proxy for object: %u",
            targetObjectID);
  }

  virtual ~ProxyMuteControl() = default;
};

}  // namespace ProxyAudio
