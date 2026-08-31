// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <dispatch/dispatch.h>

#include <aspl/Context.hpp>
#include <aspl/MuteControl.hpp>
#include <atomic>
#include <memory>
#include <string>

#include "aspl/Storage.hpp"

namespace ProxyAudio {

struct MuteControlParameters {
  //! Define whether this is input or output control.
  //! Used by default implementation of MuteControl::GetScope().
  AudioObjectPropertyScope Scope = kAudioObjectPropertyScopeOutput;

  // Key to use for storage. If left empty, no storage will be used.
  std::string StorageKey = "";

  aspl::MuteControlParameters AsAsplParameters() const {
    return {
        .Scope = Scope,
    };
  }
};

// Mute control that writes state to storage for persistence.
class MuteControl : public aspl::MuteControl {
 public:
  static constexpr SInt32 INVALID_VALUE = -1;

  explicit MuteControl(std::shared_ptr<const aspl::Context> context,
                       const MuteControlParameters& params = {});

  virtual ~MuteControl() = default;

  OSStatus SetIsMutedImpl(bool value) override;

 private:
  aspl::Storage storage_;
  const std::string storageKey_;
  std::atomic<SInt32> pendingValue_{INVALID_VALUE};
};

}  // namespace ProxyAudio
