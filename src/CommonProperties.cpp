// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "CommonProperties.hpp"

#include <CoreAudio/AudioHardwareBase.h>

#include "AudioObjectUtils.hpp"
#include "aspl/Direction.hpp"

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
  return GetPropertyData<UInt32>(
             deviceID,
             {
                 .mSelector = kAudioDevicePropertyDeviceCanBeDefaultDevice,
                 .mScope = kAudioObjectPropertyScopeOutput,
                 .mElement = kAudioObjectPropertyElementMain,
             },
             {}) != 0;
}

bool GetDeviceCanBeDefaultForSystemSoundsProperty(AudioObjectID deviceID) {
  return GetPropertyData<UInt32>(
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

aspl::Direction GetDirectionProperty(AudioObjectID streamID) {
  return GetPropertyData<UInt32>(
             streamID,
             {
                 .mSelector = kAudioStreamPropertyDirection,
                 .mScope = kAudioObjectPropertyScopeGlobal,
                 .mElement = kAudioObjectPropertyElementMain,
             },
             {}) == 0
             ? aspl::Direction::Output
             : aspl::Direction::Input;
}

uint32_t GetStartingChannelProperty(AudioObjectID streamID) {
  return GetPropertyData<UInt32>(
      streamID,
      {
          .mSelector = kAudioStreamPropertyStartingChannel,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

AudioStreamBasicDescription GetFormatProperty(AudioObjectID streamID) {
  return GetPropertyData<AudioStreamBasicDescription>(
      streamID,
      {
          .mSelector = kAudioStreamPropertyVirtualFormat,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

uint32_t GetLatencyProperty(AudioObjectID streamID) {
  return GetPropertyData<UInt32>(
      streamID,
      {
          .mSelector = kAudioStreamPropertyLatency,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

AudioObjectPropertyScope GetScopeProperty(AudioObjectID volumeControlID) {
  return GetPropertyData<AudioObjectPropertyScope>(
      volumeControlID,
      {
          .mSelector = kAudioControlPropertyScope,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

AudioObjectPropertyElement GetElementProperty(AudioObjectID volumeControlID) {
  return GetPropertyData<AudioObjectPropertyElement>(
      volumeControlID,
      {
          .mSelector = kAudioControlPropertyElement,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

AudioValueRange GetDecibelRangeProperty(AudioObjectID volumeControlID) {
  return GetPropertyData<AudioValueRange>(
      volumeControlID,
      {
          .mSelector = kAudioLevelControlPropertyDecibelRange,
          .mScope = kAudioObjectPropertyScopeGlobal,
          .mElement = kAudioObjectPropertyElementMain,
      },
      {});
}

}  // namespace ProxyAudio
