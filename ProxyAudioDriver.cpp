/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Main ProxyAudio driver implementation integrating all components.
*/

#include "ProxyAudioDriver.hpp"
#include "BoxObject.hpp"
#include "DataSourceControl.hpp"
#include "DeviceObject.hpp"
#include "MuteControl.hpp"
#include "PlugInObject.hpp"
#include "StreamObject.hpp"
#include "VolumeControl.hpp"
#include <cstring>
#include <dispatch/dispatch.h>

namespace ProxyAudio {

//==================================================================================================
// ProxyAudioDriver Implementation
//==================================================================================================

ProxyAudioDriver::ProxyAudioDriver() { CreateAllObjects(); }

ProxyAudioDriver::~ProxyAudioDriver() { DestroyAllObjects(); }

void ProxyAudioDriver::CreateAllObjects() {
  std::unique_lock lock(objects_mutex_);

  // Create all objects
  objects_[Constants::PlugIn] = std::make_unique<PlugInObject>();
  objects_[Constants::Box] = std::make_unique<BoxObject>();
  objects_[Constants::Device] = std::make_unique<DeviceObject>();
  objects_[Constants::StreamInput] = std::make_unique<StreamObject>(
      Constants::StreamInput, StreamObject::Direction::Input);
  objects_[Constants::StreamOutput] = std::make_unique<StreamObject>(
      Constants::StreamOutput, StreamObject::Direction::Output);
  objects_[Constants::VolumeInputMaster] = std::make_unique<VolumeControl>(
      Constants::VolumeInputMaster, StreamObject::Direction::Input);
  objects_[Constants::VolumeOutputMaster] = std::make_unique<VolumeControl>(
      Constants::VolumeOutputMaster, StreamObject::Direction::Output);
  objects_[Constants::MuteInputMaster] = std::make_unique<MuteControl>(
      Constants::MuteInputMaster, StreamObject::Direction::Input);
  objects_[Constants::MuteOutputMaster] = std::make_unique<MuteControl>(
      Constants::MuteOutputMaster, StreamObject::Direction::Output);
  objects_[Constants::DataSourceInputMaster] =
      std::make_unique<DataSourceControl>(Constants::DataSourceInputMaster,
                                          StreamObject::Direction::Input);
  objects_[Constants::DataSourceOutputMaster] =
      std::make_unique<DataSourceControl>(Constants::DataSourceOutputMaster,
                                          StreamObject::Direction::Output);
}

void ProxyAudioDriver::DestroyAllObjects() {
  std::unique_lock lock(objects_mutex_);
  objects_.clear();
}

AudioObject *ProxyAudioDriver::GetAudioObject(AudioObjectID object_id) {
  std::shared_lock<std::shared_mutex> lock(objects_mutex_);
  auto it = objects_.find(object_id);
  return (it != objects_.end()) ? it->second.get() : nullptr;
}

const AudioObject *
ProxyAudioDriver::GetAudioObject(AudioObjectID object_id) const {
  std::shared_lock<std::shared_mutex> lock(objects_mutex_);
  auto it = objects_.find(object_id);
  return (it != objects_.end()) ? it->second.get() : nullptr;
}

OSStatus ProxyAudioDriver::Initialize(AudioServerPlugInHostRef host) {
  std::lock_guard lock(state_mutex_);

  if (host_) {
    return kAudioHardwareIllegalOperationError;
  }

  host_ = host;

  // Initialize the plugin object with the host
  if (auto *plugin =
          dynamic_cast<PlugInObject *>(GetAudioObject(Constants::PlugIn))) {
    plugin->SetHost(host);
  }

  return noErr;
}

OSStatus
ProxyAudioDriver::HasProperty(AudioObjectID object_id,
                              const AudioObjectPropertyAddress *address,
                              Boolean *out_has_property) {
  if (!address || !out_has_property) {
    return kAudioHardwareIllegalOperationError;
  }

  const AudioObject *object = GetAudioObject(object_id);
  if (!object) {
    return kAudioHardwareBadObjectError;
  }

  PropertyAddress prop_addr(address);
  *out_has_property = object->HasProperty(prop_addr);

  return noErr;
}

OSStatus
ProxyAudioDriver::IsPropertySettable(AudioObjectID object_id,
                                     const AudioObjectPropertyAddress *address,
                                     Boolean *out_is_settable) {
  if (!address || !out_is_settable) {
    return kAudioHardwareIllegalOperationError;
  }

  const AudioObject *object = GetAudioObject(object_id);
  if (!object) {
    return kAudioHardwareBadObjectError;
  }

  PropertyAddress prop_addr(address);
  *out_is_settable = object->IsPropertySettable(prop_addr);

  return noErr;
}

OSStatus ProxyAudioDriver::GetPropertyDataSize(
    AudioObjectID object_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data,
    UInt32 *out_data_size) {
  if (!address || !out_data_size) {
    return kAudioHardwareIllegalOperationError;
  }

  const AudioObject *object = GetAudioObject(object_id);
  if (!object) {
    return kAudioHardwareBadObjectError;
  }

  PropertyAddress prop_addr(address);
  *out_data_size = object->GetPropertyDataSize(prop_addr);

  return noErr;
}

OSStatus ProxyAudioDriver::GetPropertyData(
    AudioObjectID object_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data, UInt32 data_size,
    UInt32 *out_data_size, void *out_data) {
  if (!address) {
    return kAudioHardwareIllegalOperationError;
  }

  const AudioObject *object = GetAudioObject(object_id);
  if (!object) {
    return kAudioHardwareBadObjectError;
  }

  PropertyAddress prop_addr(address);
  return object->GetPropertyData(prop_addr, qualifier_data_size, qualifier_data,
                                 data_size, out_data_size, out_data);
}

OSStatus ProxyAudioDriver::SetPropertyData(
    AudioObjectID object_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data, UInt32 data_size,
    const void *data, UInt32 *out_number_properties_changed,
    AudioObjectPropertyAddress *out_changed_addresses) {
  if (!address || !data) {
    return kAudioHardwareIllegalOperationError;
  }

  AudioObject *object = GetAudioObject(object_id);
  if (!object) {
    return kAudioHardwareBadObjectError;
  }

  PropertyAddress prop_addr(address);
  OSStatus result = object->SetPropertyData(
      prop_addr, qualifier_data_size, qualifier_data, data_size, data,
      out_number_properties_changed, out_changed_addresses);

  // Notify the host of property changes
  if (result == noErr && host_ && out_number_properties_changed &&
      *out_number_properties_changed > 0) {
    host_->PropertiesChanged(host_, object_id, *out_number_properties_changed,
                             out_changed_addresses);
  }

  return result;
}

OSStatus ProxyAudioDriver::StartIO(AudioObjectID device_object_id,
                                   UInt32 client_id) {
  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  auto *device =
      dynamic_cast<DeviceObject *>(GetAudioObject(Constants::Device));
  if (!device) {
    return kAudioHardwareBadObjectError;
  }

  return device->StartIO();
}

OSStatus ProxyAudioDriver::StopIO(AudioObjectID device_object_id,
                                  UInt32 client_id) {
  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  auto *device =
      dynamic_cast<DeviceObject *>(GetAudioObject(Constants::Device));
  if (!device) {
    return kAudioHardwareBadObjectError;
  }

  return device->StopIO();
}

OSStatus ProxyAudioDriver::GetZeroTimeStamp(AudioObjectID device_object_id,
                                            UInt32 client_id,
                                            Float64 *out_sample_time,
                                            UInt64 *out_host_time,
                                            UInt64 *out_seed) {
  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  if (!out_sample_time || !out_host_time || !out_seed) {
    return kAudioHardwareIllegalOperationError;
  }

  auto *device =
      dynamic_cast<DeviceObject *>(GetAudioObject(Constants::Device));
  if (!device) {
    return kAudioHardwareBadObjectError;
  }

  return device->GetZeroTimeStamp(out_sample_time, out_host_time, out_seed);
}

OSStatus ProxyAudioDriver::DoIOOperation(
    AudioObjectID device_object_id, AudioObjectID stream_object_id,
    UInt32 client_id, UInt32 operation_id, UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info, void *io_main_buffer,
    void *io_secondary_buffer) {
  // For this proxy driver, we don't actually process audio data
  // Input streams produce silence, output streams consume data silently

  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  if (stream_object_id == Constants::StreamInput && io_main_buffer) {
    // Zero out input buffer (produce silence)
    std::memset(io_main_buffer, 0, io_buffer_frame_size * 2 * sizeof(Float32));
  }
  // For output streams, we just consume the data (do nothing)

  return noErr;
}

OSStatus
ProxyAudioDriver::CreateDevice(CFDictionaryRef description,
                               const AudioServerPlugInClientInfo *client_info,
                               AudioObjectID *out_device_object_id) {
  // This driver has a fixed device, so just return the existing device ID
  if (out_device_object_id) {
    *out_device_object_id = Constants::Device;
  }
  return noErr;
}

OSStatus ProxyAudioDriver::DestroyDevice(AudioObjectID device_object_id) {
  // This driver has a fixed device that cannot be destroyed
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

OSStatus ProxyAudioDriver::AddDeviceClient(
    AudioObjectID device_object_id,
    const AudioServerPlugInClientInfo *client_info) {
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

OSStatus ProxyAudioDriver::RemoveDeviceClient(
    AudioObjectID device_object_id,
    const AudioServerPlugInClientInfo *client_info) {
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

OSStatus ProxyAudioDriver::PerformDeviceConfigurationChange(
    AudioObjectID device_object_id, UInt64 change_action, void *change_info) {
  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  // Handle sample rate changes
  auto *device =
      dynamic_cast<DeviceObject *>(GetAudioObject(Constants::Device));
  if (device) {
    device->SetSampleRate(static_cast<Float64>(change_action));
  }

  return noErr;
}

OSStatus ProxyAudioDriver::AbortDeviceConfigurationChange(
    AudioObjectID device_object_id, UInt64 change_action, void *change_info) {
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

OSStatus ProxyAudioDriver::WillDoIOOperation(AudioObjectID device_object_id,
                                             UInt32 client_id,
                                             UInt32 operation_id,
                                             Boolean *out_will_do,
                                             Boolean *out_will_do_in_place) {
  if (device_object_id != Constants::Device) {
    return kAudioHardwareBadObjectError;
  }

  if (out_will_do)
    *out_will_do = true;
  if (out_will_do_in_place)
    *out_will_do_in_place = true;

  return noErr;
}

OSStatus ProxyAudioDriver::BeginIOOperation(
    AudioObjectID device_object_id, UInt32 client_id, UInt32 operation_id,
    UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info) {
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

OSStatus ProxyAudioDriver::EndIOOperation(
    AudioObjectID device_object_id, UInt32 client_id, UInt32 operation_id,
    UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info) {
  return device_object_id == Constants::Device ? noErr
                                               : kAudioHardwareBadObjectError;
}

//==================================================================================================
// Static C Interface Methods
//==================================================================================================

HRESULT ProxyAudioDriver::QueryInterface(void *driver, REFIID uuid,
                                         LPVOID *interface) {
  if (!IsValidDriverRef(static_cast<AudioServerPlugInDriverRef>(driver))) {
    return E_INVALIDARG;
  }

  // Compare UUID bytes directly
  CFUUIDBytes iunknown_bytes = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
  CFUUIDBytes driver_interface_bytes = {0x4D, 0x69, 0xDC, 0x7D, 0x5C, 0x96,
                                        0x4A, 0xE9, 0x8C, 0xF0, 0x6C, 0x5B,
                                        0x9F, 0x7A, 0xE5, 0x2F};

  if (std::memcmp(&uuid, &iunknown_bytes, sizeof(CFUUIDBytes)) == 0 ||
      std::memcmp(&uuid, &driver_interface_bytes, sizeof(CFUUIDBytes)) == 0) {
    *interface = GetDriverRef();
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG ProxyAudioDriver::AddRef(void *driver) {
  return 1; // Reference counting handled by system
}

ULONG ProxyAudioDriver::Release(void *driver) {
  return 1; // Reference counting handled by system
}

OSStatus ProxyAudioDriver::StaticInitialize(AudioServerPlugInDriverRef driver,
                                            AudioServerPlugInHostRef host) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().Initialize(host);
}

OSStatus ProxyAudioDriver::StaticCreateDevice(
    AudioServerPlugInDriverRef driver, CFDictionaryRef description,
    const AudioServerPlugInClientInfo *client_info,
    AudioObjectID *out_device_object_id) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().CreateDevice(description, client_info,
                                    out_device_object_id);
}

OSStatus
ProxyAudioDriver::StaticDestroyDevice(AudioServerPlugInDriverRef driver,
                                      AudioObjectID device_object_id) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().DestroyDevice(device_object_id);
}

OSStatus ProxyAudioDriver::StaticAddDeviceClient(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    const AudioServerPlugInClientInfo *client_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().AddDeviceClient(device_object_id, client_info);
}

OSStatus ProxyAudioDriver::StaticRemoveDeviceClient(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    const AudioServerPlugInClientInfo *client_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().RemoveDeviceClient(device_object_id, client_info);
}

OSStatus ProxyAudioDriver::StaticPerformDeviceConfigurationChange(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt64 change_action, void *change_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().PerformDeviceConfigurationChange(
      device_object_id, change_action, change_info);
}

OSStatus ProxyAudioDriver::StaticAbortDeviceConfigurationChange(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt64 change_action, void *change_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().AbortDeviceConfigurationChange(
      device_object_id, change_action, change_info);
}

Boolean ProxyAudioDriver::StaticHasProperty(
    AudioServerPlugInDriverRef driver, AudioObjectID object_id,
    pid_t client_process_id, const AudioObjectPropertyAddress *address) {
  if (!IsValidDriverRef(driver))
    return false;
  Boolean has_property = false;
  OSStatus result =
      GetInstance().HasProperty(object_id, address, &has_property);
  return (result == noErr) ? has_property : false;
}

OSStatus ProxyAudioDriver::StaticIsPropertySettable(
    AudioServerPlugInDriverRef driver, AudioObjectID object_id,
    pid_t client_process_id, const AudioObjectPropertyAddress *address,
    Boolean *out_is_settable) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().IsPropertySettable(object_id, address, out_is_settable);
}

OSStatus ProxyAudioDriver::StaticGetPropertyDataSize(
    AudioServerPlugInDriverRef driver, AudioObjectID object_id,
    pid_t client_process_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data,
    UInt32 *out_data_size) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().GetPropertyDataSize(
      object_id, address, qualifier_data_size, qualifier_data, out_data_size);
}

OSStatus ProxyAudioDriver::StaticGetPropertyData(
    AudioServerPlugInDriverRef driver, AudioObjectID object_id,
    pid_t client_process_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data, UInt32 data_size,
    UInt32 *out_data_size, void *out_data) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().GetPropertyData(object_id, address, qualifier_data_size,
                                       qualifier_data, data_size, out_data_size,
                                       out_data);
}

OSStatus ProxyAudioDriver::StaticSetPropertyData(
    AudioServerPlugInDriverRef driver, AudioObjectID object_id,
    pid_t client_process_id, const AudioObjectPropertyAddress *address,
    UInt32 qualifier_data_size, const void *qualifier_data, UInt32 data_size,
    const void *data) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  UInt32 num_changed = 0;
  AudioObjectPropertyAddress changed_addresses[2];
  return GetInstance().SetPropertyData(object_id, address, qualifier_data_size,
                                       qualifier_data, data_size, data,
                                       &num_changed, changed_addresses);
}

OSStatus ProxyAudioDriver::StaticStartIO(AudioServerPlugInDriverRef driver,
                                         AudioObjectID device_object_id,
                                         UInt32 client_id) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().StartIO(device_object_id, client_id);
}

OSStatus ProxyAudioDriver::StaticStopIO(AudioServerPlugInDriverRef driver,
                                        AudioObjectID device_object_id,
                                        UInt32 client_id) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().StopIO(device_object_id, client_id);
}

OSStatus ProxyAudioDriver::StaticGetZeroTimeStamp(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt32 client_id, Float64 *out_sample_time, UInt64 *out_host_time,
    UInt64 *out_seed) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().GetZeroTimeStamp(
      device_object_id, client_id, out_sample_time, out_host_time, out_seed);
}

OSStatus ProxyAudioDriver::StaticWillDoIOOperation(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt32 client_id, UInt32 operation_id, Boolean *out_will_do,
    Boolean *out_will_do_in_place) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().WillDoIOOperation(device_object_id, client_id,
                                         operation_id, out_will_do,
                                         out_will_do_in_place);
}

OSStatus ProxyAudioDriver::StaticBeginIOOperation(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt32 client_id, UInt32 operation_id, UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().BeginIOOperation(device_object_id, client_id,
                                        operation_id, io_buffer_frame_size,
                                        io_cycle_info);
}

OSStatus ProxyAudioDriver::StaticDoIOOperation(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    AudioObjectID stream_object_id, UInt32 client_id, UInt32 operation_id,
    UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info, void *io_main_buffer,
    void *io_secondary_buffer) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().DoIOOperation(
      device_object_id, stream_object_id, client_id, operation_id,
      io_buffer_frame_size, io_cycle_info, io_main_buffer, io_secondary_buffer);
}

OSStatus ProxyAudioDriver::StaticEndIOOperation(
    AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
    UInt32 client_id, UInt32 operation_id, UInt32 io_buffer_frame_size,
    const AudioServerPlugInIOCycleInfo *io_cycle_info) {
  if (!IsValidDriverRef(driver))
    return kAudioHardwareBadObjectError;
  return GetInstance().EndIOOperation(device_object_id, client_id, operation_id,
                                      io_buffer_frame_size, io_cycle_info);
}

// Global driver interface structure - points directly to static methods
static AudioServerPlugInDriverInterface gDriverInterface = {
    nullptr, // _reserved
    ProxyAudioDriver::QueryInterface,
    ProxyAudioDriver::AddRef,
    ProxyAudioDriver::Release,
    ProxyAudioDriver::StaticInitialize,
    ProxyAudioDriver::StaticCreateDevice,
    ProxyAudioDriver::StaticDestroyDevice,
    ProxyAudioDriver::StaticAddDeviceClient,
    ProxyAudioDriver::StaticRemoveDeviceClient,
    ProxyAudioDriver::StaticPerformDeviceConfigurationChange,
    ProxyAudioDriver::StaticAbortDeviceConfigurationChange,
    ProxyAudioDriver::StaticHasProperty,
    ProxyAudioDriver::StaticIsPropertySettable,
    ProxyAudioDriver::StaticGetPropertyDataSize,
    ProxyAudioDriver::StaticGetPropertyData,
    ProxyAudioDriver::StaticSetPropertyData,
    ProxyAudioDriver::StaticStartIO,
    ProxyAudioDriver::StaticStopIO,
    ProxyAudioDriver::StaticGetZeroTimeStamp,
    ProxyAudioDriver::StaticWillDoIOOperation,
    ProxyAudioDriver::StaticBeginIOOperation,
    ProxyAudioDriver::StaticDoIOOperation,
    ProxyAudioDriver::StaticEndIOOperation};

static AudioServerPlugInDriverInterface *gDriverInterfacePtr =
    &gDriverInterface;
static AudioServerPlugInDriverRef gDriverRef = &gDriverInterfacePtr;

AudioServerPlugInDriverInterface *ProxyAudioDriver::GetDriverInterface() {
  return &gDriverInterface;
}

AudioServerPlugInDriverRef ProxyAudioDriver::GetDriverRef() {
  return gDriverRef;
}

bool ProxyAudioDriver::IsValidDriverRef(AudioServerPlugInDriverRef driver) {
  return driver == gDriverRef;
}

} // namespace ProxyAudio

//==================================================================================================
// C Factory Function
//==================================================================================================

extern "C" {
void *ProxyAudio_Create(CFAllocatorRef allocator,
                        CFUUIDRef requested_type_uuid) {
  if (CFEqual(requested_type_uuid, kAudioServerPlugInTypeUUID)) {
    return ProxyAudio::ProxyAudioDriver::GetDriverRef();
  }
  return nullptr;
}
}
