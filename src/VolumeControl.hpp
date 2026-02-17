// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <MacTypes.h>
#include <dispatch/dispatch.h>

#include <aspl/Context.hpp>
#include <aspl/VolumeControl.hpp>
#include <atomic>
#include <memory>

#include "aspl/Storage.hpp"

namespace ProxyAudio {

struct VolumeControlParameters {
  //! Define whether this is input or output control.
  //! Used by default implementation of VolumeControl::GetScope().
  AudioObjectPropertyScope Scope = kAudioObjectPropertyScopeOutput;

  //! Minimum volume value in raw units.
  SInt32 MinRawVolume = 0;

  //! Maximum volume value in raw units.
  SInt32 MaxRawVolume = 96;

  //! Minimum volume value in decibel units.
  Float32 MinDecibelVolume = -96.0f;

  //! Maximum volume value in decibel units.
  Float32 MaxDecibelVolume = 0.0f;

  // Key to use for storage. If left empty, no storage will be used.
  std::string StorageKey = "";

  aspl::VolumeControlParameters AsAsplParameters() const {
    return {
        .Scope = Scope,
        .MinRawVolume = MinRawVolume,
        .MaxRawVolume = MaxRawVolume,
        .MinDecibelVolume = MinDecibelVolume,
        .MaxDecibelVolume = MaxDecibelVolume,
    };
  }
};

// Volume control that writes state to storage for persistence.
class VolumeControl : public aspl::VolumeControl {
 public:
  static constexpr SInt64 INVALID_VALUE = std::numeric_limits<SInt64>::min();

  explicit VolumeControl(std::shared_ptr<const aspl::Context> context,
                         const VolumeControlParameters& params = {});

  virtual ~VolumeControl() = default;

  OSStatus SetRawValueImpl(SInt32 value) override;

 private:
  aspl::Storage storage_;
  const std::string storageKey_;
  std::atomic<SInt64> pendingValue_{INVALID_VALUE};
};

}  // namespace ProxyAudio
