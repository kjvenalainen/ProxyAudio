// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <memory>

#include "ProxyObject.hpp"

namespace ProxyAudio {

struct ProxyDevice;

// Explicit specialization MUST come before ProxyDevice inherits from
// ProxyObject<ProxyDevice> Otherwise BaseTraits<ProxyDevice> gets instantiated
// with the default template.
template <>
struct BaseTraits<ProxyDevice> {
  typedef aspl::Device BaseType;
  typedef aspl::DeviceParameters ParametersType;
};

// A aspl::Device which clones all of the Audio Object properties from the
// target device on creation.
class ProxyDevice : public ProxyObject<ProxyDevice>,
                    public aspl::IORequestHandler,
                    public aspl::ControlRequestHandler {
  friend struct ProxyObject<ProxyDevice>;

 protected:
  static aspl::DeviceParameters GetParameters(
      const AudioObjectID targetDeviceID,
      std::shared_ptr<const aspl::Context> context);

 public:
  explicit ProxyDevice(const AudioObjectID targetObjectID,
                       std::shared_ptr<const aspl::Context> context);

  void AddProxyStreams();

  virtual ~ProxyDevice() = default;

  void OnWriteMixedOutput(const std::shared_ptr<aspl::Stream>& stream,
                          Float64 zeroTimestamp,
                          Float64 timestamp,
                          const void* bytes,
                          UInt32 bytesCount) override;
};

}  // namespace ProxyAudio
