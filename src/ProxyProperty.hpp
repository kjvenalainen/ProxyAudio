// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>
#include <MacTypes.h>

#include <aspl/Context.hpp>
#include <memory>

#include "AudioObjectUtils.hpp"
#include "CFUtils.hpp"
#include "Error.hpp"
#include "Tracer.hpp"

namespace ProxyAudio {

// ProxyProperty allows for the cloning of a specific property from a target
// object. The class will store the current value of the property in a local
// value store and provide a way to get and set the value.
//
// ValueType defines the type of the property value.
//
// During instantiation you must provide
// - void Set(const ValueType& value): Set the current value of the
// property.
// - ValueType Get() const: Get the current value of the property.
template <typename _ValueType>
class ProxyProperty {
 public:
  using ValueType = _ValueType;
  using SetterType = std::function<void(const ValueType& value)>;
  using GetterType = std::function<ValueType()>;

  // Empty proxy, does nothing.
  ProxyProperty()
      : targetObjectID_(kAudioObjectUnknown),
        context_(nullptr),
        address_(),
        setter_(),
        getter_() {}

  ProxyProperty(const AudioObjectID targetObjectID,
                std::shared_ptr<const aspl::Context> context,
                const AudioObjectPropertyAddress& address,
                const SetterType& setter,
                const GetterType& getter)
      : targetObjectID_(targetObjectID),
        context_(context),
        address_(address),
        setter_(setter),
        getter_(getter) {
    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyProperty:ProxyProperty() Creating proxy for property: "
                  "%s",
                  ToString(address_).c_str());
    RegisterPropertyChangeCallback();
  }

  ProxyProperty(const ProxyProperty& other) = delete;
  ProxyProperty& operator=(const ProxyProperty& other) = delete;
  ProxyProperty(ProxyProperty&& other) noexcept = delete;
  ProxyProperty& operator=(ProxyProperty&& other) noexcept = delete;

  virtual ~ProxyProperty() {
    // Must not throw from a destructor (implicitly noexcept in C++11+).
    try {
      UnregisterPropertyChangeCallback();
    } catch (...) {
      // Best-effort: the listener may already have been removed if the
      // target device was reconfigured underneath us.
    }
  }

  // Get the current value of the property from the target object.
  const ValueType GetValue() const {
    try {
      const auto value =
          GetPropertyData<ValueType>(targetObjectID_, address_, {});

      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyProperty:GetValue() Got value: %s = %s",
                    ToString(address_).c_str(), ToString(value).c_str());

      return value;
    } catch (const OSStatusError& e) {
      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Error,
                    "ProxyProperty:GetValue() Failed to get property: %s",
                    e.what());

      throw e;
    }
  }

  // Set the current value of the property on the target object. Does not call
  // the setter.
  void SetValue(const ValueType& value) {
    try {
      SetPropertyData(targetObjectID_, address_, {},
                      {
                          .ptr = const_cast<ValueType*>(&value),
                          .size = sizeof(ValueType),
                      });

      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Info,
                    "ProxyProperty:SetValue() Set value: %s = %s",
                    ToString(address_).c_str(), std::to_string(value).c_str());
    } catch (const OSStatusError& e) {
      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Error,
                    "ProxyProperty:SetValue() Failed to set property: %s",
                    e.what());

      throw e;
    }
  }

 protected:
  static OSStatus OnPropertyChangedDispatch(
      AudioObjectID inObjectID,
      UInt32 inNumberAddresses,
      const AudioObjectPropertyAddress* inAddresses,
      void* inClientData) {
    static_cast<ProxyProperty*>(inClientData)
        ->OnPropertyChanged(inObjectID, inNumberAddresses, inAddresses);
    return noErr;
  }

  void OnPropertyChanged(AudioObjectID inObjectID,
                         UInt32 inNumberAddresses,
                         const AudioObjectPropertyAddress* inAddresses) {
    ProxyAudio::Tracer::FromTracer(context_->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "ProxyProperty:OnPropertyChanged() %u properties changed",
                  inNumberAddresses);

    const auto newValue = GetValue();
    if (newValue != getter_()) {
      setter_(newValue);
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
                  "ProxyProperty:RegisterPropertyChangeCallback() Registered "
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
        ->Message(
            ProxyAudio::Tracer::Info,
            "ProxyProperty:UnregisterPropertyChangeCallback() Unregistered "
            "property change callback for property: %s",
            FourCC(address_.mSelector).c_str());
  }

  const AudioObjectID targetObjectID_;
  const std::shared_ptr<const aspl::Context> context_;
  const AudioObjectPropertyAddress address_;
  const SetterType setter_;
  const GetterType getter_;
};

// ProxyProperty, with internal storage of the value.
template <typename _ValueType>
class ProxyPropertyWithStorage : public ProxyProperty<_ValueType> {
 public:
  using ValueType = _ValueType;

  ProxyPropertyWithStorage() : ProxyProperty<ValueType>(), value_() {}

  ProxyPropertyWithStorage(const AudioObjectID targetObjectID,
                           std::shared_ptr<const aspl::Context> context,
                           const AudioObjectPropertyAddress& address)
      : ProxyProperty<ValueType>(
            targetObjectID,
            context,
            address,
            [this](const ValueType& value) { this->value_ = value; },
            [this]() { return this->value_; }),
        value_(this->GetValue()) {}

  virtual ~ProxyPropertyWithStorage() = default;

  // Get the current value of the property from the internal storage.
  ValueType Get() const { return value_; }

  // Set the current value of the property on the internal storage and the
  // target object.
  void Set(const ValueType& value) {
    this->SetValue(value);
    value_ = value;
  }

 protected:
  ValueType value_;
};

}  // namespace ProxyAudio
