// SPDX-License-Identifier: MIT
//
// Copyright (c) 2025 Tapturtle
//
// See the LICENSE.txt file for licensing information.

#pragma once

#include <CoreAudio/AudioHardwareBase.h>

namespace ProxyAudio {

// Convertible to `kAudioStreamPropertyDirection`.
//
// - `kAudioStreamPropertyDirection`
//     A UInt32 where a value of 0 means that this AudioStream is an output
//     stream and a value of 1 means that it is an input stream.
enum class Direction : unsigned int {
  Output = 0,
  Input = 1,
};

// Corresponds to the various terminal types defined in
// <CoreAudio/AudioHardwareBase.h>. Such as `kAudioStreamTerminalTypeSpeaker` or
// `kAudioStreamTerminalTypeMicrophone`.
enum class TerminalType : unsigned int {
  Unknown = kAudioStreamTerminalTypeUnknown,
  Line = kAudioStreamTerminalTypeLine,
  DigitalAudioInterface = kAudioStreamTerminalTypeDigitalAudioInterface,
  Speaker = kAudioStreamTerminalTypeSpeaker,
  Headphones = kAudioStreamTerminalTypeHeadphones,
  LFESpeaker = kAudioStreamTerminalTypeLFESpeaker,
  ReceiverSpeaker = kAudioStreamTerminalTypeReceiverSpeaker,
  Microphone = kAudioStreamTerminalTypeMicrophone,
  HeadsetMicrophone = kAudioStreamTerminalTypeHeadsetMicrophone,
  ReceiverMicrophone = kAudioStreamTerminalTypeReceiverMicrophone,
  TTY = kAudioStreamTerminalTypeTTY,
  HDMI = kAudioStreamTerminalTypeHDMI,
  DisplayPort = kAudioStreamTerminalTypeDisplayPort,
};

// Corresponds to the various transport types defined in
// <CoreAudio/AudioHardwareBase.h>. Such as `kAudioDeviceTransportTypeBuiltIn`
// or `kAudioDeviceTransportTypeVirtual`.
enum class TransportType : unsigned int {
  Unknown = kAudioDeviceTransportTypeUnknown,
  BuiltIn = kAudioDeviceTransportTypeBuiltIn,
  Aggregate = kAudioDeviceTransportTypeAggregate,
  Virtual = kAudioDeviceTransportTypeVirtual,
  PCI = kAudioDeviceTransportTypePCI,
  USB = kAudioDeviceTransportTypeUSB,
  FireWire = kAudioDeviceTransportTypeFireWire,
  Bluetooth = kAudioDeviceTransportTypeBluetooth,
  BluetoothLE = kAudioDeviceTransportTypeBluetoothLE,
  HDMI = kAudioDeviceTransportTypeHDMI,
  DisplayPort = kAudioDeviceTransportTypeDisplayPort,
  AirPlay = kAudioDeviceTransportTypeAirPlay,
  ContinuityCaptureWired = kAudioDeviceTransportTypeContinuityCaptureWired,
  ContinuityCaptureWireless =
      kAudioDeviceTransportTypeContinuityCaptureWireless,
};

}  // namespace ProxyAudio
