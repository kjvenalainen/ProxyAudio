// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <memory>
#include <string>

namespace ProxyAudio {

std::string GetDeviceNameProperty(AudioObjectID deviceID);

std::string GetDeviceUIDProperty(AudioObjectID deviceID);

std::string GetDeviceModelUIDProperty(AudioObjectID deviceID);

bool GetDeviceCanBeDefaultProperty(AudioObjectID deviceID);

bool GetDeviceCanBeDefaultForSystemSoundsProperty(AudioObjectID deviceID);

uint32_t GetDeviceSampleRateProperty(AudioObjectID deviceID);

}  // namespace ProxyAudio
