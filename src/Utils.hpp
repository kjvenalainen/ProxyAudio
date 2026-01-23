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
    std::shared_ptr<aspl::Context> context) {
  try {
    auto devices = ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
        kAudioObjectSystemObject,
        {
            .mSelector = kAudioHardwarePropertyDevices,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    return devices;
  } catch (const OSStatusError& e) {
    ProxyAudio::Tracer::FromTracer(context->Tracer)
        ->Message(ProxyAudio::Tracer::Info,
                  "EnumerateAudioOutputDevices:Failed to get devices: %s",
                  e.what());

    return {};
  }
}

std::string ToString(const AudioStreamBasicDescription& format) {
  std::stringstream ss;

  ss << "{id: " << FourCC(format.mFormatID) << ", ";
  ss << "flags: 0x" << std::hex << format.mFormatFlags << std::dec << ", ";
  ss << "rate: " << format.mSampleRate << ", ";
  ss << "bits: " << format.mBitsPerChannel << ", ";
  ss << "chans: " << format.mChannelsPerFrame << ", ";
  ss << "frames/pkt: " << format.mFramesPerPacket << ", ";
  ss << "bytes/frame: " << format.mBytesPerFrame << ", ";
  ss << "bytes/pkt: " << format.mBytesPerPacket << "}";

  return ss.str();
}

template <typename T>
T DbToRawScalar(T db) {
  return std::pow(T(10.0), db / T(20.0));
}

void DumpStreamInfo(std::stringstream& ss, AudioObjectID stream) {
  try {
    const auto classId = GetPropertyData<AudioClassID>(
        stream,
        {
            .mSelector = kAudioObjectPropertyClass,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    if (classId != kAudioStreamClassID) {
      ss << "Not a stream!";
      return;
    }

    const auto direction = GetDirectionProperty(stream);
    const auto startingChannel = GetStartingChannelProperty(stream);
    ss << "Direction: "
       << (direction == aspl::Direction::Input ? "Input" : "Output")
       << ", StartingChannel: " << startingChannel;

  } catch (...) {
  }
}

void DumpVolumeControlInfo(std::stringstream& ss, AudioObjectID volume) {
  try {
    const auto classId = GetPropertyData<AudioClassID>(
        volume,
        {
            .mSelector = kAudioObjectPropertyClass,
            .mScope = kAudioObjectPropertyScopeGlobal,
            .mElement = kAudioObjectPropertyElementMain,
        },
        {});

    if (classId != kAudioVolumeControlClassID) {
      ss << "Not a volume control!";
      return;
    }

    const auto scope = GetScopeProperty(volume);
    const auto element = GetElementProperty(volume);
    ss << "Scope: ";
    switch (scope) {
      case kAudioObjectPropertyScopeInput:
        ss << "Input";
        break;
      case kAudioObjectPropertyScopeOutput:
        ss << "Output";
        break;
      case kAudioObjectPropertyScopeGlobal:
        ss << "Global";
        break;
    }

    ss << ", Element: ";
    switch (element) {
      case kAudioObjectPropertyElementMain:
        ss << "Main";
        break;
      default:
        ss << FourCC(element) << " (" << std::to_string(element) << ")";
        break;
    }

  } catch (...) {
  }
}

// Given a device, walk the inheritance tree by repeatedly getting
// kAudioObjectPropertyOwnedObjects. For each object get some basic info
// about it and print the whole tree to a string.
std::string DumpDeviceTree(AudioObjectID deviceId) {
  std::stringstream ss;

  // Helper function to recursively dump object tree
  std::function<void(AudioObjectID, int)> dumpObject;
  dumpObject = [&](AudioObjectID objectId, int depth) {
    std::string indent(depth * 2, ' ');
    for (int i = 0; i < depth * 2; i += 2) {
      indent[i] = '|';
    }

    // Get object class
    std::string objectClass = "unknown";
    unsigned int classId = kInvalidID;
    try {
      classId = GetPropertyData<AudioClassID>(
          objectId,
          {
              .mSelector = kAudioObjectPropertyClass,
              .mScope = kAudioObjectPropertyScopeGlobal,
              .mElement = kAudioObjectPropertyElementMain,
          },
          {});
      objectClass = FourCC(classId);
    } catch (...) {
      // Ignore errors
    }

    // Get object name
    std::string objectName = "unnamed";
    try {
      objectName = GetPropertyData<std::string>(
          objectId,
          {
              .mSelector = kAudioObjectPropertyName,
              .mScope = kAudioObjectPropertyScopeGlobal,
              .mElement = kAudioObjectPropertyElementMain,
          },
          {});
    } catch (...) {
      // Ignore errors - not all objects have names
    }

    unsigned int ownedObjectsCount =
        GetPropertyDataSize(objectId,
                            {
                                .mSelector = kAudioObjectPropertyOwnedObjects,
                                .mScope = kAudioObjectPropertyScopeGlobal,
                                .mElement = kAudioObjectPropertyElementMain,
                            },
                            {}) /
        sizeof(AudioObjectID);

    ss << indent << "Object ID: " << objectId << ", Class: " << objectClass
       << ", Name: " << objectName << ", Children: " << ownedObjectsCount;

    switch (classId) {
      case kAudioStreamClassID:
        ss << " [";
        DumpStreamInfo(ss, objectId);
        ss << "]";
        break;
      case kAudioVolumeControlClassID:
        ss << " [";
        DumpVolumeControlInfo(ss, objectId);
        ss << "]";
        break;
    }

    ss << "\n";

    // Get owned objects
    try {
      auto ownedObjects = GetPropertyData<std::vector<AudioObjectID>>(
          objectId,
          {
              .mSelector = kAudioObjectPropertyOwnedObjects,
              .mScope = kAudioObjectPropertyScopeGlobal,
              .mElement = kAudioObjectPropertyElementMain,
          },
          {});

      // Recursively dump owned objects
      for (auto ownedId : ownedObjects) {
        dumpObject(ownedId, depth + 1);
      }
    } catch (...) {
      // Ignore errors - not all objects have owned objects
    }
  };

  // Start dumping from the device
  dumpObject(deviceId, 0);

  return ss.str();
}

}  // namespace ProxyAudio
