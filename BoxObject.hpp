/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Box object implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <mutex>
#include <vector>

namespace ProxyAudio {

//==================================================================================================
// Box Object
//==================================================================================================

class BoxObject : public AudioObject {
public:
  BoxObject();
  ~BoxObject() override;

  bool HasProperty(const PropertyAddress &address) const override;
  bool IsPropertySettable(const PropertyAddress &address) const override;
  UInt32 GetPropertyDataSize(const PropertyAddress &address) const override;

  OSStatus GetPropertyData(const PropertyAddress &address,
                           UInt32 qualifier_data_size,
                           const void *qualifier_data, UInt32 data_size,
                           UInt32 *out_data_size,
                           void *out_data) const override;

  OSStatus
  SetPropertyData(const PropertyAddress &address, UInt32 qualifier_data_size,
                  const void *qualifier_data, UInt32 data_size,
                  const void *data, UInt32 *out_number_properties_changed,
                  AudioObjectPropertyAddress *out_changed_addresses) override;

  // Box-specific methods
  bool IsAcquired() const { return acquired_.Get(); }
  void SetAcquired(bool acquired) { acquired_.Set(acquired); }

  CFStringRef GetName() const;
  void SetName(CFStringRef name);

  std::vector<AudioObjectID> GetDeviceList() const;

private:
  Property<bool> acquired_{true};
  mutable std::mutex name_mutex_;
  CFStringRef name_{nullptr};
};

} // namespace ProxyAudio
