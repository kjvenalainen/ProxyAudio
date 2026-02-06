// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareBase.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <aspl/Stream.hpp>
#include <memory>

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

static constexpr AudioObjectPropertyAddress StreamLatencyAddress = {
    .mSelector = kAudioStreamPropertyLatency,
    .mScope = kAudioObjectPropertyScopeGlobal,
    .mElement = kAudioObjectPropertyElementMain};

static constexpr AudioObjectPropertyAddress StreamFormatAddress = {
    .mSelector = kAudioStreamPropertyPhysicalFormat,
    .mScope = kAudioObjectPropertyScopeGlobal,
    .mElement = kAudioObjectPropertyElementMain};

// A aspl::Stream which clones all of the Audio Object properties from the
// target device on creation.
class ProxyStream : public ProxyObject<ProxyStream> {
  friend struct ProxyObject<ProxyStream>;

 protected:
  static aspl::StreamParameters GetParameters(
      const AudioObjectID targetStreamID,
      std::shared_ptr<const aspl::Context> context);

 public:
  explicit ProxyStream(const AudioObjectID targetObjectID,
                       std::shared_ptr<const aspl::Context> context,
                       std::shared_ptr<aspl::Device> parentDevice);

  virtual ~ProxyStream() = default;

 protected:
};

}  // namespace ProxyAudio
