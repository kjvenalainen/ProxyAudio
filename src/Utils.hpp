// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>
#include <MacTypes.h>

#include <cstdint>
#include <functional>
#include <sstream>
#include <vector>

#include "AudioObjectUtils.hpp"
#include "CFUtils.hpp"
#include "CommonProperties.hpp"
#include "Tracer.hpp"
#include "aspl/Context.hpp"
#include "aspl/Direction.hpp"

namespace ProxyAudio {

std::vector<AudioObjectID> EnumerateAudioOutputDevices(
    std::shared_ptr<aspl::Context> context);

template <typename T>
T DbToRawScalar(T db) {
  return std::pow(T(10.0), db / T(20.0));
}

// Returns the AudioObjectId for a control matching the class and scope.
AudioObjectID GetControlId(AudioClassID classId,
                           AudioObjectPropertyScope scope,
                           const std::vector<AudioObjectID>& controls);

void DumpStreamInfo(std::stringstream& ss, AudioObjectID stream);

void DumpControlInfo(std::stringstream& ss, AudioObjectID control);

// Given a device, walk the inheritance tree by repeatedly getting
// kAudioObjectPropertyOwnedObjects. For each object get some basic info
// about it and print the whole tree to a string.
std::string DumpDeviceTree(AudioObjectID deviceId);

}  // namespace ProxyAudio
