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
#include "Utils.hpp"
#include "aspl/VolumeControl.hpp"

namespace ProxyAudio {

struct ProxyVolumeControl;

// Explicit specialization MUST come before ProxyVolumeControl inherits from
// ProxyObject<ProxyVolumeControl> Otherwise BaseTraits<ProxyVolumeControl> gets
// instantiated with the default template.
template <>
struct BaseTraits<ProxyVolumeControl> {
  typedef aspl::VolumeControl BaseType;
  typedef aspl::VolumeControlParameters ParametersType;
};

// A aspl::VolumeControl which clones all of the Audio Object properties from
// the target device on creation.
class ProxyVolumeControl : public ProxyObject<ProxyVolumeControl> {
  friend struct ProxyObject<ProxyVolumeControl>;

 protected:
  static aspl::VolumeControlParameters GetParameters(
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

      context->Tracer->Message(
          "ProxyVolumeControl:GetParameters() Target volume control: %u, "
          "MinRawVolume: %d, MaxRawVolume: %d, MinDecibelVolume: %f, "
          "MaxDecibelVolume: %f",
          targetVolumeControlID, parameters.MinRawVolume,
          parameters.MaxRawVolume, parameters.MinDecibelVolume,
          parameters.MaxDecibelVolume);

      return parameters;
    } catch (const OSStatusError& e) {
      context->Tracer->Message(
          "ProxyVolumeControl:GetParameters() Failed to get target parameters: "
          "%s",
          e.what());

      throw e;
    }
  }

 public:
  explicit ProxyVolumeControl(const AudioObjectID targetObjectID,
                              std::shared_ptr<const aspl::Context> context)
      : ProxyObject<ProxyVolumeControl>(
            targetObjectID,
            context,
            GetParameters(targetObjectID, context)) {
    GetContext()->Tracer->Message(
        "ProxyVolumeControl:ProxyVolumeControl() Creating proxy for object: %u",
        targetObjectID);
  }

  virtual ~ProxyVolumeControl() = default;
};

}  // namespace ProxyAudio
