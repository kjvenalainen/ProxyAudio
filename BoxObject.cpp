/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Box object implementation for ProxyAudio driver.
*/

#include "BoxObject.hpp"
#include "Common.hpp"

namespace ProxyAudio {

//==================================================================================================
// BoxObject Implementation
//==================================================================================================

BoxObject::BoxObject() : AudioObject(Constants::Box, kAudioBoxClassID) {
  name_ = CFSTR("Null Box");
  if (name_)
    CFRetain(name_);
}

BoxObject::~BoxObject() {
  if (name_) {
    CFRelease(name_);
  }
}

bool BoxObject::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioObjectPropertyName:
  case kAudioObjectPropertyModelName:
  case kAudioObjectPropertyManufacturer:
  case kAudioObjectPropertyElementName:
  case kAudioObjectPropertyElementCategoryName:
  case kAudioObjectPropertyElementNumberName:
  case kAudioObjectPropertyOwnedObjects:
  case kAudioBoxPropertyBoxUID:
  case kAudioBoxPropertyTransportType:
  case kAudioBoxPropertyHasAudio:
  case kAudioBoxPropertyHasVideo:
  case kAudioBoxPropertyHasMIDI:
  case kAudioBoxPropertyIsProtected:
  case kAudioBoxPropertyAcquired:
  case kAudioBoxPropertyAcquisitionFailed:
  case kAudioBoxPropertyDeviceList:
    return true;
  default:
    return false;
  }
}

bool BoxObject::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyName:
  case kAudioBoxPropertyAcquired:
    return true;
  default:
    return false;
  }
}

UInt32 BoxObject::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioBoxPropertyTransportType:
  case kAudioBoxPropertyHasAudio:
  case kAudioBoxPropertyHasVideo:
  case kAudioBoxPropertyHasMIDI:
  case kAudioBoxPropertyIsProtected:
  case kAudioBoxPropertyAcquired:
  case kAudioBoxPropertyAcquisitionFailed:
    return sizeof(UInt32);
  case kAudioObjectPropertyName:
  case kAudioObjectPropertyModelName:
  case kAudioObjectPropertyManufacturer:
  case kAudioBoxPropertyBoxUID:
    return sizeof(CFStringRef);
  case kAudioObjectPropertyOwnedObjects:
  case kAudioBoxPropertyDeviceList:
    return IsAcquired() ? sizeof(AudioObjectID) : 0;
  default:
    return 0;
  }
}

OSStatus BoxObject::GetPropertyData(const PropertyAddress &address,
                                    UInt32 qualifier_data_size,
                                    const void *qualifier_data,
                                    UInt32 data_size, UInt32 *out_data_size,
                                    void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioBoxClassID, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioBoxClassID, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::PlugIn, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyName:
    return GetPropertyValue(GetName(), data_size, out_data_size, out_data);

  case kAudioBoxPropertyBoxUID: {
    CFStringRef uid = CFStringCreateWithCString(
        nullptr, Constants::kBoxUID.data(), kCFStringEncodingUTF8);
    auto result = GetPropertyValue(uid, data_size, out_data_size, out_data);
    if (uid)
      CFRelease(uid);
    return result;
  }

  case kAudioBoxPropertyAcquired: {
    UInt32 acquired = IsAcquired() ? 1 : 0;
    return GetPropertyValue(acquired, data_size, out_data_size, out_data);
  }

  case kAudioBoxPropertyDeviceList:
  case kAudioObjectPropertyOwnedObjects:
    if (IsAcquired()) {
      return GetPropertyValue(Constants::Device, data_size, out_data_size,
                              out_data);
    } else {
      if (out_data_size)
        *out_data_size = 0;
      return noErr;
    }

  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus BoxObject::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyName: {
    if (data_size != sizeof(CFStringRef)) {
      return kAudioHardwareBadPropertySizeError;
    }
    SetName(*static_cast<const CFStringRef *>(data));
    if (out_number_properties_changed)
      *out_number_properties_changed = 1;
    if (out_changed_addresses) {
      out_changed_addresses[0] = address.Get();
    }
    return noErr;
  }
  case kAudioBoxPropertyAcquired: {
    if (data_size != sizeof(UInt32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    bool new_acquired = (*static_cast<const UInt32 *>(data)) != 0;
    if (IsAcquired() != new_acquired) {
      SetAcquired(new_acquired);
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

CFStringRef BoxObject::GetName() const {
  std::lock_guard lock(name_mutex_);
  return name_;
}

void BoxObject::SetName(CFStringRef name) {
  std::lock_guard lock(name_mutex_);
  if (name_) {
    CFRelease(name_);
  }
  name_ = name;
  if (name_) {
    CFRetain(name_);
  }
}

std::vector<AudioObjectID> BoxObject::GetDeviceList() const {
  return IsAcquired() ? std::vector<AudioObjectID>{Constants::Device}
                      : std::vector<AudioObjectID>{};
}

} // namespace ProxyAudio
