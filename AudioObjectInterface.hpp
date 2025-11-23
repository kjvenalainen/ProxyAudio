// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

namespace ProxyAudio {

class AudioObjectInterface {
 public:
  virtual ~AudioObjectInterface() = default;

  virtual Boolean HasProperty(pid_t inClientProcessID,
                              const AudioObjectPropertyAddress* inAddress) = 0;

  virtual OSStatus IsPropertySettable(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      Boolean* outIsSettable) = 0;

  virtual OSStatus GetPropertyDataSize(
      pid_t inClientProcessID,
      const AudioObjectPropertyAddress* inAddress,
      UInt32 inQualifierDataSize,
      const void* inQualifierData,
      UInt32* outDataSize) = 0;

  virtual OSStatus GetPropertyData(pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress* inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void* inQualifierData,
                                   UInt32 inDataSize,
                                   UInt32* outDataSize,
                                   void* outData) = 0;

  virtual OSStatus SetPropertyData(pid_t inClientProcessID,
                                   const AudioObjectPropertyAddress* inAddress,
                                   UInt32 inQualifierDataSize,
                                   const void* inQualifierData,
                                   UInt32 inDataSize,
                                   const void* inData) = 0;

  // All audio objects have an AudioObjectID which should be unique within
  // the plug-in.
  const AudioObjectID Id() const { return id_; }

  // Easier access to the class ID.
  const AudioClassID ClassId() const { return classId_; }

  // Easier access to the object as a specific type.
  template <
      typename T,
      std::enable_if_t<std::is_base_of_v<AudioObjectInterface, T>, int> = 0>
  T& As() {
    return reinterpret_cast<T&>(*this);
  }

 protected:
  AudioObjectInterface(const AudioObjectID id, const AudioClassID classId)
      : id_(id), classId_(classId) {}

  AudioObjectID id_;
  AudioClassID classId_;
};

}  // namespace ProxyAudio
