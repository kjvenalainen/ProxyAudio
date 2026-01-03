// Copyright (c) libASPL authors
// Licensed under MIT

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>

// Include type_traits before libASPL headers to ensure std::is_trivial is
// available when libASPL's DoubleBuffer.hpp is parsed (it uses std::is_trivial
// without including it)
#include <aspl/Driver.hpp>
#include <cmath>
#include <limits>

#include "CommonProperties.hpp"
#include "Error.hpp"
#include "ProxyDevice.hpp"
#include "Tracer.hpp"
#include "Utils.hpp"

namespace {

// Sinewave.
constexpr UInt32 SineFrequency = 500;

// Stream format.
constexpr UInt32 SampleRate = 44100;
constexpr UInt32 ChannelCount = 2;

class DriverHandler : public aspl::DriverRequestHandler {
 public:
  DriverHandler(std::shared_ptr<aspl::Context> context) : context_(context) {}

  // Invoked when HAL performs asynchrnous initialization.
  OSStatus OnInitialize() override { return kAudioHardwareNoError; }

 private:
  std::shared_ptr<aspl::Context> context_;
};

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
  auto tracer = std::make_shared<ProxyAudio::Tracer>();

  // Create context, shared between all other objects.
  // You can provide custom tracer here.
  auto context = std::make_shared<aspl::Context>(tracer);

  AudioObjectID targetDeviceId = kAudioObjectUnknown;
  try {
    auto outputDevices = ProxyAudio::EnumerateAudioOutputDevices(context);
    tracer->Message("CreateProxyAudioDriver:Found %zu output devices",
                    outputDevices.size());

    for (auto deviceID : outputDevices) {
      auto deviceName = ProxyAudio::GetDeviceNameProperty(deviceID);
      tracer->Message("CreateProxyAudioDriver:Output device: %u - %s", deviceID,
                      deviceName.c_str());

      if (deviceName == "MacBook Pro Speakers") {
        tracer->Message("CreateProxyAudioDriver:Found target device: %u - %s",
                        deviceID, deviceName.c_str());
        targetDeviceId = deviceID;
      }
    }

  } catch (const ProxyAudio::OSStatusError& e) {
    tracer->Message(
        "CreateProxyAudioDriver:Failed to enumerate output devices: %s",
        e.what());
    return nullptr;
  }

  // Create plugin object, the root of the object hierarchy, and add
  // our device to it.
  //
  // The main purpose of plugin is to provide the list of devices to HAL.
  //
  // For simplicity we use default parameters.
  auto plugin = std::make_shared<aspl::Plugin>(context);

  if (targetDeviceId != kAudioObjectUnknown) {
    try {
      context->Tracer->Message(
          "CreateProxyAudioDriver:Creating proxy device for target device: %u",
          targetDeviceId);

      auto proxyDevice =
          std::make_shared<ProxyAudio::ProxyDevice>(targetDeviceId, context);
      proxyDevice->AddStreamWithControlsAsync(aspl::Direction::Output);

      auto proxyDeviceHandler = std::make_shared<ProxyAudioHandler>();
      proxyDevice->SetControlHandler(proxyDeviceHandler);
      proxyDevice->SetIOHandler(proxyDeviceHandler);

      plugin->AddDevice(std::move(proxyDevice));
    } catch (const ProxyAudio::OSStatusError& e) {
      // We already logged the error, so just continue with no device.
      context->Tracer->Message(
          "CreateProxyAudioDriver:Failed to create proxy device: %s", e.what());
    }
  }

  // Create driver, the top-level entry point.
  // Driver owns plugin object and thus the whole object hierarchy,
  // and provides C interface for HAL.
  auto driver = std::make_shared<aspl::Driver>(context, plugin);
  auto driverHandler = std::make_shared<DriverHandler>(context);
  driver->SetDriverHandler(std::move(driverHandler));

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
