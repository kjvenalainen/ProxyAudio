// Copyright (c) libASPL authors
// Licensed under MIT

#include <CoreAudio/AudioHardwareBase.h>
#include <CoreAudio/AudioServerPlugIn.h>

// Include type_traits before libASPL headers to ensure std::is_trivial is
// available when libASPL's DoubleBuffer.hpp is parsed (it uses std::is_trivial
// without including it)
#include <aspl/Driver.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <memory>
#include <string>

#include "AudioObjectUtils.hpp"
#include "CFUtils.hpp"
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

// The manager reads this read-only property from the AudioPlugIn object after
// resolving it by bundle identifier. It deliberately returns a property list,
// rather than a bare version string, so the protocol can evolve safely.
constexpr AudioObjectPropertySelector DriverVersionProperty = 'VRSN';
constexpr char DriverStatusStorageKey[] =
    "com.tapturtle.proxyaudio.driver-status.v1";
constexpr SInt64 DriverStatusSchemaVersion = 1;
constexpr SInt64 DriverStatusProtocolVersion = 1;

class DriverStatus {
 public:
  DriverStatus()
      : instanceID_(CreateInstanceID()), startedAtUnixMs_(CurrentUnixMs()) {}

  // The returned property list follows the Core Foundation Create Rule. The
  // caller (libASPL) owns it and releases it after sending it to the client.
  CFPropertyListRef CreatePropertyList() const {
    auto* status = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                             &kCFTypeDictionaryKeyCallBacks,
                                             &kCFTypeDictionaryValueCallBacks);
    if (!status) {
      return nullptr;
    }

    SetInteger(status, CFSTR("schemaVersion"), DriverStatusSchemaVersion);
    SetInteger(status, CFSTR("protocolVersion"), DriverStatusProtocolVersion);
    SetString(status, CFSTR("bundleIdentifier"), PROXY_AUDIO_DRIVER_IDENTIFIER);
    SetString(status, CFSTR("driverVersion"), PROXY_AUDIO_DRIVER_VERSION);
    SetString(status, CFSTR("buildVersion"), PROXY_AUDIO_DRIVER_BUILD_VERSION);
    SetString(status, CFSTR("instanceID"), instanceID_);
    SetInteger(status, CFSTR("startedAtUnixMs"), startedAtUnixMs_);
    CFDictionarySetValue(status, CFSTR("live"),
                         initialized_.load(std::memory_order_acquire)
                             ? kCFBooleanTrue
                             : kCFBooleanFalse);
    CFDictionarySetValue(status, CFSTR("state"),
                         initialized_.load(std::memory_order_acquire)
                             ? CFSTR("ready")
                             : CFSTR("initializing"));

    const void* capabilities[] = {CFSTR("status-v1")};
    auto* capabilityList = CFArrayCreate(kCFAllocatorDefault, capabilities, 1,
                                         &kCFTypeArrayCallBacks);
    if (capabilityList) {
      CFDictionarySetValue(status, CFSTR("capabilities"), capabilityList);
      CFRelease(capabilityList);
    }

    return status;
  }

  void MarkInitialized() {
    initialized_.store(true, std::memory_order_release);
  }

  bool WriteToStorage(const std::shared_ptr<aspl::Storage>& storage) const {
    const auto status = CreatePropertyList();
    if (!status) {
      return false;
    }

    const auto didWrite = storage->WriteCustom(DriverStatusStorageKey, status);
    CFRelease(status);
    return didWrite;
  }

 private:
  static std::string CreateInstanceID() {
    auto* uuid = CFUUIDCreate(kCFAllocatorDefault);
    if (!uuid) {
      return "";
    }

    auto* string = CFUUIDCreateString(kCFAllocatorDefault, uuid);
    CFRelease(uuid);
    if (!string) {
      return "";
    }

    const auto result = ProxyAudio::StringFromCFStringRef(string);
    CFRelease(string);
    return result;
  }

  static SInt64 CurrentUnixMs() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  }

  static void SetInteger(CFMutableDictionaryRef dictionary,
                         CFStringRef key,
                         SInt64 value) {
    auto* number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
    if (number) {
      CFDictionarySetValue(dictionary, key, number);
      CFRelease(number);
    }
  }

  static void SetString(CFMutableDictionaryRef dictionary,
                        CFStringRef key,
                        const std::string& value) {
    auto* string = CFStringCreateWithCString(kCFAllocatorDefault, value.c_str(),
                                             kCFStringEncodingUTF8);
    if (string) {
      CFDictionarySetValue(dictionary, key, string);
      CFRelease(string);
    }
  }

  const std::string instanceID_;
  const SInt64 startedAtUnixMs_;
  std::atomic<bool> initialized_{false};
};

class DriverHandler : public aspl::DriverRequestHandler {
 public:
  DriverHandler(std::shared_ptr<aspl::Context> context,
                std::shared_ptr<aspl::Plugin> plugin,
                std::shared_ptr<aspl::Storage> storage,
                std::shared_ptr<DriverStatus> driverStatus)
      : context_(context),
        plugin_(plugin),
        storage_(storage),
        driverStatus_(driverStatus),
        onSystemAudioDevicesChanged_() {}

  // Invoked when HAL performs asynchrnous initialization.
  OSStatus OnInitialize() override {
    driverStatus_->MarkInitialized();
    if (!driverStatus_->WriteToStorage(storage_)) {
      ProxyAudio::Tracer::FromTracer(context_->Tracer)
          ->Message(ProxyAudio::Tracer::Warn,
                    "DriverHandler:OnInitialize() Failed to write driver "
                    "status to storage");
    }

    // Asynchronously add devices to the plugin. In this method the HAL is
    // holding various locks that would cause a long delay if we were to do this
    // synchronously.
    ProxyAudio::DispatchAsync(^() {
      static std::atomic<bool> isAddingDevices = true;
      AddDevices(context_, plugin_);
      isAddingDevices.store(false);

      onSystemAudioDevicesChanged_ =
          std::make_unique<ProxyAudio::PropertyChangedNotifier>(
              kAudioObjectSystemObject, context_,
              AudioObjectPropertyAddress{
                  kAudioHardwarePropertyDevices,
                  kAudioObjectPropertyScopeGlobal,
                  kAudioObjectPropertyElementMain,
              },
              [context = context_, plugin = plugin_]() {
                if (isAddingDevices.exchange(true)) {
                  ProxyAudio::Tracer::FromTracer(context->Tracer)
                      ->Message(ProxyAudio::Tracer::Info,
                                "DriverHandler:AddDevices(): Other "
                                "thread is already "
                                "adding devices, skipping.");

                  return;
                }

                ProxyAudio::DispatchAsync(^() {
                  AddDevices(context, plugin);
                  isAddingDevices.store(false);
                });
              });
    });

    return kAudioHardwareNoError;
  }

 private:
  static void AddDevices(std::shared_ptr<aspl::Context> context,
                         std::shared_ptr<aspl::Plugin> plugin) {
    const auto& tracer = ProxyAudio::Tracer::FromTracer(context->Tracer);

    // Note all of our current proxy devices.
    const auto proxyDeviceCount = plugin->GetDeviceCount();
    // (targetDeviceID, deviceUID)
    std::vector<std::shared_ptr<ProxyAudio::ProxyDevice>> proxyDevices(
        proxyDeviceCount);
    for (auto i = 0; i < proxyDeviceCount; i++) {
      proxyDevices[i] = std::static_pointer_cast<ProxyAudio::ProxyDevice>(
          plugin->GetDeviceByIndex(i));
    }

    try {
      // Track whether we need to notify HAL that the properties have changed.
      bool changedDevices = false;

      auto systemOutputDevices =
          ProxyAudio::EnumerateAudioOutputDevices(context);
      tracer->Message(ProxyAudio::Tracer::Info,
                      "DriverHandler:AddDevices(): Found %zu output devices, "
                      "%zu current proxy devices",
                      systemOutputDevices.size(), proxyDevices.size());

      // For any proxy device that no longer has a target device, remove it.
      for (auto proxyDevice : proxyDevices) {
        if (std::find(systemOutputDevices.begin(), systemOutputDevices.end(),
                      proxyDevice->GetTargetObjectID()) ==
            systemOutputDevices.end()) {
          tracer->Message(
              ProxyAudio::Tracer::Info,
              "DriverHandler:AddDevices(): Removing proxy device: %u",
              proxyDevice->GetTargetObjectID());

          plugin->RemoveDevice(proxyDevice, false);
          changedDevices = true;
        }
      }

      auto proxyTargetDevicesToAdd = std::vector<AudioObjectID>();
      for (auto deviceID : systemOutputDevices) {
        try {
          // Skip proxy devices.
          const auto deviceUID = ProxyAudio::SafeValueOr<std::string>(
              [deviceID]() {
                return ProxyAudio::GetDeviceUIDProperty(deviceID);
              },
              "", ProxyAudio::Tracer::FromTracer(context->Tracer).get());
          auto deviceName = ProxyAudio::SafeValueOr<std::string>(
              [deviceID]() {
                return ProxyAudio::GetDeviceNameProperty(deviceID);
              },
              "UNKNOWN", ProxyAudio::Tracer::FromTracer(context->Tracer).get());
          if (deviceUID.find(ProxyAudio::PROXY_DEVICE_SUFFIX) !=
              std::string::npos) {
            tracer->Message(
                ProxyAudio::Tracer::Info,
                "DriverHandler:AddDevices(): Skipping proxy device: %u - %s",
                deviceID, deviceName.c_str());

            continue;
          }

          // If a device already has a proxy device, skip it.
          if (std::find_if(
                  proxyDevices.begin(), proxyDevices.end(),
                  [deviceID](const std::shared_ptr<ProxyAudio::ProxyDevice>&
                                 proxyDevice) {
                    return proxyDevice->GetTargetObjectID() == deviceID;
                  }) != proxyDevices.end()) {
            tracer->Message(
                ProxyAudio::Tracer::Info,
                "DriverHandler:AddDevices(): Skipping device: %u - %s",
                deviceID, deviceName.c_str());

            continue;
          }

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

          // TODO: Make Proxy for all output devices.
          if (outputStreamCount > 0 &&
              (!DeviceHasVolumeAndMuteControl(deviceID, context) || true)) {
            tracer->Message(
                ProxyAudio::Tracer::Info,
                "DriverHandler:AddDevices(): Adding output device: %u - %s",
                deviceID, deviceName.c_str());

            proxyTargetDevicesToAdd.push_back(deviceID);
          }
        } catch (const ProxyAudio::OSStatusError& e) {
          tracer->Message(
              ProxyAudio::Tracer::Warn,
              "DriverHandler:AddDevices(): Failed to add device %u: %s",
              deviceID, e.what());
        } catch (...) {
          tracer->Message(ProxyAudio::Tracer::Warn,
                          "DriverHandler:AddDevices(): Generic exception "
                          "failed to add device %u",
                          deviceID);
        }
      }

      if (!proxyTargetDevicesToAdd.empty()) {
        // Dispatch creation of proxy devices one per thread in case any block.
        std::vector<std::future<std::shared_ptr<ProxyAudio::ProxyDevice>>>
            proxyDevicesFutures;
        for (auto deviceID : proxyTargetDevicesToAdd) {
          proxyDevicesFutures.push_back(
              std::async(std::launch::async, ProxyAudio::ProxyDevice::Create,
                         deviceID, context));
        }

        for (auto& future : proxyDevicesFutures) {
          auto proxyDevice = future.get();
          if (proxyDevice) {
            plugin->AddDevice(std::move(proxyDevice), false);
          }
        }

        changedDevices = true;
      }

      if (changedDevices) {
        // Notify HAL only once for the changes.
        plugin->NotifyPropertiesChanged(
            {kAudioObjectPropertyOwnedObjects, kAudioPlugInPropertyDeviceList});
      }
    } catch (const ProxyAudio::OSStatusError& e) {
      tracer->Message(ProxyAudio::Tracer::Error,
                      "DriverHandler:AddDevices(): Failed to add devices: %s",
                      e.what());
    }
  }

  static bool DeviceHasVolumeAndMuteControl(
      AudioObjectID deviceID,
      std::shared_ptr<aspl::Context> context) {
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
      ProxyAudio::Tracer::FromTracer(context->Tracer)
          ->Message(ProxyAudio::Tracer::Error,
                    "DriverHandler:DeviceHasVolumeAndMuteControl(): Failed to "
                    "get controls: %s",
                    e.what());
      return false;
    }
  }

  std::shared_ptr<aspl::Context> context_;
  std::shared_ptr<aspl::Plugin> plugin_;
  std::shared_ptr<aspl::Storage> storage_;
  std::shared_ptr<DriverStatus> driverStatus_;
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
  auto driverStatus = std::make_shared<DriverStatus>();
  plugin->RegisterCustomProperty(DriverVersionProperty,
                                 [driverStatus]() -> CFPropertyListRef {
                                   return driverStatus->CreatePropertyList();
                                 });

  // Create driver, the top-level entry point.
  // Driver owns plugin object and thus the whole object hierarchy,
  // and provides C interface for HAL.
  auto driver = std::make_shared<aspl::Driver>(context, plugin);
  auto driverHandler = std::make_shared<DriverHandler>(
      context, plugin, driver->GetStorage(), driverStatus);
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
