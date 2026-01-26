// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/MuteControl.hpp>
#include <memory>

#include "ProxyObject.hpp"

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
      std::shared_ptr<const aspl::Context> context);

 public:
  explicit ProxyMuteControl(const AudioObjectID targetObjectID,
                            std::shared_ptr<const aspl::Context> context);

  virtual ~ProxyMuteControl() = default;

  void ApplyProcessing(Float32* frames,
                       UInt32 frameCount,
                       UInt32 channelCount) const override;
};

}  // namespace ProxyAudio
