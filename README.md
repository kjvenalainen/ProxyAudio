# ProxyAudio

ProxyAudio is a macOS CoreAudio plugin that creates virtual audio devices which proxy audio data and control commands to existing physical audio devices. Built with modern C++17 and leveraging the [libASPL](https://github.com/Devolutions/libASPL) framework, ProxyAudio provides a transparent passthrough layer that enables advanced audio processing, monitoring, and routing capabilities.

## Overview

ProxyAudio creates proxy devices that appear in macOS System Preferences as separate audio output devices. When audio is routed to a proxy device, it is buffered and then forwarded to the underlying physical device. This architecture enables:

- **Transparent audio passthrough** with minimal latency overhead
- **Volume and mute control** proxying with real-time processing

## Features

### Core Functionality

- **Device Proxying**: Creates virtual devices that mirror physical audio devices
- **Stream Cloning**: Automatically clones all input and output streams from target devices
- **Property Synchronization**: Maintains synchronized properties (sample rate, latency, etc.) with target devices
- **Volume & Mute Controls**: Proxies volume and mute controls with real-time audio processing

## Architecture

### Proxy Pattern

ProxyAudio uses a comprehensive proxy pattern to clone audio objects:

- **ProxyDevice**: Proxies the physical audio device, cloning streams and controls
- **ProxyStream**: Proxies audio streams, maintaining format and latency properties
- **ProxyVolumeControl**: Proxies volume controls with real-time processing
- **ProxyMuteControl**: Proxies mute controls with real-time processing
- **ProxyProperty**: Generic property synchronization mechanism

### I/O Architecture

```
┌─────────────────┐
│  macOS HAL      │
│  (Producer)     │
└────────┬────────┘
         │ OnWriteMixedOutput()
         ▼
┌─────────────────┐
│  Ring Buffer    │  ◄── Adaptive Clock Control
│  (audio frames) │     (maintains 50% fill)
└────────┬────────┘
         │ TargetIOProc()
         ▼
┌─────────────────┐
│  Target Device  │
│  (Consumer)     │
└─────────────────┘
```

### Ring Buffer

The ring buffer decouples the proxy device's I/O cycle from the target device's I/O cycle.

### Adaptive Clock Control

ProxyAudio implements an adaptive clock mechanism that steers the HAL's write rate by adjusting the reported device clock:

- Buffer too full → Advance clock → HAL writes slower
- Buffer too empty → Retard clock → HAL writes faster

## Requirements

- **macOS**: 10.9 or later (CoreAudio HAL plugin support)
- **C++17**: Modern C++ standard required
- **Xcode**: For building the project
- **libASPL**: Included as a git submodule

## Building

ProxyAudio is built using CMake. The repository includes default build tasks for VSCode and derived editors.

1. **Clone the repository**:
   ```bash
   git clone --recurse-submodules https://github.com/kjvenalainen/ProxyAudio
   cd ProxyAudio
   ```

2. **Build the project**:
   ```bash
   make [release|debug]
   ```

4. **Build artifacts**:
   - The driver bundle will be created in the build directory.
   - A symlink at `build/latest` points to the most recent build.
   - `compile_commands.json` is generated in repo root for `clangd` support.

## Installation

After building, install the driver using the provided script:

```bash
sudo ./script/install.sh
```

This script will:
1. Validate that build products exist
2. Copy the driver bundle to `/Library/Audio/Plug-Ins/HAL/`
3. Restart the CoreAudio daemon

**Note**: Installation requires administrator privileges. The CoreAudio daemon restart may briefly interrupt audio playback.

### Manual Installation

If you prefer manual installation:

```bash
# Copy the driver bundle
sudo cp -R build/latest /Library/Audio/Plug-Ins/HAL/

# Restart CoreAudio
sudo killall -9 coreaudiod
```

## License

MIT License

Copyright (c) 2026 Tap Turtle

See [LICENSE.txt](LICENSE.txt) for full license text.

## Dependencies

- **libASPL**: Audio Server Plugin Library (included as submodule)
  - License: MIT (see `libASPL/LICENSE`)
  - Apple code portions: Apple 2012/2020 licenses (see `libASPL/LICENSE.apple*`)

## Troubleshooting

### Driver Not Appearing

- Verify installation: Check `/Library/Audio/Plug-Ins/HAL/` for the driver bundle
- Check logs: Use Console.app or `log stream` to view coreaudiod logs
- Restart CoreAudio: `sudo killall -9 coreaudiod`

### Audio Dropouts

- Check buffer size: Larger buffers reduce dropouts but increase latency
- Verify target device: Ensure target device is functioning correctly
- Check system load: High CPU usage can cause I/O issues

### Device Not Found

- Verify device name: Check exact device name in System Preferences → Sound
- Check enumeration: Review logs to see which devices are found
- Update configuration: Modify `Driver.cpp` to match your device names

## See Also

- [libASPL Documentation](https://github.com/Devolutions/libASPL)
- [CoreAudio HAL Documentation](https://developer.apple.com/documentation/coreaudio)
- [Audio Hardware Abstraction Layer](https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/CoreAudioOverview/CoreAudioOverview.pdf)
