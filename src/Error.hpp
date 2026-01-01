// Copyright (c) 2026 Tap Turtle
// See LICENSE for license details.

#pragma once

#include <CoreAudio/AudioHardware.h>

#include <stdexcept>
#include <string>

namespace ProxyAudio {

class OSStatusError : public std::runtime_error {
 public:
  static constexpr const char* GetMessage(OSStatus status) noexcept {
    switch (status) {
      case kAudioHardwareNoError:
        return "No error";
      case kAudioHardwareNotRunningError:
        return "Audio hardware is not running";
      case kAudioHardwareUnspecifiedError:
        return "Unspecified error";
      case kAudioHardwareUnknownPropertyError:
        return "Unknown property";
      case kAudioHardwareBadPropertySizeError:
        return "Bad property size";
      case kAudioHardwareIllegalOperationError:
        return "Illegal operation";
      case kAudioHardwareBadObjectError:
        return "Bad object";
      case kAudioHardwareBadDeviceError:
        return "Bad device";
      case kAudioHardwareBadStreamError:
        return "Bad stream";
      case kAudioHardwareUnsupportedOperationError:
        return "Unsupported operation";
      case kAudioHardwareNotReadyError:
        return "Not ready";
      case kAudioDeviceUnsupportedFormatError:
        return "Unsupported format";
      case kAudioDevicePermissionsError:
        return "Permissions error";
      default:
        return "Unknown error";
    }
  }

  OSStatusError(OSStatus status)
      : std::runtime_error(GetMessage(status)), status(status) {}

  OSStatus GetStatus() const noexcept { return status; }

 private:
  OSStatus status;
};

}  // namespace ProxyAudio
