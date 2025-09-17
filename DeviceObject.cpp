/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Device object implementation for ProxyAudio driver.
*/

#include "DeviceObject.hpp"
#include "Common.hpp"
#include <algorithm>
#include <mach/mach_time.h>

namespace ProxyAudio {

//==================================================================================================
// DeviceObject Implementation
//==================================================================================================

DeviceObject::DeviceObject()
    : AudioObject(Constants::Device, kAudioDeviceClassID) {
  UpdateHostTicksPerFrame();
}

void DeviceObject::SetSampleRate(Float64 rate) {
  sample_rate_.Set(rate);
  UpdateHostTicksPerFrame();
}

void DeviceObject::UpdateHostTicksPerFrame() {
  mach_timebase_info_data_t timebase_info;
  mach_timebase_info(&timebase_info);
  Float64 host_clock_frequency =
      (Float64)timebase_info.denom / (Float64)timebase_info.numer;
  host_clock_frequency *= 1000000000.0; // Convert to Hz
  host_ticks_per_frame_ = host_clock_frequency / GetSampleRate();
}

OSStatus DeviceObject::StartIO() {
  UInt64 expected = 0;
  if (io_running_count_.compare_exchange_strong(expected, 1)) {
    // First client starting IO
    std::lock_guard lock(timing_mutex_);
    number_timestamps_ = 0;
    anchor_sample_time_ = 0;
    anchor_host_time_ = mach_absolute_time();
    return noErr;
  } else if (expected != UINT64_MAX) {
    // Additional client
    io_running_count_.fetch_add(1);
    return noErr;
  }
  return kAudioHardwareIllegalOperationError;
}

OSStatus DeviceObject::StopIO() {
  UInt64 current = io_running_count_.load();
  if (current == 0) {
    return kAudioHardwareIllegalOperationError;
  } else if (current == 1) {
    // Last client stopping IO
    io_running_count_.store(0);
    return noErr;
  } else {
    // Other clients still running
    io_running_count_.fetch_sub(1);
    return noErr;
  }
}

OSStatus DeviceObject::GetZeroTimeStamp(Float64 *out_sample_time,
                                        UInt64 *out_host_time,
                                        UInt64 *out_seed) {
  std::lock_guard lock(timing_mutex_);

  Float64 host_ticks_per_ring_buffer =
      host_ticks_per_frame_ * Constants::kRingBufferSize;
  Float64 host_tick_offset =
      (number_timestamps_ + 1) * host_ticks_per_ring_buffer;
  UInt64 next_host_time =
      anchor_host_time_ + static_cast<UInt64>(host_tick_offset);

  UInt64 current_host_time = mach_absolute_time();
  if (next_host_time <= current_host_time) {
    ++number_timestamps_;
  }

  *out_sample_time = number_timestamps_ * Constants::kRingBufferSize;
  *out_host_time =
      anchor_host_time_ + (number_timestamps_ * host_ticks_per_ring_buffer);
  *out_seed = 1;

  return noErr;
}

bool DeviceObject::HasProperty(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioObjectPropertyName:
  case kAudioObjectPropertyModelName:
  case kAudioObjectPropertyManufacturer:
  case kAudioObjectPropertyOwnedObjects:
  case kAudioDevicePropertyDeviceUID:
  case kAudioDevicePropertyModelUID:
  case kAudioDevicePropertyTransportType:
  case kAudioDevicePropertyRelatedDevices:
  case kAudioDevicePropertyClockDomain:
  case kAudioDevicePropertyDeviceIsAlive:
  case kAudioDevicePropertyDeviceIsRunning:
  case kAudioDevicePropertyDeviceCanBeDefaultDevice:
  case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
  case kAudioDevicePropertyLatency:
  case kAudioDevicePropertyStreams:
  case kAudioObjectPropertyControlList:
  case kAudioDevicePropertySafetyOffset:
  case kAudioDevicePropertyNominalSampleRate:
  case kAudioDevicePropertyAvailableNominalSampleRates:
  case kAudioDevicePropertyIcon:
  case kAudioDevicePropertyIsHidden:
  case kAudioDevicePropertyPreferredChannelsForStereo:
  case kAudioDevicePropertyPreferredChannelLayout:
    return true;
  default:
    return false;
  }
}

bool DeviceObject::IsPropertySettable(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioDevicePropertyNominalSampleRate:
    return true;
  default:
    return false;
  }
}

UInt32 DeviceObject::GetPropertyDataSize(const PropertyAddress &address) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
  case kAudioObjectPropertyClass:
  case kAudioObjectPropertyOwner:
  case kAudioDevicePropertyDeviceIsAlive:
  case kAudioDevicePropertyDeviceIsRunning:
  case kAudioDevicePropertyDeviceCanBeDefaultDevice:
  case kAudioDevicePropertyDeviceCanBeDefaultSystemDevice:
  case kAudioDevicePropertyLatency:
  case kAudioDevicePropertySafetyOffset:
  case kAudioDevicePropertyIsHidden:
    return sizeof(UInt32);
  case kAudioObjectPropertyName:
  case kAudioObjectPropertyModelName:
  case kAudioObjectPropertyManufacturer:
  case kAudioDevicePropertyDeviceUID:
  case kAudioDevicePropertyModelUID:
    return sizeof(CFStringRef);
  case kAudioDevicePropertyNominalSampleRate:
    return sizeof(Float64);
  case kAudioDevicePropertyStreams:
    return 2 * sizeof(AudioObjectID); // Input and output streams
  case kAudioObjectPropertyControlList:
    return 6 * sizeof(AudioObjectID); // All controls
  default:
    return 0;
  }
}

OSStatus DeviceObject::GetPropertyData(const PropertyAddress &address,
                                       UInt32 qualifier_data_size,
                                       const void *qualifier_data,
                                       UInt32 data_size, UInt32 *out_data_size,
                                       void *out_data) const {
  switch (address.GetSelector()) {
  case kAudioObjectPropertyBaseClass:
    return GetPropertyValue(kAudioDeviceClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyClass:
    return GetPropertyValue(kAudioDeviceClassID, data_size, out_data_size,
                            out_data);
  case kAudioObjectPropertyOwner:
    return GetPropertyValue(Constants::Box, data_size, out_data_size, out_data);
  case kAudioObjectPropertyName: {
    CFStringRef name = CFSTR("ProxyAudio Device");
    return GetPropertyValue(name, data_size, out_data_size, out_data);
  }
  case kAudioDevicePropertyDeviceUID: {
    CFStringRef uid = CFStringCreateWithCString(
        nullptr, Constants::kDeviceUID.data(), kCFStringEncodingUTF8);
    auto result = GetPropertyValue(uid, data_size, out_data_size, out_data);
    if (uid)
      CFRelease(uid);
    return result;
  }
  case kAudioDevicePropertyNominalSampleRate:
    return GetPropertyValue(GetSampleRate(), data_size, out_data_size,
                            out_data);
  case kAudioDevicePropertyDeviceIsRunning: {
    UInt32 running = IsIORunning() ? 1 : 0;
    return GetPropertyValue(running, data_size, out_data_size, out_data);
  }
  default:
    return kAudioHardwareUnknownPropertyError;
  }
}

OSStatus DeviceObject::SetPropertyData(
    const PropertyAddress &address, UInt32 qualifier_data_size,
    const void *qualifier_data, UInt32 data_size, const void *data,
    UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  switch (address.GetSelector()) {
  case kAudioDevicePropertyNominalSampleRate: {
    if (data_size != sizeof(Float64)) {
      return kAudioHardwareBadPropertySizeError;
    }
    Float64 new_rate = *static_cast<const Float64 *>(data);
    if (GetSampleRate() != new_rate) {
      SetSampleRate(new_rate);
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

std::vector<AudioObjectID> DeviceObject::GetStreamList() const {
  return {Constants::StreamInput, Constants::StreamOutput};
}

std::vector<AudioObjectID> DeviceObject::GetControlList() const {
  return {Constants::VolumeInputMaster,     Constants::VolumeOutputMaster,
          Constants::MuteInputMaster,       Constants::MuteOutputMaster,
          Constants::DataSourceInputMaster, Constants::DataSourceOutputMaster};
}

} // namespace ProxyAudio
