/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
PlugIn object implementation for ProxyAudio driver.
*/

#include "PlugInObject.hpp"
#include "Common.hpp"
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>

namespace ProxyAudio {

//==================================================================================================
// PlugInObject Implementation
//==================================================================================================

PlugInObject::PlugInObject()
    : AudioObject(Constants::PlugIn, kAudioPlugInClassID) {}

bool PlugInObject::HasProperty(const PropertyAddress &address) const {
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
  case kAudioPlugInPropertyBundleID:
  case kAudioPlugInPropertyDeviceList:
  case kAudioPlugInPropertyTranslateUIDToDevice:
  case Constants::kCustomPropertyID:
    return true;
  default:
    return false;
  }
}

bool PlugInObject::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case Constants::kCustomPropertyID:
    return true;
  default:
    return false;
  }
}

UInt32 PlugInObject::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case Constants::kCustomPropertyID:
    return sizeof(UInt32);
  case kAudioObjectPropertyName:
  case kAudioObjectPropertyModelName:
  case kAudioObjectPropertyManufacturer:
  case kAudioObjectPropertyElementName:
  case kAudioObjectPropertyElementCategoryName:
  case kAudioObjectPropertyElementNumberName:
  case kAudioPlugInPropertyBundleID:
    return sizeof(CFStringRef);
  case kAudioObjectPropertyOwnedObjects:
    return static_cast<UInt32>(GetBoxList().size() * sizeof(AudioObjectID));
  case kAudioPlugInPropertyDeviceList:
    return sizeof(AudioObjectID); // Only one device
  case kAudioPlugInPropertyTranslateUIDToDevice:
    return sizeof(AudioObjectID);
  default:
    return 0;
  }
}

OSStatus PlugInObject::GetPropertyData(const PropertyAddress &address,
                                       UInt32 qualifier_data_size,
                                       const void *qualifier_data,
                                       UInt32 data_size, UInt32 *out_data_size,
                                       void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioPlugInClassID, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioPlugInClassID, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyOwner:
    return GetPropertyValue(kAudioObjectUnknown, data_size, out_data_size,
                            out_data);

  case kAudioObjectPropertyName: {
    CFStringRef name = CFSTR("ProxyAudio");
    return GetPropertyValue(name, data_size, out_data_size, out_data);
  }

  case kAudioObjectPropertyModelName: {
    CFStringRef model = CFSTR("ProxyAudio");
    return GetPropertyValue(model, data_size, out_data_size, out_data);
  }

  case kAudioObjectPropertyManufacturer: {
    CFStringRef manufacturer = CFSTR("TapTurtle");
    return GetPropertyValue(manufacturer, data_size, out_data_size, out_data);
  }

  case kAudioPlugInPropertyBundleID: {
    CFStringRef bundle_id = CFStringCreateWithCString(
        nullptr, Constants::kBundleID.data(), kCFStringEncodingUTF8);
    auto result =
        GetPropertyValue(bundle_id, data_size, out_data_size, out_data);
    if (bundle_id)
      CFRelease(bundle_id);
    return result;
  }

  case kAudioObjectPropertyOwnedObjects: {
    auto box_list = GetBoxList();
    if (out_data_size)
      *out_data_size =
          static_cast<UInt32>(box_list.size() * sizeof(AudioObjectID));
    if (out_data && data_size >= box_list.size() * sizeof(AudioObjectID)) {
      std::copy(box_list.begin(), box_list.end(),
                static_cast<AudioObjectID *>(out_data));
    }
    return noErr;
  }

  case kAudioPlugInPropertyDeviceList:
    return GetPropertyValue(Constants::Device, data_size, out_data_size,
                            out_data);

  case Constants::kCustomPropertyID:
    return GetPropertyValue(custom_property_.Get(), data_size, out_data_size,
                            out_data);

  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus PlugInObject::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case Constants::kCustomPropertyID: {
    if (data_size != sizeof(UInt32)) {
      return kAudioHardwareBadPropertySizeError;
    }
    custom_property_.Set(*static_cast<const UInt32 *>(data));
    if (out_number_properties_changed)
      *out_number_properties_changed = 1;
    if (out_changed_addresses) {
      out_changed_addresses[0] = address.Get();
    }
    return noErr;
  }
  default:
    return kAudioHardwareUnsupportedOperationError;
  }
}

std::vector<AudioObjectID> PlugInObject::GetBoxList() const {
  return {Constants::Box};
}

} // namespace ProxyAudio
