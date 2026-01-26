// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <aspl/Context.hpp>
#include <aspl/Device.hpp>

namespace ProxyAudio {

// Traits helper to extract the base type and parameters type from the proxy
// type. Deriving classes must specialize this template to provide the correct
// types.
template <typename ProxyType>
struct BaseTraits {
  typedef typename std::false_type BaseType;
  typedef typename std::false_type ParametersType;
};

// Generic ProxyObject class that proxies all of the properties from the target
// object on creation. Uses CRTP pattern to let deriving classes define the
// types and behavior. All deriving classes must define:
//
// - ParametersType: The type of the parameters to be used for the proxy object.
// - BaseType: The base type of the proxy object (must be public).
// - GetParameters: A static method that returns the initial parameters for the
// proxy object.
template <typename ProxyType>
class ProxyObject : public BaseTraits<ProxyType>::BaseType {
 public:
  template <typename... Args>
  explicit ProxyObject(const AudioObjectID targetObjectID, Args&&... args)
      : BaseTraits<ProxyType>::BaseType(std::forward<Args>(args)...),
        targetObjectID_(targetObjectID) {}

  virtual ~ProxyObject() = default;

  AudioObjectID GetTargetObjectID() const { return targetObjectID_; }

 private:
  const AudioObjectID targetObjectID_;
};

}  // namespace ProxyAudio
