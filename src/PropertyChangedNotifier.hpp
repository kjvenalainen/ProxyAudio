// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <MacTypes.h>

#include <aspl/Context.hpp>
#include <memory>

#include "CFUtils.hpp"
#include "Error.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

// PropertyChangedNotifier provides a simple notification mechanism
// when a given property changes on a target.
class PropertyChangedNotifier {
 public:
  using CallbackType = std::function<void()>;

  // Empty proxy, does nothing.
  PropertyChangedNotifier()
      : targetObjectID_(kAudioObjectUnknown),
        context_(nullptr),
        address_(),
        callback_() {}

  PropertyChangedNotifier(const AudioObjectID targetObjectID,
                          std::shared_ptr<const aspl::Context> context,
                          const AudioObjectPropertyAddress& address,
                          const CallbackType& callback)
      : targetObjectID_(targetObjectID),
        context_(context),
        address_(address),
        callback_(callback) {
    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "PropertyChangedNotifier:PropertyChangedNotifier() Creating "
                  "proxy for property: "
                  "%s",
                  ToString(address_).c_str());
    RegisterPropertyChangeCallback();
  }

  PropertyChangedNotifier(const PropertyChangedNotifier& other) = delete;
  PropertyChangedNotifier& operator=(const PropertyChangedNotifier& other) =
      delete;
  PropertyChangedNotifier(PropertyChangedNotifier&& other) noexcept = delete;
  PropertyChangedNotifier& operator=(PropertyChangedNotifier&& other) noexcept =
      delete;

  ~PropertyChangedNotifier() {
    // Must not throw from a destructor (implicitly noexcept in C++11+).
    try {
      UnregisterPropertyChangeCallback();
    } catch (...) {
      // Best-effort: the listener may already have been removed if the
      // target device was reconfigured underneath us.
    }
  }

 protected:
  static OSStatus OnPropertyChangedDispatch(
      AudioObjectID inObjectID,
      UInt32 inNumberAddresses,
      const AudioObjectPropertyAddress* inAddresses,
      void* inClientData) {
    auto* notifier = static_cast<PropertyChangedNotifier*>(inClientData);
    if (!notifier) {
      return kAudioHardwareIllegalOperationError;
    }

    // This callback is entered from CoreAudio C code. A callback exception
    // must not be allowed to unwind into coreaudiod.
    try {
      notifier->OnPropertyChanged(inObjectID, inNumberAddresses, inAddresses);
    } catch (const std::exception& e) {
      notifier->LogCallbackFailure(e.what());
    } catch (...) {
      notifier->LogCallbackFailure("unknown exception");
    }
    return noErr;
  }

  void OnPropertyChanged(AudioObjectID inObjectID,
                         UInt32 inNumberAddresses,
                         const AudioObjectPropertyAddress* inAddresses) {
    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(
            ProxyAudio::Tracer::Info,
            "PropertyChangedNotifier:OnPropertyChanged() %u properties changed",
            inNumberAddresses);
    callback_();
  }

  void LogCallbackFailure(const char* message) const noexcept {
    try {
      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Error,
                    "PropertyChangedNotifier:OnPropertyChanged() Callback "
                    "failed for %s: %s",
                    ToString(address_).c_str(), message);
    } catch (...) {
      // A logging failure must not cross the CoreAudio callback boundary.
    }
  }

  void RegisterPropertyChangeCallback() {
    const auto status = AudioObjectAddPropertyListener(
        targetObjectID_, &address_, &OnPropertyChangedDispatch, this);
    if (status != noErr) {
      throw OSStatusError(status, address_);
    }

    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "PropertyChangedNotifier:RegisterPropertyChangeCallback() "
                  "Registered "
                  "property change callback for property: %s",
                  FourCC(address_.mSelector).c_str());
  }

  void UnregisterPropertyChangeCallback() {
    const auto status = AudioObjectRemovePropertyListener(
        targetObjectID_, &address_, &OnPropertyChangedDispatch, this);

    if (status != noErr) {
      throw OSStatusError(status, address_);
    }

    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "PropertyChangedNotifier:UnregisterPropertyChangeCallback() "
                  "Unregistered "
                  "property change callback for property: %s",
                  FourCC(address_.mSelector).c_str());
  }

  const AudioObjectID targetObjectID_;
  const std::shared_ptr<const aspl::Context> context_;
  const AudioObjectPropertyAddress address_;
  const CallbackType callback_;
};

}  // namespace ProxyAudio
