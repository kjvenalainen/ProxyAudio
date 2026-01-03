// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "CommonProperties.hpp"

#include "AudioObjectUtils.hpp"

namespace ProxyAudio {

std::string GetDeviceNameProperty(AudioObjectID deviceID) {
  return GetPropertyData<std::string>(
      deviceID,
      {
          .mSelector = kAudioObjectPropertyName,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

std::string GetDeviceUIDProperty(AudioObjectID deviceID) {
  return GetPropertyData<std::string>(
      deviceID,
      {
          .mSelector = kAudioDevicePropertyDeviceUID,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

std::string GetDeviceModelUIDProperty(AudioObjectID deviceID) {
  return GetPropertyData<std::string>(
      deviceID,
      {
          .mSelector = kAudioDevicePropertyModelUID,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

bool GetDeviceCanBeDefaultProperty(AudioObjectID deviceID) {
  return GetPropertyData<uint32_t>(
             deviceID,
             {
                 .mSelector = kAudioDevicePropertyDeviceCanBeDefaultDevice,
                 .mScope = kAudioObjectPropertyScopeOutput,
                 .mElement = kAudioObjectPropertyElementMain,
             },
             {}) != 0;
}

bool GetDeviceCanBeDefaultForSystemSoundsProperty(AudioObjectID deviceID) {
  return GetPropertyData<uint32_t>(
             deviceID,
             {
                 .mSelector =
                     kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
                 .mScope = kAudioObjectPropertyScopeOutput,
                 .mElement = kAudioObjectPropertyElementMain,
             },
             {}) != 0;
}

uint32_t GetDeviceSampleRateProperty(AudioObjectID deviceID) {
  return static_cast<uint32_t>(GetPropertyData<Float64>(
      deviceID,
      {
          .mSelector = kAudioDevicePropertyNominalSampleRate,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {}));
}

}  // namespace ProxyAudio
