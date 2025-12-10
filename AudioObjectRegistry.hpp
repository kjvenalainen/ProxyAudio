// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <memory>
#include <shared_mutex>
#include <vector>

#include "AudioObjectInterface.hpp"
#include "Error.hpp"
#include "Utils.hpp"

namespace ProxyAudio {

// Dynamic registry for Audio Objects, allowing access based on ID.
class AudioObjectRegistry {
 public:
  AudioObjectRegistry() = default;

  ~AudioObjectRegistry() = default;

  // Constructs a new Audio Object, adding it to the registry with a free unique
  // ID, and
  template <typename T, typename... Args>
  std::shared_ptr<T> Construct(Args&&... args) {
    static_assert(std::is_base_of_v<AudioObjectInterface, T>,
                  "T must be derived from AudioObjectInterface");

    std::lock_guard<std::shared_mutex> lock(objects_mutex_);

    // Find a free ID.
    for (size_t i = 0; i < objects_.size(); i++) {
      if (objects_[i] == nullptr) {
        // Construct the object with the free ID, and add it to the vector.
        auto object = std::make_shared<T>(
            static_cast<AudioObjectID>(i + kAudioObjectPlugInObject), *this,
            std::forward<Args>(args)...);

        Log("Using existing ID [id: %d, numObjects: %zu]", object->Id(),
            objects_.size());

        objects_[i] = object;
        return object;
      }
    }

    // No free ID found, add a new one at the end of the vector.
    auto object = std::make_shared<T>(
        static_cast<AudioObjectID>(objects_.size() + kAudioObjectPlugInObject),
        *this, std::forward<Args>(args)...);

    Log("Using new ID [id: %d, numObjects: %zu]", object->Id(),
        objects_.size());

    objects_.push_back(object);
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

    return objects_[id - kAudioObjectPlugInObject];
  }

  // Remove the object by ID.
  void Remove(AudioObjectID id) {
    std::lock_guard<std::shared_mutex> lock(objects_mutex_);

    if (id - kAudioObjectPlugInObject >= objects_.size()) {
      Log("Object not found [id: %d, numObjects: %zu]", id, objects_.size());

      return;
    }

    Log("Removing object [id: %d, numObjects: %zu]", id, objects_.size());

    objects_[id - kAudioObjectPlugInObject].reset();
  }

 private:
  // Mutex for modifying the objects vector.
  std::shared_mutex objects_mutex_;
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
