/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
PlugIn object implementation for ProxyAudio driver.
*/

#pragma once

#include "AudioObject.hpp"
#include "Property.hpp"
#include <vector>

namespace ProxyAudio {

//==================================================================================================
// PlugIn Object
//==================================================================================================

class PlugInObject : public AudioObject {
public:
  PlugInObject();
  ~PlugInObject() override = default;

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

  // Plugin-specific methods
  void SetHost(AudioServerPlugInHostRef host) { host_ = host; }
  AudioServerPlugInHostRef GetHost() const { return host_; }

  std::vector<AudioObjectID> GetBoxList() const;

private:
  AudioServerPlugInHostRef host_{nullptr};
  Property<UInt32> custom_property_{0};
};

} // namespace ProxyAudio
