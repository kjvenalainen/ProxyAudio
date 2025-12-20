// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

#include <memory>
#include <shared_mutex>
#include <vector>

#include "Object.hpp"

namespace ProxyAudio {

class Registry {
 public:
  Registry() = default;

  ~Registry() = default;

  AudioObjectID AddObject(std::shared_ptr<Object> object) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    const auto index = GetFreeIndex();
    const AudioObjectID newId =
        static_cast<AudioObjectID>(index + kAudioObjectPlugInObject);

    if (index < objects_.size()) {
      objects_[index] = std::move(object);
    } else {
      objects_.push_back(std::move(object));
    }

    return newId;
  }

  std::shared_ptr<Object> GetObject(AudioObjectID id) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    if (id - kAudioObjectPlugInObject >= objects_.size()) {
      throw ErrorWithCode(
          kAudioHardwareBadObjectError,
          "Object not found [id: " + std::to_string(id) +
              ", numObjects: " + std::to_string(objects_.size()) + "]");
    }

    auto object = objects_[id - kAudioObjectPlugInObject];

    if (object == nullptr) {
      throw ErrorWithCode(
          kAudioHardwareBadObjectError,
          "Object not found [id: " + std::to_string(id) +
              ", numObjects: " + std::to_string(objects_.size()) + "]");
    }

    return object;
  }

 private:
  size_t GetFreeIndex() {
    for (size_t i = 0; i < objects_.size(); i++) {
      if (objects_[i] == nullptr) {
        return i;
      }
    }

    return objects_.size();
  }

  std::shared_mutex mutex_;
  std::vector<std::shared_ptr<Object>> objects_;
};

}  // namespace ProxyAudio
