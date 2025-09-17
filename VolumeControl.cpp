/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Volume control implementation for ProxyAudio driver.
*/

#include "VolumeControl.hpp"
#include "Common.hpp"
#include <algorithm>
#include <cmath>

namespace ProxyAudio {

//==================================================================================================
// VolumeControl Implementation
//==================================================================================================

VolumeControl::VolumeControl(AudioObjectID object_id,
                             StreamObject::Direction direction)
    : AudioObject(object_id, kAudioVolumeControlClassID),
      direction_(direction) {}

bool VolumeControl::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
  case kAudioLevelControlPropertyScalarValue:
  case kAudioLevelControlPropertyDecibelValue:
  case kAudioLevelControlPropertyDecibelRange:
    return true;
  default:
    return false;
  }
}

bool VolumeControl::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioLevelControlPropertyScalarValue:
  case kAudioLevelControlPropertyDecibelValue:
    return true;
  default:
    return false;
  }
}

UInt32
VolumeControl::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
    return sizeof(UInt32);
  case kAudioLevelControlPropertyScalarValue:
  case kAudioLevelControlPropertyDecibelValue:
    return sizeof(Float32);
  case kAudioLevelControlPropertyDecibelRange:
    return sizeof(AudioValueRange);
  default:
    return 0;
  }
}

OSStatus VolumeControl::GetPropertyData(const PropertyAddress &address,
                                        UInt32 qualifier_data_size,
                                        const void *qualifier_data,
                                        UInt32 data_size, UInt32 *out_data_size,
                                        void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioVolumeControlClassID, data_size,
                            out_data_size, out_data);
  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioVolumeControlClassID, data_size,
                            out_data_size, out_data);
  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::Device, data_size, out_data_size,
                            out_data);
  case kAudioLevelControlPropertyDecibelValue:
    return GetPropertyValue(GetVolumeDB(), data_size, out_data_size, out_data);
  case kAudioLevelControlPropertyScalarValue:
    return GetPropertyValue(GetVolumeScalar(), data_size, out_data_size,
                            out_data);
  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus VolumeControl::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioLevelControlPropertyDecibelValue: {
    if (data_size != sizeof(Float32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    Float32 new_db = *static_cast<const Float32 *>(data);
    if (GetVolumeDB() != new_db) {
      SetVolumeDB(new_db);
      if (out_number_properties_changed)
        *out_number_properties_changed = 1;
      if (out_changed_addresses) {
        out_changed_addresses[0] = address.Get();
      }
    }
    return noErr;
  }
  case kAudioLevelControlPropertyScalarValue: {
    if (data_size != sizeof(Float32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    Float32 new_scalar = *static_cast<const Float32 *>(data);
    if (GetVolumeScalar() != new_scalar) {
      SetVolumeScalar(new_scalar);
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

Float32 VolumeControl::GetVolumeScalar() const {
  return DBToScalar(GetVolumeDB());
}

void VolumeControl::SetVolumeDB(Float32 db) {
  volume_db_.Set(
      std::clamp(db, Constants::kVolumeMinDB, Constants::kVolumeMaxDB));
}

void VolumeControl::SetVolumeScalar(Float32 scalar) {
  SetVolumeDB(ScalarToDB(scalar));
}

Float32 VolumeControl::DBToScalar(Float32 db) {
  return std::pow(10.0f, db / 20.0f);
}

Float32 VolumeControl::ScalarToDB(Float32 scalar) {
  return 20.0f * std::log10(std::max(scalar, 0.0001f));
}

} // namespace ProxyAudio
