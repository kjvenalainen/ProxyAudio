// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <cstdint>
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

}  // namespace ProxyAudio
