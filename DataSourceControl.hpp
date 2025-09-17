/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Data source control implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include "StreamObject.hpp"
#include <string>
#include <vector>

namespace ProxyAudio {

//==================================================================================================
// Data Source Control
//==================================================================================================

class DataSourceControl : public AudioObject {
public:
  DataSourceControl(AudioObjectID object_id, StreamObject::Direction direction);
  ~DataSourceControl() override = default;

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

  // DataSource-specific methods
  UInt32 GetCurrentSource() const { return current_source_.Get(); }
  void SetCurrentSource(UInt32 source);

  std::vector<UInt32> GetAvailableSources() const;
  std::string GetSourceName(UInt32 source) const;

private:
  const StreamObject::Direction direction_;
  Property<UInt32> current_source_{0};
};

} // namespace ProxyAudio
