/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Volume control implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include "StreamObject.hpp"
#include <algorithm>

namespace ProxyAudio {

//==================================================================================================
// Volume Control
//==================================================================================================

class VolumeControl : public AudioObject {
public:
  VolumeControl(AudioObjectID object_id, StreamObject::Direction direction);
  ~VolumeControl() override = default;

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

  // Volume-specific methods
  Float32 GetVolumeDB() const { return volume_db_.Get(); }
  void SetVolumeDB(Float32 db);

  Float32 GetVolumeScalar() const;
  void SetVolumeScalar(Float32 scalar);

private:
  const StreamObject::Direction direction_;
  Property<Float32> volume_db_{0.0f};

  static Float32 DBToScalar(Float32 db);
  static Float32 ScalarToDB(Float32 scalar);
};

} // namespace ProxyAudio
