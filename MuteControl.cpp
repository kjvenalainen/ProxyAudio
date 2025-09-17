/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Mute control implementation for ProxyAudio driver.
*/

#include "MuteControl.hpp"
#include "Common.hpp"

namespace ProxyAudio {

//==================================================================================================
// MuteControl Implementation
//==================================================================================================

MuteControl::MuteControl(AudioObjectID object_id,
                         StreamObject::Direction direction)
    : AudioObject(object_id, kAudioMuteControlClassID), direction_(direction) {}

bool MuteControl::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
  case kAudioBooleanControlPropertyValue:
    return true;
  default:
    return false;
  }
}

bool MuteControl::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioBooleanControlPropertyValue:
    return true;
  default:
    return false;
  }
}

UInt32 MuteControl::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioControlPropertyScope:
  case kAudioControlPropertyElement:
  case kAudioBooleanControlPropertyValue:
    return sizeof(UInt32);
  default:
    return 0;
  }
}

OSStatus MuteControl::GetPropertyData(const PropertyAddress &address,
                                      UInt32 qualifier_data_size,
                                      const void *qualifier_data,
                                      UInt32 data_size, UInt32 *out_data_size,
                                      void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioMuteControlClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioMuteControlClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::Device, data_size, out_data_size,
                            out_data);
  case kAudioBooleanControlPropertyValue: {
    UInt32 muted = IsMuted() ? 1 : 0;
    return GetPropertyValue(muted, data_size, out_data_size, out_data);
  }
  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus MuteControl::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioBooleanControlPropertyValue: {
    if (data_size != sizeof(UInt32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    bool new_muted = (*static_cast<const UInt32 *>(data)) != 0;
    if (IsMuted() != new_muted) {
      SetMuted(new_muted);
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

} // namespace ProxyAudio
