// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#include "Utils.hpp"

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

AudioObjectID GetControlId(AudioClassID classId,
                           AudioObjectPropertyScope scope,
                           const std::vector<AudioObjectID>& controls) {
  for (auto& controlId : controls) {
    const auto objectClassId = GetClassIdProperty(controlId);
    const auto objectScope = GetScopeProperty(controlId);

    if (objectClassId == classId && objectScope == scope) {
      return controlId;
    }
  }

  return kAudioObjectUnknown;
}

void DumpStreamInfo(std::stringstream& ss, AudioObjectID stream) {
  try {
    const auto classId = GetClassIdProperty(stream);

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

void DumpControlInfo(std::stringstream& ss, AudioObjectID control) {
  try {
    const auto scope = GetScopeProperty(control);
    const auto element = GetElementProperty(control);
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
      classId = GetClassIdProperty(objectId);
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
      case kAudioMuteControlClassID:
      case kAudioStereoPanControlClassID:
      case kAudioDataSourceControlClassID:
      case kAudioDataDestinationControlClassID:
        ss << " [";
        DumpControlInfo(ss, objectId);
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
