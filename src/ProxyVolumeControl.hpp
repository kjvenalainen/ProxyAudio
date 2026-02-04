// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/VolumeControl.hpp>
#include <memory>

#include "ProxyObject.hpp"
#include "ProxyProperty.hpp"

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

static constexpr AudioObjectPropertyAddress ScalarValueAddress = {
    .mSelector = kAudioLevelControlPropertyScalarValue,
    .mScope = kAudioObjectPropertyScopeGlobal,
    .mElement = kAudioObjectPropertyElementMain};

// A aspl::VolumeControl which clones all of the Audio Object properties from
// the target device on creation.
class ProxyVolumeControl : public ProxyObject<ProxyVolumeControl> {
  friend struct ProxyObject<ProxyVolumeControl>;

 protected:
  static aspl::VolumeControlParameters GetParameters(
      const AudioObjectID targetVolumeControlID,
      std::shared_ptr<const aspl::Context> context);

 public:
  explicit ProxyVolumeControl(const AudioObjectID targetObjectID,
                              std::shared_ptr<const aspl::Context> context);

  virtual ~ProxyVolumeControl() = default;

  void ApplyProcessing(Float32* frames,
                       UInt32 frameCount,
                       UInt32 channelCount) const override;

 protected:
  OSStatus SetRawValueImpl(SInt32 value) override;

  ProxyProperty<Float32> scalarValueProxy_;
};

}  // namespace ProxyAudio
