/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Mute control implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include "StreamObject.hpp"

namespace ProxyAudio {

//==================================================================================================
// Mute Control
//==================================================================================================

class MuteControl : public AudioObject {
public:
  MuteControl(AudioObjectID object_id, StreamObject::Direction direction);
  ~MuteControl() override = default;

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

  // Mute-specific methods
  bool IsMuted() const { return is_muted_.Get(); }
  void SetMuted(bool muted) { is_muted_.Set(muted); }

private:
  const StreamObject::Direction direction_;
  Property<bool> is_muted_{false};
};

} // namespace ProxyAudio
