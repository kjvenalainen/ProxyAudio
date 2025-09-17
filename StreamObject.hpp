/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Stream object implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"

namespace ProxyAudio {

//==================================================================================================
// Stream Object
//==================================================================================================

class StreamObject : public AudioObject {
public:
  enum class Direction { Input, Output };

  StreamObject(AudioObjectID object_id, Direction direction);
  ~StreamObject() override = default;

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

  // Stream-specific methods
  Direction GetDirection() const { return direction_; }
  bool IsActive() const { return is_active_.Get(); }
  void SetActive(bool active) { is_active_.Set(active); }

  AudioStreamBasicDescription GetFormat() const;

private:
  const Direction direction_;
  Property<bool> is_active_{true};
};

} // namespace ProxyAudio
