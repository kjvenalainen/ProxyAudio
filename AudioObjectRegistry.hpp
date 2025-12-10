// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <vector>

#include "AudioObjectInterface.hpp"
#include "Error.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

// Dynamic registry for Audio Objects, allowing access based on ID.
class AudioObjectRegistry {
  static constexpr intptr_t kAudioObjectPlaceholder =
      std::numeric_limits<intptr_t>::max();

 public:
  AudioObjectRegistry() = default;

  ~AudioObjectRegistry() = default;

  // Constructs a new Audio Object, adding it to the registry with a free unique
  // ID, and
  template <typename T, typename... Args>
  std::shared_ptr<T> Construct(Args&&... args) {
    static_assert(std::is_base_of_v<AudioObjectInterface, T>,
                  "T must be derived from AudioObjectInterface");

    std::unique_lock<std::mutex> lock(objects_mutex_);

    // Find a free ID.
    const auto index = GetFirstFreeIndex();
    const AudioObjectID newId =
        static_cast<AudioObjectID>(index + kAudioObjectPlugInObject);

    Log("Using new ID [id: %d, numObjects: %zu]", newId, objects_.size());

    // Store a temporary object placeholder in the vector, to avoid deadlock
    // while creating the object in case it recursively creates objects via
    // the registry.
    if (index < objects_.size()) {
      objects_[index] = {reinterpret_cast<T*>(kAudioObjectPlaceholder),
                         [](void*) {}};
    } else {
      objects_.push_back(
          {reinterpret_cast<T*>(kAudioObjectPlaceholder), [](void*) {}});
    }

    lock.unlock();

    // Construct the object with the new ID.
    auto object =
        std::make_shared<T>(newId, *this, std::forward<Args>(args)...);

    objects_[index] = object;
    return object;
  }

  // Get the object by ID.
  std::shared_ptr<AudioObjectInterface> operator[](const AudioObjectID id) {
    // No need to lock, we will copy the shared pointer to the caller, extending
    // its lifetime.

    if (id - kAudioObjectPlugInObject >= objects_.size()) {
      throw ErrorWithCode(
          kAudioHardwareBadObjectError,
          "Object not found [id: " + std::to_string(id) +
              ", numObjects: " + std::to_string(objects_.size()) + "]");
    }

    const auto& object = objects_[id - kAudioObjectPlugInObject];

    if (object == nullptr ||
        object.get() ==
            reinterpret_cast<AudioObjectInterface*>(kAudioObjectPlaceholder)) {
      throw ErrorWithCode(
          kAudioHardwareBadObjectError,
          "Object not found [id: " + std::to_string(id) +
              ", numObjects: " + std::to_string(objects_.size()) + "]");
    }

    return object;
  }

  // Remove the object by ID.
  void Remove(AudioObjectID id) {
    std::lock_guard<std::mutex> lock(objects_mutex_);

    if (id - kAudioObjectPlugInObject >= objects_.size() ||
        objects_[id - kAudioObjectPlugInObject] == nullptr) {
      Log("Object not found [id: %d, numObjects: %zu]", id, objects_.size());

      return;
    }

    Log("Removing object [id: %d, numObjects: %zu]", id, objects_.size());

    objects_[id - kAudioObjectPlugInObject].reset();
  }

 private:
  size_t GetFirstFreeIndex() {
    for (size_t i = 0; i < objects_.size(); i++) {
      if (objects_[i] == nullptr) {
        return i;
      }
    }

    return objects_.size();
  }

  // Mutex for modifying the objects vector.
  std::mutex objects_mutex_;
  // Vector of objects, indexed by ID.
  std::vector<std::shared_ptr<AudioObjectInterface>> objects_;
};

class AudioObjectRegistryRef {
 public:
  AudioObjectRegistryRef(AudioObjectRegistry& registry) : registry_(registry) {}

  ~AudioObjectRegistryRef() = default;

 protected:
  AudioObjectRegistry& registry_;
};

}  // namespace ProxyAudio
