// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "ProxyDevice.hpp"

#include <CoreAudio/AudioHardwareBase.h>
#include <MacTypes.h>

#include <cstdint>

#include "AudioObjectUtils.hpp"
#include "Error.hpp"

namespace ProxyAudio {

aspl::DeviceParameters ProxyDevice::GetTargetDeviceParameters(
    std::shared_ptr<const aspl::Context> context,
    const AudioObjectID targetDeviceID) {
  context->Tracer->Message("[PROXY DEVICE] Getting target device parameters");

  try {
    aspl::DeviceParameters parameters{
        .Name = ProxyAudio::GetPropertyData<std::string>(
                    targetDeviceID,
                    {
                        .mSelector = kAudioObjectPropertyName,
                        .mScope = kAudioObjectPropertyScopeGlobal,
                        .mElement = kAudioObjectPropertyElementMain,
                    },
                    {}) +
                " (Proxy)",
        .DeviceUID = ProxyAudio::GetPropertyData<std::string>(
                         targetDeviceID,
                         {
                             .mSelector = kAudioDevicePropertyDeviceUID,
                             .mScope = kAudioObjectPropertyScopeGlobal,
                             .mElement = kAudioObjectPropertyElementMain,
                         },
                         {}) +
                     "_proxy",
        .ModelUID = ProxyAudio::GetPropertyData<std::string>(
                        targetDeviceID,
                        {
                            .mSelector = kAudioDevicePropertyModelUID,
                            .mScope = kAudioObjectPropertyScopeGlobal,
                            .mElement = kAudioObjectPropertyElementMain,
                        },
                        {}) +
                    "_proxy",
        .CanBeDefault =
            ProxyAudio::GetPropertyData<uint32_t>(
                targetDeviceID,
                {
                    .mSelector = kAudioDevicePropertyDeviceCanBeDefaultDevice,
                    .mScope = kAudioObjectPropertyScopeOutput,
                    .mElement = kAudioObjectPropertyElementMain,
                },
                {}) != 0,
        .CanBeDefaultForSystemSounds =
            ProxyAudio::GetPropertyData<uint32_t>(
                targetDeviceID,
                {
                    .mSelector =
                        kAudioDevicePropertyDeviceCanBeDefaultSystemDevice,
                    .mScope = kAudioObjectPropertyScopeOutput,
                    .mElement = kAudioObjectPropertyElementMain,
                },
                {}) != 0,
        .SampleRate =
            static_cast<uint32_t>(ProxyAudio::GetPropertyData<Float64>(
                targetDeviceID,
                {
                    .mSelector = kAudioDevicePropertyNominalSampleRate,
                    .mScope = kAudioObjectPropertyScopeGlobal,
                    .mElement = kAudioObjectPropertyElementMain,
                },
                {})),
    };

    context->Tracer->Message("[PROXY DEVICE] Target device parameters: %s",
                             parameters.Name.c_str());
    context->Tracer->Message("[PROXY DEVICE] Target device parameters: %s",
                             parameters.DeviceUID.c_str());
    context->Tracer->Message("[PROXY DEVICE] Target device parameters: %s",
                             parameters.ModelUID.c_str());
    context->Tracer->Message("[PROXY DEVICE] Target device parameters: %s",
                             parameters.CanBeDefault ? "Yes" : "No");
    context->Tracer->Message(
        "[PROXY DEVICE] Target device parameters: %s",
        parameters.CanBeDefaultForSystemSounds ? "Yes" : "No");
    context->Tracer->Message("[PROXY DEVICE] Target device parameters: %u",
                             parameters.SampleRate);

    return parameters;
  } catch (const OSStatusError& e) {
    context->Tracer->Message(
        "[PROXY DEVICE] Failed to get target device parameters: %s", e.what());

    return {
        .Name = "Unknown Device",
    };
  }
}

ProxyDevice::ProxyDevice(std::shared_ptr<const aspl::Context> context,
                         const AudioObjectID targetDeviceID)
    : aspl::Device(context, GetTargetDeviceParameters(context, targetDeviceID)),
      targetDeviceID_(targetDeviceID) {}

}  // namespace ProxyAudio
