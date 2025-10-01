// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreFoundation/CFUUID.h>

#include <memory>
#include <type_traits>

namespace ProxyAudio {

// Auto-release semantics for CF types.
template <typename T>
class CFSharedPtr : public std::shared_ptr<T> {
 public:
  CFSharedPtr(T* inPtr) : std::shared_ptr<T>(MakeShared(inPtr)) {}

  ~CFSharedPtr() = default;

  template <typename U,
            std::enable_if_t<
                std::is_same_v<std::remove_cv_t<U>, std::remove_cv_t<T*>>,
                int> = 0>
  const bool operator==(const U& other) const noexcept {
    return CFEqual(this->get(), other);
  }

 private:
  static void Deleter(T* inPtr) noexcept { CFRelease(inPtr); }

  static constexpr std::shared_ptr<T> MakeShared(T* inPtr) noexcept {
    return std::shared_ptr<T>(inPtr, Deleter);
  }
};

}  // namespace ProxyAudio
