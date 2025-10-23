# Proxy Audio: Passthrough Audio Driver

Proxy Audio is a modern C++20 based audio driver that passes data and commands through to another system device.

# Installation

TBD

# CoreAudio Notes

`AudioObjectInterface` heirarchy of ownership.

PlugIn Driver
  ↳ Box
    ↳ Device - Only if box is acquired
      ↳ Stream (Stream)
      ↳ Volume (Control)
      ↳ Mute (Control)
      ↳ DataSource (Control)

# License

MIT License

Copyright (c) 2025 Kevin Venalainen
