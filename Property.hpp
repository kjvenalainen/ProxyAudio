/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Thread-safe property wrapper template.
*/

#pragma once

#include <mutex>
#include <shared_mutex>

namespace ProxyAudio {

//==================================================================================================
// Property Template
//==================================================================================================

// Property value wrapper with type safety
template <typename T> class Property {
public:
  explicit Property(T initial_value) : value_(std::move(initial_value)) {}

  const T &Get() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return value_;
  }

  void Set(T new_value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    value_ = std::move(new_value);
  }

  bool CompareAndSet(const T &expected, T new_value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (value_ == expected) {
      value_ = std::move(new_value);
      return true;
    }
    return false;
  }

private:
  mutable std::shared_mutex mutex_;
  T value_;
};

} // namespace ProxyAudio
