/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Device object implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include <atomic>
#include <mutex>
#include <vector>

namespace ProxyAudio {

//==================================================================================================
// Device Object
//==================================================================================================

class DeviceObject : public AudioObject {
public:
  DeviceObject();
  ~DeviceObject() override = default;

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

  // Device-specific methods
  Float64 GetSampleRate() const { return sample_rate_.Get(); }
  void SetSampleRate(Float64 rate);

  bool IsIORunning() const { return io_running_count_.load() > 0; }
  OSStatus StartIO();
  OSStatus StopIO();

  // Timing methods
  OSStatus GetZeroTimeStamp(Float64 *out_sample_time, UInt64 *out_host_time,
                            UInt64 *out_seed);

  std::vector<AudioObjectID> GetStreamList() const;
  std::vector<AudioObjectID> GetControlList() const;

private:
  Property<Float64> sample_rate_{44100.0};
  std::atomic<UInt64> io_running_count_{0};

  // Timing state
  mutable std::mutex timing_mutex_;
  Float64 host_ticks_per_frame_{0.0};
  UInt64 number_timestamps_{0};
  Float64 anchor_sample_time_{0.0};
  UInt64 anchor_host_time_{0};

  void UpdateHostTicksPerFrame();
};

} // namespace ProxyAudio
