// Copyright (c) libASPL authors
// Licensed under MIT

#include <CoreAudio/AudioServerPlugIn.h>

// Include type_traits before libASPL headers to ensure std::is_trivial is
// available when libASPL's DoubleBuffer.hpp is parsed (it uses std::is_trivial
// without including it)
#include <aspl/Driver.hpp>
#include <cmath>
#include <limits>

#include "Utils.hpp"

namespace {

// Sinewave.
constexpr UInt32 SineFrequency = 500;

// Stream format.
constexpr UInt32 SampleRate = 44100;
constexpr UInt32 ChannelCount = 2;

// Control and I/O request handler.
class ProxyAudioHandler : public aspl::ControlRequestHandler, public aspl::IORequestHandler
{
public:
    void OnReadClientInput(const std::shared_ptr<aspl::Client>& client,
        const std::shared_ptr<aspl::Stream>& stream,
        Float64 zeroTimestamp,
        Float64 timestamp,
        void* bytes,
        UInt32 bytesCount) override
    {
        SInt16* samples = (SInt16*)bytes;
        UInt32 numSamples = bytesCount / sizeof(SInt16) / ChannelCount;

        for (UInt32 n = 0; n < numSamples; n++) {
            for (UInt32 c = 0; c < ChannelCount; c++) {
                samples[n * ChannelCount + c] = ConvertSample(MakeSample(timestamp, n));
            }
        }
    }

private:
    double MakeSample(Float64 timestamp, UInt32 n)
    {
        return std::sin(2 * M_PI / SampleRate * SineFrequency * (timestamp + n));
    }

    SInt16 ConvertSample(double s)
    {
        constexpr SInt16 SInt16Min = std::numeric_limits<SInt16>::min();
        constexpr SInt16 Sint16Max = std::numeric_limits<SInt16>::max();

        s *= (Sint16Max + 1.0);

        return s < SInt16Min          ? SInt16Min // clip
               : s >= Sint16Max + 1.0 ? Sint16Max // clip
                                      : SInt16(s);
    }
};

std::shared_ptr<aspl::Driver> CreateProxyAudioDriver()
{
  auto tracer = std::make_shared<aspl::Tracer>();
  tracer->Message("Creating Proxy Audio Driver");

  // Create context, shared between all other objects.
  // You can provide custom tracer here.
  auto context = std::make_shared<aspl::Context>();

  auto outputDevices = EnumerateAudioOutputDevices(context);

  tracer->Message("Found %zu output devices", outputDevices.size());
  for (auto deviceID : outputDevices) {
    tracer->Message("Output device: %u", deviceID);
  }

  // Create device object with some custom parameters.
  aspl::DeviceParameters deviceParams;
  deviceParams.Name = "Proxy Audio Device";
  deviceParams.Manufacturer = "Tap Turtle";
  deviceParams.SampleRate = SampleRate;
  deviceParams.ChannelCount = ChannelCount;

  auto device = std::make_shared<aspl::Device>(context, deviceParams);

  // Add to device one stream, one volume control, and one mute control.
  // Associate volume and mute control with the stream.
  //
  // If desired, you can provide parameters for streams and controls as well,
  // but for simplicity we use defaults here.
  //
  // HAL will use stream and controls to determine how to work with our device
  // and to store volume and mute settings.
  //
  // IORequestHandler will use stream to apply stored volume and mute settings.
  device->AddStreamWithControlsAsync(aspl::Direction::Input);
  device->AddStreamWithControlsAsync(aspl::Direction::Output);

  // Create and set custom handler for both control and I/O requests.
  // You can use separate handlers, but here we use one.
  auto handler = std::make_shared<ProxyAudioHandler>();

  device->SetControlHandler(handler);
  device->SetIOHandler(handler);

  // Create plugin object, the root of the object hierarchy, and add
  // our device to it.
  //
  // The main purpose of plugin is to provide the list of devices to HAL.
  //
  // For simplicity we use default parameters.
  auto plugin = std::make_shared<aspl::Plugin>(context);

  plugin->AddDevice(device);

  // Create driver, the top-level entry point.
  // Driver owns plugin object and thus the whole object hierarchy,
  // and provides C interface for HAL.
  auto driver = std::make_shared<aspl::Driver>(context, plugin);

  return driver;
}

} // namespace

extern "C" void* ProxyAudioEntryPoint(CFAllocatorRef allocator, CFUUIDRef typeUUID)
{
    // The UUID of the plug-in type (443ABAB8-E7B3-491A-B985-BEB9187030DB).
    if (!CFEqual(typeUUID, kAudioServerPlugInTypeUUID)) {
        return nullptr;
    }

    // Store shared pointer to the driver to keep it alive.
    static std::shared_ptr<aspl::Driver> driver = CreateProxyAudioDriver();

    return driver->GetReference();
}
