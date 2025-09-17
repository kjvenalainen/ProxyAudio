/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Data source control implementation for ProxyAudio driver.
*/

#include "DataSourceControl.hpp"
#include "Common.hpp"
#include <sstream>

namespace ProxyAudio {

//==================================================================================================
// DataSourceControl Implementation
//==================================================================================================

DataSourceControl::DataSourceControl(AudioObjectID object_id,
                                     StreamObject::Direction direction)
    : AudioObject(object_id, kAudioDataSourceControlClassID),
      direction_(direction) {}

bool DataSourceControl::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
  case kAudioSelectorControlPropertyCurrentItem:
  case kAudioSelectorControlPropertyAvailableItems:
  case kAudioSelectorControlPropertyItemName:
    return true;
  default:
    return false;
  }
}

bool DataSourceControl::IsPropertySettable(
    const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioSelectorControlPropertyCurrentItem:
    return true;
  default:
    return false;
  }
}

UInt32
DataSourceControl::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
  case kAudioSelectorControlPropertyCurrentItem:
    return sizeof(UInt32);
  case kAudioSelectorControlPropertyAvailableItems:
    return Constants::kDataSourceItems * sizeof(UInt32);
  case kAudioSelectorControlPropertyItemName:
    return sizeof(CFStringRef);
  default:
    return 0;
  }
}

OSStatus DataSourceControl::GetPropertyData(const PropertyAddress &address,
                                            UInt32 qualifier_data_size,
                                            const void *qualifier_data,
                                            UInt32 data_size,
                                            UInt32 *out_data_size,
                                            void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioDataSourceControlClassID, data_size,
                            out_data_size, out_data);
  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioDataSourceControlClassID, data_size,
                            out_data_size, out_data);
  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::Device, data_size, out_data_size,
                            out_data);
  case kAudioSelectorControlPropertyCurrentItem:
    return GetPropertyValue(GetCurrentSource(), data_size, out_data_size,
                            out_data);
  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus DataSourceControl::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioSelectorControlPropertyCurrentItem: {
    if (data_size != sizeof(UInt32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    UInt32 new_source = *static_cast<const UInt32 *>(data);
    if (GetCurrentSource() != new_source) {
      SetCurrentSource(new_source);
      if (out_number_properties_changed)
        *out_number_properties_changed = 1;
      if (out_changed_addresses) {
        out_changed_addresses[0] = address.Get();
      }
    }
    return noErr;
  }
  default:
    return kAudioHardwareUnsupportedOperationError;
  }
}

void DataSourceControl::SetCurrentSource(UInt32 source) {
  if (source < Constants::kDataSourceItems) {
    current_source_.Set(source);
  }
}

std::vector<UInt32> DataSourceControl::GetAvailableSources() const {
  std::vector<UInt32> sources;
  for (UInt32 i = 0; i < Constants::kDataSourceItems; ++i) {
    sources.push_back(i);
  }
  return sources;
}

std::string DataSourceControl::GetSourceName(UInt32 source) const {
  std::ostringstream name;
  name << "ProxyAudio Source " << source;
  return name.str();
}

} // namespace ProxyAudio
