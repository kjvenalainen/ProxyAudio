/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Property address inheriting from AudioObjectPropertyAddress for seamless
integration.
*/

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>

namespace ProxyAudio {

//==================================================================================================
// Property System
//==================================================================================================

class PropertyAddress : public AudioObjectPropertyAddress {
public:
  // Constructor from AudioObjectPropertyAddress directly
  constexpr PropertyAddress(const AudioObjectPropertyAddress &address)
      : AudioObjectPropertyAddress(address) {}

  // Constructor from pointer to AudioObjectPropertyAddress
  PropertyAddress(const AudioObjectPropertyAddress *address)
      : AudioObjectPropertyAddress(address ? *address
                                           : AudioObjectPropertyAddress{}) {}

  // Convenient accessors (can also use mSelector, mScope, mElement directly)
  constexpr AudioObjectPropertySelector GetSelector() const noexcept {
    return mSelector;
  }
  constexpr AudioObjectPropertyScope GetScope() const noexcept {
    return mScope;
  }
  constexpr AudioObjectPropertyElement GetElement() const noexcept {
    return mElement;
  }

  // Get reference to base class (for compatibility)d
  constexpr const AudioObjectPropertyAddress &Get() const noexcept {
    return *this;
  }

  constexpr bool operator==(const PropertyAddress &other) const noexcept {
    return mSelector == other.mSelector && mScope == other.mScope &&
           mElement == other.mElement;
  }
};

} // namespace ProxyAudio
