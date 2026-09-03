# ProxyAudio

ProxyAudio is a macOS Core Audio HAL plug-in that creates a virtual output device for each eligible physical output device. Each virtual device is named `<device name> (Proxy)` and forwards its audio, volume, mute, stream, and relevant device properties to its corresponding target.

It is intended as a transparent layer for audio routing, monitoring, or further processing. The driver is implemented in C++17 on top of the bundled [libASPL](https://github.com/gavv/libASPL) submodule.

## How it works

The driver observes the system's output devices and creates or removes proxy devices as those targets appear and disappear. Proxy devices are excluded from discovery, so they are never proxied again.

Audio written by an application to a proxy device is placed in a ring buffer and consumed by an I/O procedure on the target device. The buffer decouples the two I/O cycles. A clock adjustment keeps the buffer near its target fill level: when it is too full, the proxy reports a faster clock so HAL produces data more slowly; when it is too empty, it reports a slower clock so HAL produces data more quickly.

## Requirements

- macOS with the Core Audio development frameworks and command-line tools available
- CMake and a C++17-capable compiler (normally supplied by Xcode or the Xcode Command Line Tools)
- Git, including the `libASPL` submodule

The optional SwiftUI manager app targets macOS 13 or later. Building the driver does not require the manager app.

## Build

Clone the repository with its submodule:

```bash
git clone --recurse-submodules https://github.com/kjvenalainen/ProxyAudio.git
cd ProxyAudio
```

Then build either configuration:

```bash
make release  # default: make
make debug
```

The build produces the driver bundle at `build/ProxyAudio/<configuration>/ProxyAudio.driver`, updates `build/latest` to point to that bundle, and writes a repository-root `compile_commands.json` for language servers.

To build and run the deterministic unit-test suite:

```bash
make test
```

The test build fetches GoogleTest the first time it is configured.

### Manager app

After building a driver, build the optional installer/status app with:

```bash
make app
open build/ProxyAudioManager.app
```

The app embeds the bundle referenced by `build/latest`; build the desired Debug or Release driver first.

## Install and remove

Install the most recently built driver:

```bash
make install
```

The script validates `build/latest`, stops `coreaudiod`, replaces `/Library/Audio/Plug-Ins/HAL/ProxyAudio.driver`, and lets macOS restart the audio daemon. It prompts for administrator credentials. Audio playback will be interrupted while Core Audio restarts.

Remove the installed driver:

```bash
make uninstall
```

`make uninstall` also requires the `build/latest` link so it can identify the installed bundle. If the build directory has been removed, delete `/Library/Audio/Plug-Ins/HAL/ProxyAudio.driver` manually with administrator privileges, then restart `coreaudiod`.

## Development notes

- `make clean` removes build products, generated documentation output, and `compile_commands.json`.
- `make fmt` formats non-generated C++ source files with `clang-format`.
- Local adjustments to the `libASPL` submodule are maintained as patches in [`patches/`](patches/README.md). The build applies them before building libASPL.

## Troubleshooting

**No proxy device appears.** Confirm the bundle exists at `/Library/Audio/Plug-Ins/HAL/ProxyAudio.driver`, then wait briefly after installation or restart Core Audio. Inspect the system log with Console or `log stream` for messages from Core Audio and ProxyAudio.

**Audio drops out or has unexpected latency.** Check that the physical target device is working without ProxyAudio and reduce system load. The proxy ring buffer trades additional latency for decoupling of the virtual and physical device I/O cycles.

**A build cannot initialize libASPL.** Ensure the submodule is present with `git submodule update --init --recursive`, then consult the patch workflow in [`patches/README.md`](patches/README.md) if a local patch no longer applies.

## License and dependencies

ProxyAudio is released under the [MIT License](LICENSE.txt).

The bundled libASPL dependency has its own [license](libASPL/LICENSE) and includes Apple-licensed portions; see its [documentation](libASPL/README.md) for details.
