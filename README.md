# Proxy Audio: Passthrough Audio Driver

Proxy Audio is a modern C++20 based audio driver that passes data and commands through to another system device.

# Installation

TBD

# CoreAudio Architecture

## AudioObjectInterface Hierarchy

ProxyAudio implements a hierarchy of audio objects that conform to CoreAudio's AudioServerPlugIn architecture. All objects implement the `AudioObjectInterface`, which provides property access methods.

### Object Hierarchy

```
PlugInDriver (ProxyDriverInterface)
  ↳ Box
    ↳ Device (only when box is acquired)
      ↳ I/O Stream
        ↳ I/O Volume Control
        ↳ I/O Mute Control
        ↳ I/O DataSource Control
        ↳ PlayThrough DataDestination Control
```

### Object Descriptions

- **ProxyDriverInterface**: The main plug-in driver. Manages the audio server plugin interface, hosts the object registry, and implements plug-in level properties (manufacturer, resource bundle, device list, etc.).

- **Box**: Represents a physical or virtual audio box. Manages acquisition state and device creation. When acquired, the box creates and owns Device objects. Implements box-specific properties (UID, transport type, audio/video/MIDI capabilities).

- **Device**: Represents an audio device with input/output capabilities. Supports 44.1kHz and 48kHz sample rates with 2-channel 32-bit float LPCM audio. Owns streams and controls. Implements device-specific properties (device UID, model UID, sample rate, latency, icon, etc.).

- **Stream**: Represents a unidirectional audio stream (input or output). Defines the audio format (sample rate, channel count, bit depth) and handles format changes via device configuration requests.

- **Volume**: A level control that manages volume with scalar (0.0-1.0) and decibel (-96dB to +6dB) representations. Provides conversion methods between scalar and dB values with a squared curve for better UI control.

- **Mute**: A boolean control for muting audio on input or output channels.

- **DataSource**: A selector control for choosing between multiple input or output sources (supports 4 items).

- **DataDestination**: A selector control for choosing the playthrough destination (supports 4 items).

### Design Patterns

- **C++ Types Internally**: All classes store state using C++ standard library types (`std::string`, `std::atomic`, `std::mutex`) for memory safety and modern C++ practices.

- **CF Type Conversion**: Conversion to CoreFoundation types (e.g., `CFStringRef`) happens at the API boundary using utility functions like `StringToCFString()` and `CFStringToString()`.

- **Thread Safety**: Mutable state is protected with `std::mutex` locks. Atomic types are used where appropriate for lock-free access.

- **Error Handling**: Exceptions (`ErrorWithCode`) are thrown for invalid operations and caught at the static dispatch layer in `PlugInDriverInterface`.

- **Property Dispatch**: All property methods use switch statements on property selectors for efficient dispatch and clear code organization.

- **Registry Pattern**: The `AudioObjectRegistry` manages object lifecycle and provides ID-based lookup. Objects are stored as `std::shared_ptr` for automatic memory management.

# License

MIT License

Copyright (c) 2025 Kevin Venalainen
