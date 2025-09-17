/*
See the LICENSE.txt file for this sample's licensing information.

Abstract:
Common types, constants, and utilities for ProxyAudio driver.
*/

#pragma once

#include <CoreAudio/AudioServerPlugIn.h>
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace ProxyAudio {

//==================================================================================================
// Type Traits for Audio Objects (C++20 concepts replacement for older
// compilers)
//==================================================================================================

template <typename T> struct is_audio_object_type {
  template <typename U>
  static auto test(int)
      -> decltype(std::declval<U>().GetObjectID(),
                  std::declval<U>().GetClassID(), std::true_type{});
  template <typename> static std::false_type test(...);

  static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> struct is_property_handler {
  template <typename U>
  static auto test(int)
      -> decltype(std::declval<U>().HasProperty(
                      std::declval<const AudioObjectPropertyAddress &>()),
                  std::declval<U>().IsPropertySettable(
                      std::declval<const AudioObjectPropertyAddress &>()),
                  std::declval<U>().GetPropertyDataSize(
                      std::declval<const AudioObjectPropertyAddress &>()),
                  std::true_type{});
  template <typename> static std::false_type test(...);

  static constexpr bool value = decltype(test<T>(0))::value;
};

//==================================================================================================
// Constants
//==================================================================================================

namespace Constants {
constexpr std::string_view kBundleID = "com.tapturtle.ProxyAudio";
constexpr std::string_view kBoxUID = "ProxyAudioBox_UID";
constexpr std::string_view kDeviceUID = "ProxyAudioDevice_UID";
constexpr std::string_view kDeviceModelUID = "ProxyAudioDevice_ModelUID";

constexpr AudioObjectPropertySelector kCustomPropertyID = 'PCst';
constexpr UInt32 kRingBufferSize = 16384;
constexpr Float32 kVolumeMinDB = -96.0f;
constexpr Float32 kVolumeMaxDB = 6.0f;
constexpr UInt32 kDataSourceItems = 4;
constexpr std::string_view kDataSourceNamePattern = "ProxyAudio Source {}";

enum ObjectIDs : AudioObjectID {
  PlugIn = kAudioObjectPlugInObject,
  Box = 2,
  Device = 3,
  StreamInput = 4,
  VolumeInputMaster = 5,
  MuteInputMaster = 6,
  DataSourceInputMaster = 7,
  StreamOutput = 8,
  VolumeOutputMaster = 9,
  MuteOutputMaster = 10,
  DataSourceOutputMaster = 11,
  DataDestinationPlayThruMaster = 12
};
} // namespace Constants

} // namespace ProxyAudio
