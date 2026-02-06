// Copyright (c) libASPL authors
// Licensed under MIT

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>

// Include type_traits before libASPL headers to ensure std::is_trivial is
// available when libASPL's DoubleBuffer.hpp is parsed (it uses std::is_trivial
// without including it)
#include <aspl/Driver.hpp>
#include <cmath>

#include "CommonProperties.hpp"
#include "Dispatch.hpp"
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
  DriverHandler(std::shared_ptr<aspl::Context> context,
                std::shared_ptr<aspl::Plugin> plugin)
      : context_(context), plugin_(plugin) {}

  // Invoked when HAL performs asynchrnous initialization.
  OSStatus OnInitialize() override {
    // Asynchronously add devices to the plugin. In this method the HAL is
    // holding various locks that would cause a long delay if we were to do this
    // synchronously.
    ProxyAudio::DispatchAsync(^() {
      AddDevices();
    });

    return kAudioHardwareNoError;
  }

 private:
  void AddDevices() {
    const auto tracer = ProxyAudio::Tracer::FromTracer(context_->Tracer);

    try {
      bool addedDevices = false;
      auto outputDevices = ProxyAudio::EnumerateAudioOutputDevices(context_);
      tracer->Message(ProxyAudio::Tracer::Info,
                      "DriverHandler:OnInitialize(): Found %zu output devices",
                      outputDevices.size());

      for (auto deviceID : outputDevices) {
        try {
          auto deviceName = ProxyAudio::GetDeviceNameProperty(deviceID);
          auto deviceTree = ProxyAudio::DumpDeviceTree(deviceID);
          tracer->Message(
              ProxyAudio::Tracer::Info,
              "DriverHandler:OnInitialize(): Output device: %u - %s \n%s",
              deviceID, deviceName.c_str(), deviceTree.c_str());

          // Add all devices that have at least one output stream.
          auto outputStreamCount =
              ProxyAudio::GetPropertyDataSize(
                  deviceID,
                  {
                      .mSelector = kAudioDevicePropertyStreams,
                      .mScope = kAudioObjectPropertyScopeOutput,
                      .mElement = kAudioObjectPropertyElementMain,
                  },
                  {}) /
              sizeof(AudioObjectID);

          if (outputStreamCount > 0) {
            auto proxyDevice =
                ProxyAudio::ProxyDevice::Create(deviceID, context_);
            plugin_->AddDevice(std::move(proxyDevice));
            addedDevices = true;
          }
        } catch (const ProxyAudio::OSStatusError& e) {
          tracer->Message(
              ProxyAudio::Tracer::Warn,
              "DriverHandler:OnInitialize(): Failed to add device %u: %s",
              deviceID, e.what());
        }
      }
    } catch (const ProxyAudio::OSStatusError& e) {
      tracer->Message(ProxyAudio::Tracer::Error,
                      "DriverHandler:OnInitialize(): Failed to add devices: %s",
                      e.what());
    }
  }

  std::shared_ptr<aspl::Context> context_;
  std::shared_ptr<aspl::Plugin> plugin_;
};

std::shared_ptr<aspl::Driver> CreateProxyAudioDriver() {
  auto tracer = std::make_shared<ProxyAudio::Tracer>(
      ProxyAudio::Tracer::Mode::Syslog, ProxyAudio::Tracer::Style::Hierarchical,
      ProxyAudio::Tracer::Level::Info);

  // Create context, shared between all other objects.
  auto context = std::make_shared<aspl::Context>(tracer);

  // Create plugin object, the root of the object hierarchy. Don't add any
  // devices yet.
  auto plugin = std::make_shared<aspl::Plugin>(context);

  // Create driver, the top-level entry point.
  // Driver owns plugin object and thus the whole object hierarchy,
  // and provides C interface for HAL.
  auto driver = std::make_shared<aspl::Driver>(context, plugin);
  auto driverHandler = std::make_shared<DriverHandler>(context, plugin);
  driver->SetDriverHandler(std::move(driverHandler));

  return driver;
}

}  // namespace

extern "C" void* ProxyAudioEntryPoint(CFAllocatorRef allocator,
                                      CFUUIDRef typeUUID) {
  // The UUID of the plug-in type (443ABAB8-E7B3-491A-B985-BEB9187030DB).
  if (!CFEqual(typeUUID, kAudioServerPlugInTypeUUID)) {
    return nullptr;
  }

  // Store shared pointer to the driver to keep it alive.
  static std::shared_ptr<aspl::Driver> driver = CreateProxyAudioDriver();

  return driver->GetReference();
}
