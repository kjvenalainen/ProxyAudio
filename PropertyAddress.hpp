// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

namespace ProxyAudio {

//==================================================================================================
// Property System
//==================================================================================================

class PropertyAddress {
 public:
  // Constructor from AudioObjectPropertyAddress directly
  constexpr PropertyAddress(const AudioObjectPropertyAddress& address)
      : address_(address) {}

  // Constructor from pointer to AudioObjectPropertyAddress
  PropertyAddress(const AudioObjectPropertyAddress* address)
      : address_(address ? *address : AudioObjectPropertyAddress{}) {}

  // Convenient accessors (can also use mSelector, mScope, mElement directly)
  constexpr AudioObjectPropertySelector Selector() const noexcept {
    return address_.mSelector;
  }
  constexpr AudioObjectPropertyScope Scope() const noexcept {
    return address_.mScope;
  }
  constexpr AudioObjectPropertyElement Element() const noexcept {
    return address_.mElement;
  }

  // Get reference to base class (for compatibility)d
  constexpr const AudioObjectPropertyAddress& Get() const noexcept {
    return address_;
  }

  constexpr bool operator==(const PropertyAddress &other) const noexcept {
    return address_.mSelector == other.address_.mSelector &&
           address_.mScope == other.address_.mScope &&
           address_.mElement == other.address_.mElement;
  }

 private:
  AudioObjectPropertyAddress address_;
};

} // namespace ProxyAudio
