// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>

#include <memory>
#include <vector>

#include "Context.hpp"
#include "Error.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

struct ObjectProperties {
  AudioClassID BaseClass;
  AudioClassID Class;
};

enum Scope : unsigned int {
  Global = 0,
  Input = 1,
  Output = 2,
};

static constexpr Scope FromPropertyScope(AudioObjectPropertyScope scope) {
  switch (scope) {
    case kAudioObjectPropertyScopeGlobal:
      return Scope::Global;
    case kAudioObjectPropertyScopeInput:
      return Scope::Input;
    case kAudioObjectPropertyScopeOutput:
      return Scope::Output;
    default:
      throw ErrorWithCode(kAudioHardwareBadObjectError, "Invalid scope");
  }
}

// This is the base level class for all audio objects.
class Object : public std::enable_shared_from_this<Object> {
 protected:
  Object(std::shared_ptr<ProxyAudio::Context> context,
         const ObjectProperties& properties)
      : context_(std::move(context)), properties_(properties) {}

 public:
  virtual ~Object() = default;

  const AudioObjectID Id() const { return id_; }

  const AudioClassID BaseClassId() const { return properties_.BaseClass; }

  const AudioClassID ClassId() const { return properties_.Class; }

  const std::shared_ptr<Object> Parent() const { return parent_.lock(); }

  Boolean HasProperty(pid_t inClientProcessID,
                      const AudioObjectPropertyAddress* inAddress) {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
        return true;
      default:
        return HasPropertyInternal(inClientProcessID, inAddress);
    }
  };

  OSStatus IsPropertySettable(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress,
                              Boolean* outIsSettable) {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
      case kAudioObjectPropertyClass:
      case kAudioObjectPropertyOwner:
      case kAudioObjectPropertyOwnedObjects:
        *outIsSettable = false;
        break;
      default:
        return IsPropertySettableInternal(inClientProcessID, inAddress,
                                          outIsSettable);
    }

    return S_OK;
  }

  OSStatus GetPropertyDataSize(pid_t inClientProcessID,
                               const AudioObjectPropertyAddress* inAddress,
                               UInt32 inQualifierDataSize,
                               const void* inQualifierData,
                               UInt32* outDataSize) {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        *outDataSize = sizeof(AudioClassID);
        break;
      case kAudioObjectPropertyClass:
        *outDataSize = sizeof(AudioClassID);
        break;
      case kAudioObjectPropertyOwner:
        *outDataSize = sizeof(AudioObjectID);
        break;
      case kAudioObjectPropertyOwnedObjects: {
        size_t numObjects = 0;
        switch (inAddress->mScope) {
          case kAudioObjectPropertyScopeGlobal:
            numObjects = children_[Scope::Global].size() +
                         children_[Scope::Input].size() +
                         children_[Scope::Output].size();
            break;
          case kAudioObjectPropertyScopeInput:
            numObjects = children_[Scope::Input].size();
            break;
          case kAudioObjectPropertyScopeOutput:
            numObjects = children_[Scope::Output].size();
            break;
        }
        *outDataSize = static_cast<UInt32>(numObjects * sizeof(AudioObjectID));
        break;
      }
      default:
        return GetPropertyDataSizeInternal(inClientProcessID, inAddress,
                                           inQualifierDataSize, inQualifierData,
                                           outDataSize);
    }

    return S_OK;
  }

  OSStatus GetPropertyData(pid_t inClientProcessID,
                           const AudioObjectPropertyAddress* inAddress,
                           UInt32 inQualifierDataSize,
                           const void* inQualifierData,
                           UInt32 inDataSize,
                           UInt32* outDataSize,
                           void* outData) {
    switch (inAddress->mSelector) {
      case kAudioObjectPropertyBaseClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Object kAudioObjectPropertyBaseClass"));
        *((AudioClassID*)outData) = properties_.BaseClass;
        *outDataSize = sizeof(AudioClassID);
        break;
      case kAudioObjectPropertyClass:
        EXPECT(inDataSize >= sizeof(AudioClassID),
               BadDataSizeError("Object kAudioObjectPropertyClass"));
        *((AudioClassID*)outData) = properties_.Class;
        *outDataSize = sizeof(AudioClassID);
        break;
      case kAudioObjectPropertyOwner:
        EXPECT(inDataSize >= sizeof(AudioObjectID),
               BadDataSizeError("Object kAudioObjectPropertyOwner"));
        *((AudioObjectID*)outData) = id_;
        *outDataSize = sizeof(AudioObjectID);
        break;
      case kAudioObjectPropertyOwnedObjects: {
        auto outSpan =
            SafeSpan(static_cast<AudioObjectID*>(outData), inDataSize);
        size_t outIdx = 0;

        // Converter lambda to extract AudioObjectID from Object pointer.
        const auto extractId = [](const std::shared_ptr<Object>& obj) {
          return obj ? obj->Id() : kAudioObjectUnknown;
        };

        switch (inAddress->mScope) {
          case kAudioObjectPropertyScopeGlobal:
            // Return global + input + output.
            outIdx += SafeCopyToSpan(outSpan.subspan(outIdx),
                                     children_[Scope::Global], extractId);
            outIdx += SafeCopyToSpan(outSpan.subspan(outIdx),
                                     children_[Scope::Input], extractId);
            outIdx += SafeCopyToSpan(outSpan.subspan(outIdx),
                                     children_[Scope::Output], extractId);
            break;
          case kAudioObjectPropertyScopeInput:
            outIdx += SafeCopyToSpan(outSpan.subspan(outIdx),
                                     children_[Scope::Input], extractId);
            break;
          case kAudioObjectPropertyScopeOutput:
            outIdx += SafeCopyToSpan(outSpan.subspan(outIdx),
                                     children_[Scope::Output], extractId);
            break;
        }

        *outDataSize = 0;
        break;
      }
      default:
        return GetPropertyDataInternal(inClientProcessID, inAddress,
                                       inQualifierDataSize, inQualifierData,
                                       inDataSize, outDataSize, outData);
    }
    return S_OK;
  }

  virtual OSStatus SetPropertyData(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      const void* inData,
      std::vector<AudioObjectPropertyAddress>& changedAddresses) = 0;

 protected:
  // Adds a child, taking ownership of the child.
  void AddChild(std::shared_ptr<Object> child, Scope scope) {
    child->SetOwner(weak_from_this());

    children_[scope].push_back(std::move(child));
  }

  // Called when an object is adopted into the object tree. This also
  // registers the object in the registry and assigns it an ID.
  void SetOwner(std::weak_ptr<Object> parent) {
    if (!parent_.expired()) {
      throw ErrorWithCode(kAudioHardwareBadObjectError,
                          "Object already has a parent");
    }

    parent_ = std::move(parent);
    id_ = context_->Registry->AddObject(shared_from_this());
  }

  virtual Boolean HasPropertyInternal(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress) = 0;

  virtual OSStatus IsPropertySettableInternal(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      Boolean* outIsSettable) = 0;

  virtual OSStatus GetPropertyDataSizeInternal(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32* outDataSize) = 0;

  virtual OSStatus GetPropertyDataInternal(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      UInt32* outDataSize,
      void* outData) = 0;

  virtual OSStatus SetPropertyDataInternal(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32 inDataSize,
      const void* inData,
      std::vector<AudioObjectPropertyAddress>& changedAddresses) = 0;

  std::shared_ptr<Context> context_;
  ObjectProperties properties_;
  AudioObjectID id_ = kAudioObjectUnknown;
  std::weak_ptr<Object> parent_;
  std::array<std::vector<std::shared_ptr<Object>>, 3>
      children_;  // Ordered by scope: global, input, output.
};

}  // namespace ProxyAudio
