/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Base AudioObject class for ProxyAudio driver.
*/

#pragma once

#include "PropertyAddress.hpp"
#include <CoreAudio/AudioServerPlugIn.h>

namespace ProxyAudio {

//==================================================================================================
// Base Audio Object
//==================================================================================================

class AudioObject {
public:
  explicit AudioObject(AudioObjectID object_id, AudioClassID class_id)
      : object_id_(object_id), class_id_(class_id) {}

  virtual ~AudioObject() = default;

  AudioObjectID GetObjectID() const noexcept { return object_id_; }
  AudioClassID GetClassID() const noexcept { return class_id_; }

  virtual bool HasProperty(const PropertyAddress &address) const = 0;
  virtual bool IsPropertySettable(const PropertyAddress &address) const = 0;
  virtual UInt32 GetPropertyDataSize(const PropertyAddress &address) const = 0;

  virtual OSStatus GetPropertyData(const PropertyAddress &address,
                                   UInt32 qualifier_data_size,
                                   const void *qualifier_data, UInt32 data_size,
                                   UInt32 *out_data_size,
                                   void *out_data) const = 0;

  virtual OSStatus
  SetPropertyData(const PropertyAddress &address, UInt32 qualifier_data_size,
                  const void *qualifier_data, UInt32 data_size,
                  const void *data, UInt32 *out_number_properties_changed,
                  AudioObjectPropertyAddress *out_changed_addresses) = 0;

protected:
  // Helper methods for common property operations
  template <typename T>
  OSStatus GetPropertyValue(const T &value, UInt32 data_size,
                            UInt32 *out_data_size, void *out_data) const {
    if (out_data_size)
      *out_data_size = sizeof(T);
    if (out_data && data_size >= sizeof(T)) {
      *static_cast<T *>(out_data) = value;
      return noErr;
    }
    return data_size < sizeof(T)
               ? static_cast<OSStatus>(kAudioHardwareBadPropertySizeError)
               : static_cast<OSStatus>(noErr);
  }

  template <typename T>
  OSStatus SetPropertyValue(T &target, const void *data, UInt32 data_size) {
    if (data_size != sizeof(T)) {
      return kAudioHardwareBadPropertySizeError;
    }
    target = *static_cast<const T *>(data);
    return noErr;
  }

private:
  const AudioObjectID object_id_;
  const AudioClassID class_id_;
};

} // namespace ProxyAudio
