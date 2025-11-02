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

namespace ProxyAudio {

// Dynamic registry for Audio Objects, allowing access based on ID. StartId is
// the first ID to use, and is used to offset the IDs of the objects in the
// registry.
template <size_t StartId = kAudioObjectPlugInObject>
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
        auto object =
            std::make_shared<T>(static_cast<AudioObjectID>(i + StartId),
                                std::forward<Args>(args)...);

        objects_[i] = object;
        return object;
      }
    }

    // No free ID found, add a new one at the end of the vector.
    auto object = std::make_shared<T>(
        static_cast<AudioObjectID>(objects_.size() + StartId),
        std::forward<Args>(args)...);

    objects_.push_back(object);
    return object;
  }

  // Get the object by ID.
  std::shared_ptr<AudioObjectInterface> operator[](const AudioObjectID id) {
    // No need to lock, we will copy the shared pointer to the caller, extending
    // its lifetime.

    if (id - StartId >= objects_.size()) {
      return nullptr;
    }

    return objects_[id - StartId];
  }

  // Remove the object by ID.
  void Remove(AudioObjectID id) {
    std::lock_guard<std::shared_mutex> lock(objects_mutex_);

    if (id - StartId >= objects_.size()) {
      return;
    }

    objects_[id - StartId].reset();
  }

 private:
  // Mutex for modifying the objects vector.
  std::shared_mutex objects_mutex_;
  // Vector of objects, indexed by ID.
  std::vector<std::shared_ptr<AudioObjectInterface>> objects_;
};

}  // namespace ProxyAudio
