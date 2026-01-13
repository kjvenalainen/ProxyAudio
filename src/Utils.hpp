// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <cstdint>
#include <functional>
#include <sstream>
#include <vector>

#include "AudioObjectUtils.hpp"
#include "CFUtils.hpp"
#include "aspl/Context.hpp"

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
    context->Tracer->Message(
        "EnumerateAudioOutputDevices:Failed to get devices: %s", e.what());

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

// Given a device, walk the inheritance tree by repeatedly getting
// kAudioObjectPropertyOwnedObjects. For each object get some basic info
// about it and print the whole tree to a string.
std::string DumpDeviceTree(AudioObjectID deviceId) {
  std::stringstream ss;

  // Helper function to recursively dump object tree
  std::function<void(AudioObjectID, int)> dumpObject;
  dumpObject = [&](AudioObjectID objectId, int depth) {
    std::string indent(depth * 2, ' ');

    // Get object class
    std::string objectClass = "unknown";
    try {
      auto classId = GetPropertyData<AudioClassID>(
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
       << ", Name: " << objectName << ", Owned objects: " << ownedObjectsCount
       << "\n";

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
