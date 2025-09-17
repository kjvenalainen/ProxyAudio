/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Stream object implementation for ProxyAudio driver.
*/

#include "StreamObject.hpp"
#include "Common.hpp"

namespace ProxyAudio {

//==================================================================================================
// StreamObject Implementation
//==================================================================================================

StreamObject::StreamObject(AudioObjectID object_id, Direction direction)
    : AudioObject(object_id, kAudioStreamClassID), direction_(direction) {}

bool StreamObject::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioStreamPropertyDirection:
  case kAudioStreamPropertyTerminalType:
  case kAudioStreamPropertyStartingChannel:
  case kAudioStreamPropertyLatency:
  case kAudioStreamPropertyVirtualFormat:
  case kAudioStreamPropertyPhysicalFormat:
  case kAudioStreamPropertyAvailableVirtualFormats:
  case kAudioStreamPropertyAvailablePhysicalFormats:
  case kAudioStreamPropertyIsActive:
    return true;
  default:
    return false;
  }
}

bool StreamObject::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioStreamPropertyIsActive:
    return true;
  default:
    return false;
  }
}

UInt32 StreamObject::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioStreamPropertyDirection:
  case kAudioStreamPropertyTerminalType:
  case kAudioStreamPropertyStartingChannel:
  case kAudioStreamPropertyLatency:
  case kAudioStreamPropertyIsActive:
    return sizeof(UInt32);
  case kAudioStreamPropertyVirtualFormat:
  case kAudioStreamPropertyPhysicalFormat:
    return sizeof(AudioStreamBasicDescription);
  default:
    return 0;
  }
}

OSStatus StreamObject::GetPropertyData(const PropertyAddress &address,
                                       UInt32 qualifier_data_size,
                                       const void *qualifier_data,
                                       UInt32 data_size, UInt32 *out_data_size,
                                       void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioStreamClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioStreamClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::Device, data_size, out_data_size,
                            out_data);
  case kAudioStreamPropertyDirection: {
    UInt32 dir = (direction_ == Direction::Input) ? 1 : 0;
    return GetPropertyValue(dir, data_size, out_data_size, out_data);
  }
  case kAudioStreamPropertyIsActive: {
    UInt32 active = IsActive() ? 1 : 0;
    return GetPropertyValue(active, data_size, out_data_size, out_data);
  }
  case kAudioStreamPropertyVirtualFormat:
  case kAudioStreamPropertyPhysicalFormat:
    return GetPropertyValue(GetFormat(), data_size, out_data_size, out_data);
  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus StreamObject::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioStreamPropertyIsActive: {
    if (data_size != sizeof(UInt32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    bool new_active = (*static_cast<const UInt32 *>(data)) != 0;
    if (IsActive() != new_active) {
      SetActive(new_active);
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

AudioStreamBasicDescription StreamObject::GetFormat() const {
  AudioStreamBasicDescription format = {};
  format.mSampleRate = 44100.0;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
  format.mBytesPerPacket = 8;
  format.mFramesPerPacket = 1;
  format.mBytesPerFrame = 8;
  format.mChannelsPerFrame = 2;
  format.mBitsPerChannel = 32;
  format.mReserved = 0;
  return format;
}

} // namespace ProxyAudio
