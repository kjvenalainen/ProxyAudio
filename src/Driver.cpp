// Copyright (c) libASPL authors
// Licensed under MIT

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>

// Include type_traits before libASPL headers to ensure std::is_trivial is
// available when libASPL's DoubleBuffer.hpp is parsed (it uses std::is_trivial
// without including it)
#include <aspl/Driver.hpp>
#include <cmath>

#include "AudioObjectUtils.hpp"
#include "CommonProperties.hpp"
#include "Dispatch.hpp"
#include "Error.hpp"
#include "PropertyChangedNotifier.hpp"
#include "ProxyDevice.hpp"
#include "ProxyProperty.hpp"
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
      : context_(context), plugin_(plugin), onSystemAudioDevicesChanged_() {}

  // Invoked when HAL performs asynchrnous initialization.
  OSStatus OnInitialize() override {
    // Asynchronously add devices to the plugin. In this method the HAL is
    // holding various locks that would cause a long delay if we were to do this
    // synchronously.
    ProxyAudio::DispatchAsync(^() {
      AddDevices();

      onSystemAudioDevicesChanged_ =
          std::make_unique<ProxyAudio::PropertyChangedNotifier>(
              kAudioObjectSystemObject, context_,
              AudioObjectPropertyAddress{
                  kAudioHardwarePropertyDevices,
                  kAudioObjectPropertyScopeGlobal,
                  kAudioObjectPropertyElementMain,
              },
              [this]() { this->AddDevices(); });
      
    });

    return kAudioHardwareNoError;
  }

 private:
  void AddDevices() {
    const auto tracer = ProxyAudio::Tracer::FromTracer(context_->Tracer);

    // Note all of our current proxy devices.
    const auto proxyDeviceCount = plugin_->GetDeviceCount();
    // (targetDeviceID, deviceUID)
    std::vector<std::shared_ptr<ProxyAudio::ProxyDevice>> proxyDevices(
        proxyDeviceCount);
    for (auto i = 0; i < proxyDeviceCount; i++) {
      proxyDevices[i] = std::static_pointer_cast<ProxyAudio::ProxyDevice>(
          plugin_->GetDeviceByIndex(i));
    }

    try {
      bool addedDevices = false;
      auto systemOutputDevices =
          ProxyAudio::EnumerateAudioOutputDevices(context_);
      tracer->Message(ProxyAudio::Tracer::Info,
                      "DriverHandler:OnInitialize(): Found %zu output devices",
                      systemOutputDevices.size());

      // For any proxy device that no longer has a target device, remove it.
      for (auto proxyDevice : proxyDevices) {
        if (std::find(systemOutputDevices.begin(), systemOutputDevices.end(),
                      proxyDevice->GetTargetObjectID()) ==
            systemOutputDevices.end()) {
          plugin_->RemoveDevice(proxyDevice);
        }
      }

      for (auto deviceID : systemOutputDevices) {
        try {
          // Skip proxy devices.
          const auto deviceUID = ProxyAudio::SafeValueOr<std::string>(
              [deviceID]() {
                return ProxyAudio::GetDeviceUIDProperty(deviceID);
              },
              "", ProxyAudio::Tracer::FromTracer(context_->Tracer).get());
          if (deviceUID.find(ProxyAudio::PROXY_DEVICE_SUFFIX) !=
              std::string::npos) {
            continue;
          }

          auto deviceName = ProxyAudio::GetDeviceNameProperty(deviceID);
          auto deviceTree = ProxyAudio::DumpDeviceTree(deviceID);
          tracer->Message(
              ProxyAudio::Tracer::Info,
              "DriverHandler:OnInitialize(): Output device: %u - %s \n%s",
              deviceID, deviceName.c_str(), deviceTree.c_str());

          // Add all devices that have at least one output stream, and that
          // don't have a volume+mute control.
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

  bool DeviceHasVolumeAndMuteControl(AudioObjectID deviceID) {
    try {
      const auto controls =
          ProxyAudio::GetPropertyData<std::vector<AudioObjectID>>(
              deviceID,
              {
                  .mSelector = kAudioObjectPropertyControlList,
                  .mScope = kAudioObjectPropertyScopeOutput,
                  .mElement = kAudioObjectPropertyElementMain,
              },
              {});

      bool hasVolumeControl =
          ProxyAudio::GetControlId(kAudioVolumeControlClassID,
                                   kAudioDevicePropertyScopeOutput,
                                   controls) != kAudioObjectUnknown;
      bool hasMuteControl =
          ProxyAudio::GetControlId(kAudioMuteControlClassID,
                                   kAudioDevicePropertyScopeOutput,
                                   controls) != kAudioObjectUnknown;

      return hasVolumeControl && hasMuteControl;
    } catch (const ProxyAudio::OSStatusError& e) {
      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Error,
                    "DriverHandler:DeviceHasVolumeAndMuteControl(): Failed to "
                    "get controls: %s",
                    e.what());
      return false;
    }
  }

  std::shared_ptr<aspl::Context> context_;
  std::shared_ptr<aspl::Plugin> plugin_;
  std::unique_ptr<ProxyAudio::PropertyChangedNotifier>
      onSystemAudioDevicesChanged_;
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
