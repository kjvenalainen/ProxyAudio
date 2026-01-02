// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>
#include <memory>

namespace ProxyAudio {

// A aspl::Device which clones all of the Audio Object properties from the
// target device on creation.
class ProxyDevice : public aspl::Device {
  static aspl::DeviceParameters GetTargetDeviceParameters(
      std::shared_ptr<const aspl::Context> context,
      const AudioObjectID targetDeviceID);

 public:
  explicit ProxyDevice(std::shared_ptr<const aspl::Context> context,
                       const AudioObjectID targetDeviceID);

 private:
  const AudioObjectID targetDeviceID_;
};

}  // namespace ProxyAudio
