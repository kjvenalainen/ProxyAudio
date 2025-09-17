/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Main ProxyAudio driver class with static C interface methods.
*/

#pragma once

#include "AudioObject.hpp"
#include "Common.hpp"

// Forward declarations for all audio object types
namespace ProxyAudio {
class AudioObject;
class PlugInObject;
class BoxObject;
class DeviceObject;
class StreamObject;
class VolumeControl;
class MuteControl;
class DataSourceControl;

//==================================================================================================
// ProxyAudioDriver
//==================================================================================================

class ProxyAudioDriver {
public:
  ProxyAudioDriver();
  ~ProxyAudioDriver();

  // Driver lifecycle
  OSStatus Initialize(AudioServerPlugInHostRef host);
  OSStatus Teardown();

  // Device management
  OSStatus CreateDevice(CFDictionaryRef description,
                        const AudioServerPlugInClientInfo *client_info,
                        AudioObjectID *out_device_object_id);
  OSStatus DestroyDevice(AudioObjectID device_object_id);
  OSStatus AddDeviceClient(AudioObjectID device_object_id,
                           const AudioServerPlugInClientInfo *client_info);
  OSStatus RemoveDeviceClient(AudioObjectID device_object_id,
                              const AudioServerPlugInClientInfo *client_info);

  // Configuration changes
  OSStatus PerformDeviceConfigurationChange(AudioObjectID device_object_id,
                                            UInt64 change_action,
                                            void *change_info);
  OSStatus AbortDeviceConfigurationChange(AudioObjectID device_object_id,
                                          UInt64 change_action,
                                          void *change_info);

  // Property operations
  OSStatus HasProperty(AudioObjectID object_id,
                       const AudioObjectPropertyAddress *address,
                       Boolean *out_has_property);
  OSStatus IsPropertySettable(AudioObjectID object_id,
                              const AudioObjectPropertyAddress *address,
                              Boolean *out_is_settable);
  OSStatus GetPropertyDataSize(AudioObjectID object_id,
                               const AudioObjectPropertyAddress *address,
                               UInt32 qualifier_data_size,
                               const void *qualifier_data,
                               UInt32 *out_data_size);
  OSStatus GetPropertyData(AudioObjectID object_id,
                           const AudioObjectPropertyAddress *address,
                           UInt32 qualifier_data_size,
                           const void *qualifier_data, UInt32 data_size,
                           UInt32 *out_data_size, void *out_data);
  OSStatus SetPropertyData(AudioObjectID object_id,
                           const AudioObjectPropertyAddress *address,
                           UInt32 qualifier_data_size,
                           const void *qualifier_data, UInt32 data_size,
                           const void *data,
                           UInt32 *out_number_properties_changed,
                           AudioObjectPropertyAddress *out_changed_addresses);

  // IO operations
  OSStatus StartIO(AudioObjectID device_object_id, UInt32 client_id);
  OSStatus StopIO(AudioObjectID device_object_id, UInt32 client_id);
  OSStatus GetZeroTimeStamp(AudioObjectID device_object_id, UInt32 client_id,
                            Float64 *out_sample_time, UInt64 *out_host_time,
                            UInt64 *out_seed);
  OSStatus WillDoIOOperation(AudioObjectID device_object_id, UInt32 client_id,
                             UInt32 operation_id, Boolean *out_will_do,
                             Boolean *out_will_do_in_place);
  OSStatus BeginIOOperation(AudioObjectID device_object_id, UInt32 client_id,
                            UInt32 operation_id, UInt32 io_buffer_frame_size,
                            const AudioServerPlugInIOCycleInfo *io_cycle_info);
  OSStatus DoIOOperation(AudioObjectID device_object_id,
                         AudioObjectID stream_object_id, UInt32 client_id,
                         UInt32 operation_id, UInt32 io_buffer_frame_size,
                         const AudioServerPlugInIOCycleInfo *io_cycle_info,
                         void *io_main_buffer, void *io_secondary_buffer);
  OSStatus EndIOOperation(AudioObjectID device_object_id, UInt32 client_id,
                          UInt32 operation_id, UInt32 io_buffer_frame_size,
                          const AudioServerPlugInIOCycleInfo *io_cycle_info);

  // Static C interface methods that delegate to singleton
  static HRESULT QueryInterface(void *driver, REFIID uuid, LPVOID *interface);
  static ULONG AddRef(void *driver);
  static ULONG Release(void *driver);
  static OSStatus StaticInitialize(AudioServerPlugInDriverRef driver,
                                   AudioServerPlugInHostRef host);
  static OSStatus
  StaticCreateDevice(AudioServerPlugInDriverRef driver,
                     CFDictionaryRef description,
                     const AudioServerPlugInClientInfo *client_info,
                     AudioObjectID *out_device_object_id);
  static OSStatus StaticDestroyDevice(AudioServerPlugInDriverRef driver,
                                      AudioObjectID device_object_id);
  static OSStatus
  StaticAddDeviceClient(AudioServerPlugInDriverRef driver,
                        AudioObjectID device_object_id,
                        const AudioServerPlugInClientInfo *client_info);
  static OSStatus
  StaticRemoveDeviceClient(AudioServerPlugInDriverRef driver,
                           AudioObjectID device_object_id,
                           const AudioServerPlugInClientInfo *client_info);
  static OSStatus StaticPerformDeviceConfigurationChange(
      AudioServerPlugInDriverRef driver, AudioObjectID device_object_id,
      UInt64 change_action, void *change_info);
  static OSStatus
  StaticAbortDeviceConfigurationChange(AudioServerPlugInDriverRef driver,
                                       AudioObjectID device_object_id,
                                       UInt64 change_action, void *change_info);
  static Boolean StaticHasProperty(AudioServerPlugInDriverRef driver,
                                   AudioObjectID object_id,
                                   pid_t client_process_id,
                                   const AudioObjectPropertyAddress *address);
  static OSStatus
  StaticIsPropertySettable(AudioServerPlugInDriverRef driver,
                           AudioObjectID object_id, pid_t client_process_id,
                           const AudioObjectPropertyAddress *address,
                           Boolean *out_is_settable);
  static OSStatus
  StaticGetPropertyDataSize(AudioServerPlugInDriverRef driver,
                            AudioObjectID object_id, pid_t client_process_id,
                            const AudioObjectPropertyAddress *address,
                            UInt32 qualifier_data_size,
                            const void *qualifier_data, UInt32 *out_data_size);
  static OSStatus StaticGetPropertyData(
      AudioServerPlugInDriverRef driver, AudioObjectID object_id,
      pid_t client_process_id, const AudioObjectPropertyAddress *address,
      UInt32 qualifier_data_size, const void *qualifier_data, UInt32 data_size,
      UInt32 *out_data_size, void *out_data);
  static OSStatus
  StaticSetPropertyData(AudioServerPlugInDriverRef driver,
                        AudioObjectID object_id, pid_t client_process_id,
                        const AudioObjectPropertyAddress *address,
                        UInt32 qualifier_data_size, const void *qualifier_data,
                        UInt32 data_size, const void *data);
  static OSStatus StaticStartIO(AudioServerPlugInDriverRef driver,
                                AudioObjectID device_object_id,
                                UInt32 client_id);
  static OSStatus StaticStopIO(AudioServerPlugInDriverRef driver,
                               AudioObjectID device_object_id,
                               UInt32 client_id);
  static OSStatus StaticGetZeroTimeStamp(AudioServerPlugInDriverRef driver,
                                         AudioObjectID device_object_id,
                                         UInt32 client_id,
                                         Float64 *out_sample_time,
                                         UInt64 *out_host_time,
                                         UInt64 *out_seed);
  static OSStatus StaticWillDoIOOperation(AudioServerPlugInDriverRef driver,
                                          AudioObjectID device_object_id,
                                          UInt32 client_id, UInt32 operation_id,
                                          Boolean *out_will_do,
                                          Boolean *out_will_do_in_place);
  static OSStatus
  StaticBeginIOOperation(AudioServerPlugInDriverRef driver,
                         AudioObjectID device_object_id, UInt32 client_id,
                         UInt32 operation_id, UInt32 io_buffer_frame_size,
                         const AudioServerPlugInIOCycleInfo *io_cycle_info);
  static OSStatus
  StaticDoIOOperation(AudioServerPlugInDriverRef driver,
                      AudioObjectID device_object_id,
                      AudioObjectID stream_object_id, UInt32 client_id,
                      UInt32 operation_id, UInt32 io_buffer_frame_size,
                      const AudioServerPlugInIOCycleInfo *io_cycle_info,
                      void *io_main_buffer, void *io_secondary_buffer);
  static OSStatus
  StaticEndIOOperation(AudioServerPlugInDriverRef driver,
                       AudioObjectID device_object_id, UInt32 client_id,
                       UInt32 operation_id, UInt32 io_buffer_frame_size,
                       const AudioServerPlugInIOCycleInfo *io_cycle_info);

  // Get the driver interface structure
  static AudioServerPlugInDriverInterface *GetDriverInterface();
  static AudioServerPlugInDriverRef GetDriverRef();

  // Singleton access
  static ProxyAudioDriver &GetInstance() {
    static ProxyAudioDriver instance;
    return instance;
  }

private:
  // Object management
  AudioObject *GetAudioObject(AudioObjectID object_id);
  const AudioObject *GetAudioObject(AudioObjectID object_id) const;

  // State management
  mutable std::shared_mutex objects_mutex_;
  std::unordered_map<AudioObjectID, std::unique_ptr<AudioObject>> objects_;

  // Driver state
  std::atomic<UInt32> ref_count_{0};
  AudioServerPlugInHostRef host_{nullptr};

  // Thread safety
  mutable std::mutex state_mutex_;

  // Initialization
  void CreateAllObjects();
  void DestroyAllObjects();

  // Validation helper
  static bool IsValidDriverRef(AudioServerPlugInDriverRef driver);
};

} // namespace ProxyAudio
